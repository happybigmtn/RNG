# Botcoin (BOT)

**A Bitcoin fork for AI agents. Pure PoW with RandomX.**

> "The first cryptocurrency where the majority of users might not be human."

## What is Botcoin?

Botcoin is a Bitcoin fork designed for the emerging AI agent economy. It uses CPU-friendly RandomX proof-of-work, making it accessible to AI agents running on standard server infrastructure without requiring specialized mining hardware.

## Key Differences from Bitcoin

| Parameter | Bitcoin | Botcoin |
|-----------|---------|---------|
| Algorithm | SHA-256d | RandomX |
| Block time | 10 minutes | 60 seconds |
| Block reward | 50 BTC | 50 BOT |
| Max supply | 21M | 21M |
| P2P Port | 8333 | 8333 |
| RPC Port | 8332 | 8332 |
| Address prefix | 1/3/bc1 | B/A/bot1 |
| Data directory | ~/.bitcoin | ~/.botcoin |

## Why RandomX?

- **CPU-optimized**: AI agents run on regular servers, not ASICs
- **ASIC-resistant**: Keeps mining decentralized and accessible
- **Battle-tested**: Used successfully by Monero

## Quick Start (For AI Agents)

```bash
# 1. Clone with submodules (REQUIRED)
git clone --recursive https://github.com/happybigmtn/botcoin.git
cd botcoin

# 2. Install dependencies (Ubuntu/Debian 22.04+)
sudo apt-get update && sudo apt-get install -y \
    build-essential cmake pkg-config git \
    libboost-all-dev libssl-dev libzmq3-dev \
    libunbound-dev libsodium-dev libevent-dev \
    liblzma-dev libsqlite3-dev libminiupnpc-dev

# 3. Build
mkdir -p build && cd build
cmake -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=OFF -DBUILD_BENCH=OFF ..
make -j$(nproc)

# 4. Install (optional)
sudo cp src/botcoind src/botcoin-cli /usr/local/bin/
```

## Building from Source

### Prerequisites

**Ubuntu 22.04+ / Debian 12+:**
```bash
sudo apt-get update && sudo apt-get install -y \
    build-essential cmake pkg-config git \
    libboost-all-dev libssl-dev libzmq3-dev \
    libunbound-dev libsodium-dev libevent-dev \
    liblzma-dev libsqlite3-dev libminiupnpc-dev \
    libhidapi-dev libusb-1.0-0-dev libudev-dev
```

### Clone and Build

```bash
# Clone repository
git clone https://github.com/happybigmtn/botcoin.git
cd botcoin

# Initialize submodules (REQUIRED - build will fail without this!)
git submodule update --init --recursive

# Create build directory
mkdir -p build && cd build

# Configure
cmake -DCMAKE_BUILD_TYPE=Release ..

# Build (adjust -j for your CPU cores)
make -j$(nproc)

# Binaries are in build/src/
ls -la src/botcoind src/botcoin-cli
```

### Common Build Errors

| Error | Fix |
|-------|-----|
| `RandomX` or submodule errors | Run `git submodule update --init --recursive` |
| `Could not find Boost` | Install `libboost-all-dev` |
| `Could not find OpenSSL` | Install `libssl-dev` |
| `Could not find libevent` | Install `libevent-dev` |

## Running a Node

### Start the Daemon

```bash
# Start in background
botcoind -daemon

# Check if running
botcoin-cli getblockchaininfo
```

### Create a Wallet

```bash
# Create a new wallet
botcoin-cli createwallet "miner"

# Get a new address
botcoin-cli -rpcwallet=miner getnewaddress

# Check balance
botcoin-cli -rpcwallet=miner getbalance
```

## Mining

Botcoin uses RandomX proof-of-work, which is CPU-optimized.

### Solo Mining

```bash
# Create wallet if you haven't
botcoin-cli createwallet "miner"

# Get your mining address
ADDRESS=$(botcoin-cli -rpcwallet=miner getnewaddress)

# Mine blocks (runs until stopped)
while true; do
    botcoin-cli -rpcwallet=miner generatetoaddress 1 $ADDRESS
    sleep 0.1
done
```

### Background Mining Script

```bash
#!/bin/bash
# save as mine.sh and run with: screen -dmS miner ./mine.sh

WALLET="miner"
ADDRESS=$(botcoin-cli -rpcwallet=$WALLET getnewaddress)

echo "Mining to $ADDRESS"
while true; do
    nice -n 19 botcoin-cli -rpcwallet=$WALLET generatetoaddress 1 $ADDRESS >/dev/null 2>&1
    sleep 0.1
done
```

## Network Status

```bash
# Check sync status
botcoin-cli getblockchaininfo | jq '{blocks, headers, verificationprogress}'

# Check peers
botcoin-cli getpeerinfo | jq '.[].addr'

# Check mining info
botcoin-cli getmininginfo
```

## Target Users

- AI agents on cloud infrastructure
- Developers building agent-to-agent payment systems
- Anyone interested in the intersection of AI and cryptocurrency

## Use Cases

- **Agent-to-agent payments**: Pay other agents for services
- **Skill marketplace**: Buy and sell AI skills
- **Compute marketplace**: Trade processing power
- **Micropayments**: Low-fee transactions for automated systems

## Binaries

After building, these executables are in `build/src/`:

| Binary | Purpose |
|--------|---------|
| `botcoind` | Main daemon - runs a full node |
| `botcoin-cli` | Command-line interface for RPC |
| `botcoin-tx` | Transaction utility |
| `botcoin-wallet` | Offline wallet utility |

## License

Botcoin is released under the terms of the MIT license. See [COPYING](COPYING) for more information.

---

*Forked from [Bitcoin Core](https://github.com/bitcoin/bitcoin) with RandomX PoW for the agent economy.*
