// Copyright (c) 2024-present The Botcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_NODE_INTERNAL_MINER_H
#define BITCOIN_NODE_INTERNAL_MINER_H

#include <atomic>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include <script/script.h>
#include <uint256.h>

class ChainstateManager;
namespace interfaces { class Mining; }

namespace node {

/**
 * Internal multi-threaded miner for Botcoin.
 * 
 * Bypasses RPC to mine directly using ProcessNewBlock(), eliminating
 * the RPC queue bottleneck that limits generatetoaddress throughput.
 * 
 * Safety guarantees:
 * - Mining is OFF by default (requires explicit -mine flag)
 * - Requires -mineaddress (no default, prevents accidental mining)
 * - Requires -minethreads (explicit thread count)
 * - Clean shutdown with thread joining
 * - Thread-safe statistics via atomics
 * 
 * Usage:
 *   botcoind -mine -mineaddress=bot1q... -minethreads=8
 */
class InternalMiner {
public:
    /**
     * Construct internal miner.
     * Does NOT start mining - call Start() explicitly.
     */
    InternalMiner(ChainstateManager& chainman, interfaces::Mining& mining);
    
    /**
     * Destructor ensures clean shutdown.
     * Calls Stop() if still running.
     */
    ~InternalMiner();

    // Disable copy/move
    InternalMiner(const InternalMiner&) = delete;
    InternalMiner& operator=(const InternalMiner&) = delete;
    InternalMiner(InternalMiner&&) = delete;
    InternalMiner& operator=(InternalMiner&&) = delete;

    /**
     * Start mining with specified configuration.
     * 
     * @param num_threads   Number of mining threads (must be > 0)
     * @param coinbase_script  Script for coinbase output (validated address)
     * @param fast_mode     Use RandomX fast mode (2GB RAM) vs light (256MB)
     * @param low_priority  Run threads at nice 19 (low CPU priority)
     * @return true if started successfully
     */
    bool Start(int num_threads, 
               const CScript& coinbase_script,
               bool fast_mode = true,
               bool low_priority = true);
    
    /**
     * Stop all mining threads.
     * Blocks until all threads have joined.
     * Safe to call multiple times.
     */
    void Stop();
    
    /**
     * Check if miner is currently running.
     */
    bool IsRunning() const { return m_running.load(std::memory_order_relaxed); }
    
    /**
     * Get total hashes computed across all threads.
     */
    uint64_t GetHashCount() const { return m_hash_count.load(std::memory_order_relaxed); }
    
    /**
     * Get number of blocks successfully mined.
     */
    uint64_t GetBlocksFound() const { return m_blocks_found.load(std::memory_order_relaxed); }
    
    /**
     * Get current hashrate estimate (hashes per second).
     */
    double GetHashRate() const;

    /**
     * Get number of active mining threads.
     */
    int GetThreadCount() const { return m_num_threads; }

private:
    /**
     * Main mining loop for each thread.
     * @param thread_id  Unique thread identifier (0 to num_threads-1)
     */
    void MinerThread(int thread_id);
    
    /**
     * Check if we should refresh the block template.
     * Returns true if tip changed or template is stale.
     */
    bool ShouldRefreshTemplate(const uint256& current_tip) const;

    // References to node components (must outlive miner)
    ChainstateManager& m_chainman;
    interfaces::Mining& m_mining;
    
    // Mining configuration (set at Start(), immutable during mining)
    CScript m_coinbase_script;
    int m_num_threads{0};
    bool m_fast_mode{true};
    bool m_low_priority{true};
    
    // Thread management
    std::atomic<bool> m_running{false};
    std::vector<std::thread> m_threads;
    
    // Statistics (thread-safe)
    std::atomic<uint64_t> m_hash_count{0};
    std::atomic<uint64_t> m_blocks_found{0};
    std::atomic<int64_t> m_start_time{0};  // Unix timestamp
    
    // Template refresh tracking
    mutable std::mutex m_tip_mutex;
    uint256 m_last_tip;
    int64_t m_last_template_time{0};
    
    // Constants
    static constexpr int64_t TEMPLATE_REFRESH_INTERVAL_SECS = 30;
};

} // namespace node

#endif // BITCOIN_NODE_INTERNAL_MINER_H
