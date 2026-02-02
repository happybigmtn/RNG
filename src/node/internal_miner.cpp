// Copyright (c) 2024-present The Botcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <node/internal_miner.h>

#include <chain.h>
#include <chainparams.h>
#include <consensus/merkle.h>
#include <crypto/randomx_hash.h>
#include <interfaces/mining.h>
#include <logging.h>
#include <pow.h>
#include <primitives/block.h>
#include <shutdown.h>
#include <util/time.h>
#include <validation.h>

#ifdef HAVE_SCHED_SETSCHEDULER
#include <sched.h>
#endif
#include <sys/resource.h>

namespace node {

InternalMiner::InternalMiner(ChainstateManager& chainman, interfaces::Mining& mining)
    : m_chainman(chainman), m_mining(mining)
{
    LogPrintf("InternalMiner: Initialized (not started)\n");
}

InternalMiner::~InternalMiner()
{
    Stop();
}

bool InternalMiner::Start(int num_threads, 
                          const CScript& coinbase_script,
                          bool fast_mode,
                          bool low_priority)
{
    // Validate parameters
    if (num_threads <= 0) {
        LogPrintf("InternalMiner: ERROR - num_threads must be > 0\n");
        return false;
    }
    
    if (coinbase_script.empty()) {
        LogPrintf("InternalMiner: ERROR - coinbase_script is empty\n");
        return false;
    }
    
    // Prevent double-start
    bool expected = false;
    if (!m_running.compare_exchange_strong(expected, true)) {
        LogPrintf("InternalMiner: Already running\n");
        return false;
    }
    
    // Store configuration
    m_coinbase_script = coinbase_script;
    m_num_threads = num_threads;
    m_fast_mode = fast_mode;
    m_low_priority = low_priority;
    
    // Reset statistics
    m_hash_count.store(0, std::memory_order_relaxed);
    m_blocks_found.store(0, std::memory_order_relaxed);
    m_start_time.store(GetTime(), std::memory_order_relaxed);
    
    // Initialize tip tracking
    {
        LOCK(cs_main);
        m_last_tip = m_chainman.ActiveChain().Tip()->GetBlockHash();
    }
    m_last_template_time = GetTime();
    
    // Log startup with full configuration (loud, per Codex recommendation)
    LogPrintf("╔══════════════════════════════════════════════════════════════╗\n");
    LogPrintf("║          INTERNAL MINER STARTING                             ║\n");
    LogPrintf("╠══════════════════════════════════════════════════════════════╣\n");
    LogPrintf("║  Threads:      %-46d ║\n", num_threads);
    LogPrintf("║  RandomX Mode: %-46s ║\n", fast_mode ? "FAST (2GB RAM)" : "LIGHT (256MB RAM)");
    LogPrintf("║  Priority:     %-46s ║\n", low_priority ? "LOW (nice 19)" : "NORMAL");
    LogPrintf("║  Script Size:  %-46zu ║\n", coinbase_script.size());
    LogPrintf("╚══════════════════════════════════════════════════════════════╝\n");
    
    // Initialize RandomX in appropriate mode
    if (fast_mode) {
        LogPrintf("InternalMiner: Initializing RandomX fast mode (this may take a moment)...\n");
        // RandomXContext will initialize dataset on first HashFast() call
    }
    
    // Launch mining threads
    m_threads.reserve(num_threads);
    for (int i = 0; i < num_threads; ++i) {
        m_threads.emplace_back(&InternalMiner::MinerThread, this, i);
    }
    
    LogPrintf("InternalMiner: Started %d mining threads\n", num_threads);
    return true;
}

void InternalMiner::Stop()
{
    // Signal threads to stop
    bool expected = true;
    if (!m_running.compare_exchange_strong(expected, false)) {
        return; // Already stopped
    }
    
    LogPrintf("InternalMiner: Stopping...\n");
    
    // Wait for all threads to finish
    for (auto& thread : m_threads) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    m_threads.clear();
    
    // Log final statistics
    int64_t elapsed = GetTime() - m_start_time.load(std::memory_order_relaxed);
    uint64_t hashes = m_hash_count.load(std::memory_order_relaxed);
    uint64_t blocks = m_blocks_found.load(std::memory_order_relaxed);
    
    LogPrintf("╔══════════════════════════════════════════════════════════════╗\n");
    LogPrintf("║          INTERNAL MINER STOPPED                              ║\n");
    LogPrintf("╠══════════════════════════════════════════════════════════════╣\n");
    LogPrintf("║  Runtime:      %-43ld s ║\n", elapsed);
    LogPrintf("║  Total Hashes: %-46lu ║\n", hashes);
    LogPrintf("║  Blocks Found: %-46lu ║\n", blocks);
    if (elapsed > 0) {
        LogPrintf("║  Avg Hashrate: %-42.2f H/s ║\n", static_cast<double>(hashes) / elapsed);
    }
    LogPrintf("╚══════════════════════════════════════════════════════════════╝\n");
}

double InternalMiner::GetHashRate() const
{
    int64_t elapsed = GetTime() - m_start_time.load(std::memory_order_relaxed);
    if (elapsed <= 0) return 0.0;
    return static_cast<double>(m_hash_count.load(std::memory_order_relaxed)) / elapsed;
}

bool InternalMiner::ShouldRefreshTemplate(const uint256& current_tip) const
{
    std::lock_guard<std::mutex> lock(m_tip_mutex);
    
    // Refresh if tip changed
    if (current_tip != m_last_tip) {
        return true;
    }
    
    // Refresh if template is stale
    if (GetTime() - m_last_template_time >= TEMPLATE_REFRESH_INTERVAL_SECS) {
        return true;
    }
    
    return false;
}

void InternalMiner::MinerThread(int thread_id)
{
    LogPrintf("InternalMiner: Thread %d started\n", thread_id);
    
    // NOTE: For low CPU priority, run botcoind with: nice -n 19 botcoind ...
    // setpriority(PRIO_PROCESS, ...) affects the whole process, not just this thread,
    // which would harm networking/validation. Use external nice instead.
    (void)m_low_priority; // Config stored but applied externally
    
    // Calculate nonce range for this thread (non-overlapping)
    // Each thread gets an equal slice of the nonce space
    const uint64_t nonce_range_size = static_cast<uint64_t>(UINT32_MAX) / m_num_threads;
    const uint32_t nonce_start = static_cast<uint32_t>(thread_id * nonce_range_size);
    const uint32_t nonce_end = (thread_id == m_num_threads - 1) 
                               ? UINT32_MAX 
                               : static_cast<uint32_t>((thread_id + 1) * nonce_range_size - 1);
    
    LogPrintf("InternalMiner: Thread %d nonce range: [%u, %u]\n", 
              thread_id, nonce_start, nonce_end);
    
    // Local hash counter (batched updates to reduce atomic contention)
    uint64_t local_hashes = 0;
    constexpr uint64_t HASH_BATCH_SIZE = 1000;
    
    while (m_running.load(std::memory_order_relaxed) && !m_chainman.m_interrupt) {
        // Get current chain tip
        uint256 current_tip;
        const CBlockIndex* tip_index;
        {
            LOCK(cs_main);
            tip_index = m_chainman.ActiveChain().Tip();
            if (!tip_index) {
                // Chain not ready, wait and retry
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;
            }
            current_tip = tip_index->GetBlockHash();
        }
        
        // Check if we're in IBD (Initial Block Download) - don't mine during sync
        if (m_chainman.IsInitialBlockDownload()) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            continue;
        }
        
        // Create block template
        auto block_template = m_mining.createNewBlock({
            .coinbase_output_script = m_coinbase_script
        });
        
        if (!block_template) {
            LogPrintf("InternalMiner: Thread %d failed to create block template\n", thread_id);
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            continue;
        }
        
        // Update tip tracking
        {
            std::lock_guard<std::mutex> lock(m_tip_mutex);
            m_last_tip = current_tip;
            m_last_template_time = GetTime();
        }
        
        // Get block and prepare for mining
        CBlock block = block_template->getBlock();
        block.hashMerkleRoot = BlockMerkleRoot(block);
        block.nNonce = nonce_start;
        
        // Get RandomX seed hash
        uint256 seed_hash;
        {
            LOCK(cs_main);
            seed_hash = GetRandomXSeedHash(tip_index);
        }
        
        // Mining loop for this template
        while (m_running.load(std::memory_order_relaxed) && 
               !m_chainman.m_interrupt &&
               block.nNonce <= nonce_end) {
            
            // Compute RandomX hash using the proper serialization helper
            // GetBlockPoWHash correctly serializes CBlockHeader to 80 bytes
            uint256 pow_hash = GetBlockPoWHash(block.GetBlockHeader(), seed_hash);
            
            ++local_hashes;
            
            // Check for tip change / staleness every 10,000 hashes (not every hash!)
            // This avoids expensive cs_main locking on every nonce attempt
            if (local_hashes % 10000 == 0) {
                // Check for tip change
                uint256 check_tip;
                {
                    LOCK(cs_main);
                    const CBlockIndex* current = m_chainman.ActiveChain().Tip();
                    if (current) check_tip = current->GetBlockHash();
                }
                if (check_tip != current_tip) {
                    // Tip changed, get new template
                    break;
                }
                
                // Check template staleness
                if (ShouldRefreshTemplate(current_tip)) {
                    break;
                }
            }
            
            // Batch update global counter
            if (local_hashes >= HASH_BATCH_SIZE) {
                m_hash_count.fetch_add(local_hashes, std::memory_order_relaxed);
                local_hashes = 0;
            }
            
            // Check if we found a valid block
            if (CheckProofOfWork(pow_hash, block.nBits, m_chainman.GetConsensus())) {
                // FOUND A BLOCK!
                LogPrintf("╔══════════════════════════════════════════════════════════════╗\n");
                LogPrintf("║  🎉 BLOCK FOUND BY THREAD %-35d ║\n", thread_id);
                LogPrintf("║  Hash:  %s  ║\n", block.GetHash().GetHex().c_str());
                LogPrintf("║  Nonce: %-54u ║\n", block.nNonce);
                LogPrintf("╚══════════════════════════════════════════════════════════════╝\n");
                
                auto block_ptr = std::make_shared<const CBlock>(block);
                
                bool accepted = m_chainman.ProcessNewBlock(
                    block_ptr, 
                    /*force_processing=*/true, 
                    /*min_pow_checked=*/true, 
                    /*new_block=*/nullptr
                );
                
                if (accepted) {
                    m_blocks_found.fetch_add(1, std::memory_order_relaxed);
                    LogPrintf("InternalMiner: Block accepted by network!\n");
                } else {
                    LogPrintf("InternalMiner: WARNING - Block was rejected!\n");
                }
                
                // Break to get new template
                break;
            }
            
            ++block.nNonce;
        }
        
        // Flush remaining local hashes
        if (local_hashes > 0) {
            m_hash_count.fetch_add(local_hashes, std::memory_order_relaxed);
            local_hashes = 0;
        }
    }
    
    LogPrintf("InternalMiner: Thread %d stopped\n", thread_id);
}

} // namespace node
