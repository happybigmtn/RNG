// Copyright (c) 2024-present The Botcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_NODE_INTERNAL_MINER_H
#define BITCOIN_NODE_INTERNAL_MINER_H

#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include <primitives/block.h>
#include <script/script.h>
#include <uint256.h>

class ChainstateManager;
namespace interfaces { class Mining; }

namespace node {

/**
 * Internal multi-threaded miner for Botcoin.
 * 
 * Architecture:
 * - One COORDINATOR thread: creates block templates, monitors chain tip
 * - N WORKER threads: pure nonce grinding with no locks
 * - Lock-free template sharing via atomic pointer swap
 * 
 * This design eliminates the lock contention that plagued v1 where all
 * threads competed for cs_main and createNewBlock().
 * 
 * Safety guarantees (per Codex review):
 * - Mining is OFF by default (requires explicit -mine flag)
 * - Requires -mineaddress (no default, prevents accidental mining)
 * - Requires -minethreads (explicit thread count, logged loudly)
 * - Clean shutdown with proper thread join ordering
 * - Thread-safe statistics via atomics
 * - Nonce partitioning prevents duplicate work
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
     * @param num_threads   Number of worker threads (must be > 0)
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
    bool IsRunning() const { return m_running.load(std::memory_order_acquire); }
    
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
     * Shared mining context - passed to workers via atomic pointer.
     * Immutable once published (workers read-only).
     */
    struct MiningContext {
        CBlock block;              // Block template (workers modify nNonce only)
        uint256 seed_hash;         // RandomX seed hash
        unsigned int nBits;        // Difficulty bits for CheckProofOfWork
        uint64_t template_id;      // Monotonic ID to detect staleness
        
        MiningContext() : nBits(0), template_id(0) {}
    };

    /**
     * Coordinator thread: creates templates and monitors chain.
     */
    void CoordinatorThread();
    
    /**
     * Worker thread: pure nonce grinding.
     * @param thread_id  Unique thread identifier (0 to num_threads-1)
     */
    void WorkerThread(int thread_id);
    
    /**
     * Submit a found block to the network.
     * Thread-safe, called by workers when they find a valid block.
     */
    bool SubmitBlock(const CBlock& block);

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
    std::thread m_coordinator_thread;
    std::vector<std::thread> m_worker_threads;
    
    // Shared mining context (lock-free via atomic pointer)
    // Coordinator writes new context, workers read
    std::shared_ptr<MiningContext> m_current_context;
    std::mutex m_context_mutex;  // Protects m_current_context writes
    std::condition_variable m_context_cv;  // Workers wait for new context
    std::atomic<uint64_t> m_context_version{0};  // For quick staleness check
    
    // Statistics (thread-safe, updated by workers)
    std::atomic<uint64_t> m_hash_count{0};
    std::atomic<uint64_t> m_blocks_found{0};
    std::atomic<int64_t> m_start_time{0};  // Unix timestamp
    
    // Constants
    static constexpr int64_t TEMPLATE_REFRESH_INTERVAL_SECS = 30;
    static constexpr uint64_t HASH_BATCH_SIZE = 10000;  // Update stats every N hashes
    static constexpr uint64_t STALENESS_CHECK_INTERVAL = 1000;  // Check for new template every N hashes
};

} // namespace node

#endif // BITCOIN_NODE_INTERNAL_MINER_H
