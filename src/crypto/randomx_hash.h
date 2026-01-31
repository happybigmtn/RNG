// Copyright (c) 2024-present The Botcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BOTCOIN_CRYPTO_RANDOMX_HASH_H
#define BOTCOIN_CRYPTO_RANDOMX_HASH_H

#include <uint256.h>

#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <span>

/**
 * RandomX proof-of-work hash computation for Botcoin.
 *
 * RandomX is a CPU-optimized, ASIC-resistant PoW algorithm that uses:
 * - 2080 MiB dataset for fast mode (mining)
 * - 256 MiB cache for light mode (validation)
 *
 * Botcoin uses a custom ARGON_SALT ("BotcoinX\x01") to differentiate
 * from Monero and prevent hashpower rental attacks.
 *
 * Seed hash rotation:
 * - Epoch: 2048 blocks (~34 hours at 60s blocks)
 * - Lag: 64 blocks (~1 hour) for pre-computation
 * - Key changes at: block_height % 2048 == 64
 * - Seed block: floor((block_height - 64 - 1) / 2048) * 2048
 */

// Forward declarations from randomx.h
struct randomx_cache;
struct randomx_vm;
struct randomx_dataset;

/**
 * RandomX context manager - handles VM, cache, and dataset lifecycle.
 * Thread-safe singleton pattern for efficient resource management.
 */
class RandomXContext {
public:
    ~RandomXContext();

    // Singleton access
    static RandomXContext& GetInstance();

    /**
     * Compute RandomX hash of input data using the current seed.
     * Uses light mode (256 MiB) for validation efficiency.
     *
     * @param input     Data to hash (typically 80-byte block header)
     * @param seed_hash The seed hash for this block's epoch
     * @return          256-bit RandomX hash
     */
    uint256 Hash(std::span<const unsigned char> input, const uint256& seed_hash);

    /**
     * Compute RandomX hash in fast mode with full dataset.
     * Requires ~2080 MiB RAM. Use for mining operations.
     *
     * @param input     Data to hash (typically 80-byte block header)
     * @param seed_hash The seed hash for this block's epoch
     * @return          256-bit RandomX hash
     */
    uint256 HashFast(std::span<const unsigned char> input, const uint256& seed_hash);

    /**
     * Check if RandomX is properly initialized.
     */
    bool IsInitialized() const;

    /**
     * Initialize for a new seed hash. Call when seed epoch changes.
     * Light mode is initialized automatically; fast mode requires explicit call.
     *
     * @param seed_hash New seed hash for the epoch
     * @param fast_mode If true, also initialize the full dataset (~2 GiB)
     */
    void UpdateSeedHash(const uint256& seed_hash, bool fast_mode = false);

    /**
     * Get current seed hash.
     */
    std::optional<uint256> GetCurrentSeedHash() const;

    // Disable copy
    RandomXContext(const RandomXContext&) = delete;
    RandomXContext& operator=(const RandomXContext&) = delete;

private:
    RandomXContext();

    void InitLight(const uint256& seed_hash);
    void InitFast(const uint256& seed_hash);
    void Cleanup();

    mutable std::mutex m_mutex;
    randomx_cache* m_cache{nullptr};
    randomx_vm* m_vm_light{nullptr};
    randomx_vm* m_vm_fast{nullptr};
    randomx_dataset* m_dataset{nullptr};
    std::optional<uint256> m_current_seed_hash;
    bool m_fast_mode_initialized{false};
};

/**
 * Compute RandomX PoW hash for a block header.
 * This is the main entry point for PoW validation.
 *
 * @param header_data Serialized block header (80 bytes)
 * @param seed_hash   The seed hash for this block's epoch
 * @return            256-bit RandomX hash
 */
uint256 RandomXHash(std::span<const unsigned char> header_data, const uint256& seed_hash);

/**
 * Compute RandomX hash using light mode (validation).
 * Uses 256 MiB cache - slower but memory efficient.
 */
uint256 RandomXHashLight(std::span<const unsigned char> data, const uint256& seed_hash);

/**
 * Calculate the seed height for a given block height.
 * Seed rotates every 2048 blocks with a 64-block lag.
 *
 * @param block_height Current block height
 * @return             Height of the block whose hash is used as seed
 */
uint64_t GetRandomXSeedHeight(uint64_t block_height);

/**
 * The epoch length for seed hash rotation (blocks).
 */
constexpr uint64_t RANDOMX_EPOCH_LENGTH = 2048;

/**
 * The lag before a new seed becomes active (blocks).
 */
constexpr uint64_t RANDOMX_EPOCH_LAG = 64;

#endif // BOTCOIN_CRYPTO_RANDOMX_HASH_H
