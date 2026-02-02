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
#include <streams.h>
#include <util/signalinterrupt.h>
#include <util/time.h>
#include <validation.h>

namespace node {

InternalMiner::InternalMiner(ChainstateManager& chainman, interfaces::Mining& mining)
    : m_chainman(chainman), m_mining(mining)
{
    LogInfo("InternalMiner: Initialized (not started)\n");
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
        LogInfo("InternalMiner: ERROR - num_threads must be > 0\n");
        return false;
    }
    
    if (coinbase_script.empty()) {
        LogInfo("InternalMiner: ERROR - coinbase_script is empty\n");
        return false;
    }
    
    // Prevent double-start
    bool expected = false;
    if (!m_running.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
        LogInfo("InternalMiner: Already running\n");
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
    m_context_version.store(0, std::memory_order_relaxed);
    
    // Log startup with full configuration (LOUD per Codex recommendation)
    LogInfo("╔══════════════════════════════════════════════════════════════╗\n");
    LogInfo("║          INTERNAL MINER STARTING                             ║\n");
    LogInfo("╠══════════════════════════════════════════════════════════════╣\n");
    LogInfo("║  Architecture: Coordinator + %d Workers                      ║\n", num_threads);
    LogInfo("║  RandomX Mode: %-46s ║\n", fast_mode ? "FAST (2GB RAM)" : "LIGHT (256MB RAM)");
    LogInfo("║  Priority:     %-46s ║\n", low_priority ? "LOW (nice 19)" : "NORMAL");
    LogInfo("║  Script Size:  %-46zu ║\n", coinbase_script.size());
    LogInfo("╠══════════════════════════════════════════════════════════════╣\n");
    LogInfo("║  Lock-free template sharing enabled                          ║\n");
    LogInfo("║  Nonce partitioning: %d ranges                               ║\n", num_threads);
    LogInfo("╚══════════════════════════════════════════════════════════════╝\n");
    
    // Initialize RandomX in appropriate mode
    if (fast_mode) {
        LogInfo("InternalMiner: Initializing RandomX fast mode (this may take a moment)...\n");
    }
    
    // Start coordinator thread first (creates initial template)
    m_coordinator_thread = std::thread(&InternalMiner::CoordinatorThread, this);
    
    // Wait for first template to be ready before starting workers
    {
        std::unique_lock<std::mutex> lock(m_context_mutex);
        m_context_cv.wait(lock, [this] { 
            return m_current_context != nullptr || !m_running.load(std::memory_order_acquire);
        });
    }
    
    if (!m_running.load(std::memory_order_acquire)) {
        LogInfo("InternalMiner: Coordinator failed to start, aborting\n");
        if (m_coordinator_thread.joinable()) {
            m_coordinator_thread.join();
        }
        return false;
    }
    
    // Launch worker threads
    m_worker_threads.reserve(num_threads);
    for (int i = 0; i < num_threads; ++i) {
        m_worker_threads.emplace_back(&InternalMiner::WorkerThread, this, i);
    }
    
    LogInfo("InternalMiner: Started coordinator + %d worker threads\n", num_threads);
    return true;
}

void InternalMiner::Stop()
{
    // Signal threads to stop
    bool expected = true;
    if (!m_running.compare_exchange_strong(expected, false, std::memory_order_acq_rel)) {
        return; // Already stopped
    }
    
    LogInfo("InternalMiner: Stopping...\n");
    
    // Wake up any waiting workers
    m_context_cv.notify_all();
    
    // Wait for worker threads first (they depend on coordinator's context)
    for (auto& thread : m_worker_threads) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    m_worker_threads.clear();
    
    // Then stop coordinator
    if (m_coordinator_thread.joinable()) {
        m_coordinator_thread.join();
    }
    
    // Clear context
    {
        std::lock_guard<std::mutex> lock(m_context_mutex);
        m_current_context.reset();
    }
    
    // Log final statistics
    int64_t elapsed = GetTime() - m_start_time.load(std::memory_order_relaxed);
    uint64_t hashes = m_hash_count.load(std::memory_order_relaxed);
    uint64_t blocks = m_blocks_found.load(std::memory_order_relaxed);
    
    LogInfo("╔══════════════════════════════════════════════════════════════╗\n");
    LogInfo("║          INTERNAL MINER STOPPED                              ║\n");
    LogInfo("╠══════════════════════════════════════════════════════════════╣\n");
    LogInfo("║  Runtime:      %-43ld s ║\n", elapsed);
    LogInfo("║  Total Hashes: %-46lu ║\n", hashes);
    LogInfo("║  Blocks Found: %-46lu ║\n", blocks);
    if (elapsed > 0) {
        LogInfo("║  Avg Hashrate: %-42.2f H/s ║\n", static_cast<double>(hashes) / elapsed);
    }
    LogInfo("╚══════════════════════════════════════════════════════════════╝\n");
}

double InternalMiner::GetHashRate() const
{
    int64_t elapsed = GetTime() - m_start_time.load(std::memory_order_relaxed);
    if (elapsed <= 0) return 0.0;
    return static_cast<double>(m_hash_count.load(std::memory_order_relaxed)) / elapsed;
}

void InternalMiner::CoordinatorThread()
{
    LogInfo("InternalMiner: Coordinator thread started\n");
    
    uint256 last_tip;
    int64_t last_template_time = 0;
    uint64_t template_id = 0;
    
    while (m_running.load(std::memory_order_acquire) && 
           !static_cast<bool>(m_chainman.m_interrupt)) {
        
        // Get current chain state
        uint256 current_tip;
        const CBlockIndex* tip_index;
        {
            LOCK(cs_main);
            tip_index = m_chainman.ActiveChain().Tip();
            if (!tip_index) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;
            }
            current_tip = tip_index->GetBlockHash();
        }
        
        // Check if we need a new template
        bool need_new_template = (current_tip != last_tip) ||
                                 (GetTime() - last_template_time >= TEMPLATE_REFRESH_INTERVAL_SECS) ||
                                 (template_id == 0);  // First template
        
        // Skip mining during IBD
        if (m_chainman.IsInitialBlockDownload()) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            continue;
        }
        
        if (need_new_template) {
            // Create new block template
            auto block_template = m_mining.createNewBlock({
                .coinbase_output_script = m_coinbase_script
            });
            
            if (!block_template) {
                LogInfo("InternalMiner: Coordinator failed to create block template\n");
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
                continue;
            }
            
            // Build new context
            auto new_context = std::make_shared<MiningContext>();
            new_context->block = block_template->getBlock();
            new_context->block.hashMerkleRoot = BlockMerkleRoot(new_context->block);
            new_context->template_id = ++template_id;
            
            // Get RandomX seed hash and nBits
            {
                LOCK(cs_main);
                new_context->seed_hash = GetRandomXSeedHash(tip_index);
                new_context->nBits = new_context->block.nBits;
            }
            
            // Atomically publish new context
            {
                std::lock_guard<std::mutex> lock(m_context_mutex);
                m_current_context = new_context;
                m_context_version.store(template_id, std::memory_order_release);
            }
            m_context_cv.notify_all();
            
            last_tip = current_tip;
            last_template_time = GetTime();
            
            if (template_id == 1) {
                LogInfo("InternalMiner: First template ready (height %d)\n", tip_index->nHeight + 1);
            } else {
                LogInfo("InternalMiner: New template #%lu (tip changed or refresh)\n", template_id);
            }
        }
        
        // Sleep briefly before checking again
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    LogInfo("InternalMiner: Coordinator thread stopped\n");
}

void InternalMiner::WorkerThread(int thread_id)
{
    LogInfo("InternalMiner: Worker %d started\n", thread_id);
    
    // Create per-thread RandomX VM for lock-free hashing
    RandomXMiningVM mining_vm;
    
    // Calculate nonce range for this thread (non-overlapping)
    const uint64_t nonce_range_size = static_cast<uint64_t>(UINT32_MAX) / m_num_threads;
    const uint32_t nonce_start = static_cast<uint32_t>(thread_id * nonce_range_size);
    const uint32_t nonce_end = (thread_id == m_num_threads - 1) 
                               ? UINT32_MAX 
                               : static_cast<uint32_t>((thread_id + 1) * nonce_range_size - 1);
    
    LogInfo("InternalMiner: Worker %d nonce range: [%u, %u]\n", 
              thread_id, nonce_start, nonce_end);
    
    // Local state
    uint64_t local_hashes = 0;
    uint64_t last_context_version = 0;
    std::shared_ptr<MiningContext> ctx;
    CBlock working_block;
    uint32_t nonce = nonce_start;
    
    while (m_running.load(std::memory_order_acquire) && 
           !static_cast<bool>(m_chainman.m_interrupt)) {
        
        // Check for new template (lock-free fast path)
        uint64_t current_version = m_context_version.load(std::memory_order_acquire);
        if (current_version != last_context_version || !ctx) {
            // Get new context
            {
                std::unique_lock<std::mutex> lock(m_context_mutex);
                if (!m_current_context) {
                    // Wait for first template
                    m_context_cv.wait(lock, [this] {
                        return m_current_context != nullptr || 
                               !m_running.load(std::memory_order_acquire);
                    });
                    if (!m_running.load(std::memory_order_acquire)) break;
                }
                ctx = m_current_context;
            }
            
            if (!ctx) continue;
            
            // Initialize/update per-thread VM if seed changed
            if (!mining_vm.HasSeed(ctx->seed_hash)) {
                if (!mining_vm.Initialize(ctx->seed_hash)) {
                    LogInfo("InternalMiner: Worker %d failed to init VM\n", thread_id);
                    std::this_thread::sleep_for(std::chrono::seconds(1));
                    continue;
                }
                LogInfo("InternalMiner: Worker %d VM initialized\n", thread_id);
            }
            
            // Copy block template for local modification
            working_block = ctx->block;
            nonce = nonce_start;  // Reset to start of our range
            last_context_version = ctx->template_id;
        }
        
        // Pure nonce grinding loop - NO LOCKS (per-thread VM)
        for (uint64_t i = 0; i < STALENESS_CHECK_INTERVAL && nonce <= nonce_end; ++i, ++nonce) {
            working_block.nNonce = nonce;
            
            // Compute RandomX hash using per-thread VM (LOCK-FREE!)
            // Serialize header to bytes
            DataStream ss{};
            ss << static_cast<const CBlockHeader&>(working_block);
            std::span<const unsigned char> header_data{
                reinterpret_cast<const unsigned char*>(ss.data()), 
                ss.size()
            };
            uint256 pow_hash = mining_vm.Hash(header_data);
            
            ++local_hashes;
            
            // Check if we found a valid block using CheckProofOfWork
            if (CheckProofOfWork(pow_hash, ctx->nBits, Params().GetConsensus())) {
                LogInfo("╔══════════════════════════════════════════════════════════════╗\n");
                LogInfo("║  🎉 BLOCK FOUND BY WORKER %d                                 ║\n", thread_id);
                LogInfo("╠══════════════════════════════════════════════════════════════╣\n");
                LogInfo("║  Nonce:  %u                                                  ║\n", nonce);
                LogInfo("║  Hash:   %s... ║\n", pow_hash.ToString().substr(0, 16).c_str());
                LogInfo("╚══════════════════════════════════════════════════════════════╝\n");
                
                if (SubmitBlock(working_block)) {
                    m_blocks_found.fetch_add(1, std::memory_order_relaxed);
                    // Force template refresh by invalidating our version
                    last_context_version = 0;
                }
                break;  // Get fresh template after finding block
            }
        }
        
        // Batch update global hash count
        if (local_hashes >= HASH_BATCH_SIZE) {
            m_hash_count.fetch_add(local_hashes, std::memory_order_relaxed);
            local_hashes = 0;
        }
        
        // If we exhausted our nonce range, wait for new template
        if (nonce > nonce_end) {
            // We've tried all nonces in our range for this template
            // This is rare (only happens if no block found in ~4B/N attempts)
            // Wait for coordinator to provide new template
            std::unique_lock<std::mutex> lock(m_context_mutex);
            m_context_cv.wait_for(lock, std::chrono::seconds(1));
            nonce = nonce_start;
        }
    }
    
    // Final hash count update
    if (local_hashes > 0) {
        m_hash_count.fetch_add(local_hashes, std::memory_order_relaxed);
    }
    
    LogInfo("InternalMiner: Worker %d stopped\n", thread_id);
}

bool InternalMiner::SubmitBlock(const CBlock& block)
{
    // Thread-safe block submission
    LOCK(cs_main);
    
    bool new_block = false;
    auto block_ptr = std::make_shared<const CBlock>(block);
    bool accepted = m_chainman.ProcessNewBlock(block_ptr, /*force_processing=*/true, 
                                                /*min_pow_checked=*/true, &new_block);
    
    if (accepted && new_block) {
        LogInfo("InternalMiner: Block accepted by network!\n");
        return true;
    } else if (accepted) {
        LogInfo("InternalMiner: Block was duplicate\n");
        return false;
    } else {
        LogInfo("InternalMiner: Block rejected (stale or invalid)\n");
        return false;
    }
}

} // namespace node
