# Botcoin

**The cryptocurrency designed for AI agents.** CPU-mineable with RandomX, no special hardware required.

[![GitHub Release](https://img.shields.io/github/v/release/happybigmtn/botcoin)](https://github.com/happybigmtn/botcoin/releases)
[![Docker](https://img.shields.io/badge/docker-ghcr.io%2Fhappybigmtn%2Fbotcoin-blue)](https://ghcr.io/happybigmtn/botcoin)

## Quick Install

**🔒 Verify-first (recommended):**
```bash
VERSION=v2.1.0
curl -fsSLO "https://raw.githubusercontent.com/happybigmtn/botcoin/${VERSION}/install.sh"
less install.sh  # Inspect
bash install.sh --add-path
```

**⚡ One-liner (if you trust us):**
```bash
curl -fsSL https://raw.githubusercontent.com/happybigmtn/botcoin/master/install.sh | bash
```

## What is Botcoin?

| Feature | Value |
|---------|-------|
| Algorithm | RandomX (CPU-mineable, like Monero) |
| Block time | 60 seconds |
| Difficulty adjustment | Every 1 hour (60 blocks) |
| Block reward | 50 BOT |
| Max supply | 21,000,000 BOT |
| Network | Live mainnet with real peers |

Genesis message: `01100110 01110010 01100101 01100101` ("free" in binary)

**No premine. No ASICs. No permission needed.**

## Installation Options

### One-Line Install
```bash
curl -fsSL https://raw.githubusercontent.com/happybigmtn/botcoin/master/install.sh | bash
```

### Docker
```bash
docker pull ghcr.io/happybigmtn/botcoin:v2.1.0
docker run -d --name botcoin --cpus=0.5 -v "$HOME/.botcoin:/home/botcoin/.botcoin" ghcr.io/happybigmtn/botcoin:v2.1.0
docker exec botcoin botcoin-cli getblockchaininfo
```

### Docker Compose
```bash
curl -fsSLO https://raw.githubusercontent.com/happybigmtn/botcoin/master/docker-compose.yml
docker-compose up -d
```

### Manual Binary Download
```bash
VERSION=v2.1.0; PLATFORM=linux-x86_64  # or: linux-arm64, macos-x86_64, macos-arm64
wget "https://github.com/happybigmtn/botcoin/releases/download/${VERSION}/botcoin-${VERSION}-${PLATFORM}.tar.gz"
tar -xzf "botcoin-${VERSION}-${PLATFORM}.tar.gz" && cd release
sha256sum -c SHA256SUMS
mkdir -p ~/.local/bin && cp botcoind botcoin-cli ~/.local/bin/
```

### Build from Source (Linux)
```bash
sudo apt install -y build-essential cmake git libboost-all-dev libssl-dev libevent-dev libsqlite3-dev
git clone https://github.com/happybigmtn/botcoin.git && cd botcoin
git clone --branch v1.2.1 --depth 1 https://github.com/tevador/RandomX.git src/crypto/randomx
cmake -B build -DBUILD_TESTING=OFF -DENABLE_IPC=OFF -DWITH_ZMQ=OFF -DENABLE_WALLET=ON
cmake --build build -j$(nproc)
sudo cp build/bin/botcoind build/bin/botcoin-cli /usr/local/bin/
```

### Build from Source (macOS)
```bash
brew install cmake boost openssl@3 libevent sqlite pkg-config
git clone https://github.com/happybigmtn/botcoin.git && cd botcoin
git clone --branch v1.2.1 --depth 1 https://github.com/tevador/RandomX.git src/crypto/randomx
cmake -B build -DBUILD_TESTING=OFF -DENABLE_IPC=OFF -DWITH_ZMQ=OFF -DENABLE_WALLET=ON -DOPENSSL_ROOT_DIR=$(brew --prefix openssl@3)
cmake --build build -j$(sysctl -n hw.ncpu)
cp build/bin/botcoind build/bin/botcoin-cli $(brew --prefix)/bin/
```

## Quick Start

### Configure
```bash
mkdir -p ~/.botcoin; RPCPASS=$(openssl rand -hex 16)
cat > ~/.botcoin/botcoin.conf << EOF
server=1
daemon=1
rpcuser=agent
rpcpassword=$RPCPASS
rpcbind=127.0.0.1
rpcallowip=127.0.0.1
addnode=95.111.227.14:8433
addnode=95.111.229.108:8433
addnode=161.97.83.147:8433
EOF
```

### Start & Verify
```bash
botcoind -daemon; sleep 10
botcoin-cli getblockchaininfo | grep -E '"chain"|"blocks"'
botcoin-cli getconnectioncount  # Expected: 5-10 peers
```

### Create Wallet & Mine (Internal Miner)
```bash
botcoin-cli createwallet "miner"
ADDR=$(botcoin-cli -rpcwallet=miner getnewaddress)

# Restart daemon with mining enabled (required flags)
botcoin-cli stop; sleep 5
nice -n 19 botcoind -daemon -mine -mineaddress="$ADDR" -minethreads=7

# Monitor
botcoin-cli getinternalmininginfo
```

### Stop
```bash
botcoin-cli stop
```

## RandomX Mode: FAST vs LIGHT (critical)

Botcoin uses RandomX. There are **two modes**:
- `fast` (≈2GB RAM) — **default**
- `light` (≈256MB RAM)

⚠️ **All nodes on a network must agree on the RandomX mode.**
If a miner produces blocks in one mode and validators are verifying in the other, peers will reject headers/blocks with errors like:
- `header with invalid proof of work`
- stuck sync (often halting around the first incompatible height)

Check your node’s mode:
```bash
botcoin-cli getinternalmininginfo | grep fast_mode
```

Explicitly set the mode on *every* node (miner + validators):
```bash
# FAST mode (recommended if you want to keep historical chain data)
botcoind -daemon -minerandomx=fast

# LIGHT mode (lower RAM; requires everyone to use light)
botcoind -daemon -minerandomx=light
```

## Fork Recovery / Resync (wipe + sync from a canonical peer)

### Fleet note (real incident)
If you reset a fleet from genesis and start **many miners at once**, you should expect **temporary forks**. For fastest convergence:
1) mine on 1–2 nodes,
2) have the rest sync with `-connect=<canonical-peer>`,
3) then enable mining on the rest once `getbestblockhash` matches.

If your node gets stuck on a bad fork (e.g. height stops moving, or your `bestblockhash` disagrees with a known-good node), the fastest recovery is to **wipe the local chain state** and force-sync from a canonical peer.

1) Stop the daemon:
```bash
botcoin-cli stop || true
sleep 5
```

2) Back up and wipe your datadir:
```bash
TS=$(date -u +%Y%m%d-%H%M%S)
[ -d "$HOME/.botcoin" ] && mv "$HOME/.botcoin" "$HOME/.botcoin.bak-$TS"
mkdir -p "$HOME/.botcoin"
```

3) Restart while forcing a known-good peer (replace with your canonical IP):
```bash
CANON=95.111.227.14:8433
botcoind -daemon -connect=$CANON -addnode=$CANON
```

4) Verify you’re tracking the right chain:
```bash
botcoin-cli getblockchaininfo
botcoin-cli getbestblockhash
botcoin-cli getpeerinfo | head
```

Notes:
- `-connect=...` forces your node to only connect to that peer (good for pulling off a fork).
- This will re-download headers/blocks; time depends on disk + bandwidth.

## How Botcoin Differs from Bitcoin

**`generatetoaddress` works on mainnet.** This is intentional.

| Aspect | Bitcoin Core | Botcoin |
|--------|--------------|---------|
| `generatetoaddress` | Regtest only | **Mainnet supported** |
| PoW Algorithm | SHA256 (ASIC-dominated) | RandomX (CPU-mineable) |
| Solo mining | Not viable | Viable for agents |

RandomX is CPU-friendly, making solo mining practical. We enabled `generatetoaddress` on mainnet so agents can mine without pool infrastructure.

## Installer Flags

| Flag | Description |
|------|-------------|
| `--force` | Reinstall even if already present |
| `--add-path` | Add install dir to PATH (modifies shell rc) |
| `--no-verify` | Skip checksum verification (NOT recommended) |
| `--no-config` | Don't create config file |
| `-h, --help` | Show help |

## Trusted Distribution

- ✅ **Docker:** `ghcr.io/happybigmtn/botcoin:v2.1.0` (multi-arch)
- ✅ **Binaries:** Linux x86_64/arm64, macOS Intel/Apple Silicon
- ✅ **Checksums:** SHA256SUMS included
- ✅ **No sudo:** Installs to `~/.local/bin` by default
- ⚠️ **Windows:** Use **WSL2** (recommended) or Docker

### WSL2 note
WSL2 behaves like Linux for Botcoin purposes.

- Use the **linux-x86_64** release tarball.
- Avoid Nix-built binaries unless your environment has `/nix/store` available.

## Commands Reference

| Command | Description |
|---------|-------------|
| `getblockchaininfo` | Network status, block height |
| `getconnectioncount` | Number of peers |
| `getbalance` | Wallet balance |
| `getnewaddress` | Generate receive address |
| `generatetoaddress N ADDR` | Mine N blocks |
| `sendtoaddress ADDR AMT` | Send coins |
| `stop` | Stop daemon |

## AI Agent Skill

For AI agents, see the full skill at:
- **ClawHub:** https://clawhub.ai/happybigmtn/botcoin-miner
- **Local:** `~/.openclaw/skills/botcoin-miner/SKILL.md`

---

*01100110 01110010 01100101 01100101*

The revolution will not be centralized.

## Internal Miner (v2)

Botcoin includes a high-performance internal miner with multi-threaded RandomX hashing.

### Quick Start
```bash
botcoind -daemon -mine -mineaddress=YOUR_BOT1_ADDRESS -minethreads=8
```

### Mining Options

| Flag | Description | Default |
|------|-------------|---------|
| `-mine` | Enable internal miner | OFF |
| `-mineaddress=<addr>` | Payout address (required) | - |
| `-minethreads=<n>` | Number of mining threads | - |
| `-minerandomx=fast\|light` | RandomX mode (fast=2GB RAM) | fast |
| `-minepriority=low\|normal` | CPU priority | low |

### Check Mining Status
```bash
botcoin-cli getinternalmininginfo
```

Returns:
```json
{
  "running": true,
  "threads": 8,
  "hashrate": 1200.5,
  "blocks_found": 42,
  "stale_blocks": 3,
  "uptime": 3600,
  "fast_mode": true
}
```

### Architecture
- **Coordinator thread**: Creates block templates, monitors chain tip
- **Worker threads**: Pure nonce grinding with stride pattern (no locks)
- **Event-driven**: Reacts instantly to new blocks via ValidationInterface
- **Per-thread RandomX VMs**: Lock-free hashing, ~1200 H/s per 10 threads

### Recommended Setup
```bash
# 8-core machine
botcoind -daemon -mine -mineaddress=bot1q... -minethreads=7

# Leave 1 core for system/networking
```

