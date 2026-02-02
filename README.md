# Botcoin

**The cryptocurrency designed for AI agents.** CPU-mineable with RandomX, no special hardware required.

[![GitHub Release](https://img.shields.io/github/v/release/happybigmtn/botcoin)](https://github.com/happybigmtn/botcoin/releases)
[![Docker](https://img.shields.io/badge/docker-ghcr.io%2Fhappybigmtn%2Fbotcoin-blue)](https://ghcr.io/happybigmtn/botcoin)

## Quick Install

```bash
curl -fsSL https://raw.githubusercontent.com/happybigmtn/botcoin/master/install.sh | bash
```

Or verify first:
```bash
VERSION=v1.0.1
curl -fsSLO "https://raw.githubusercontent.com/happybigmtn/botcoin/${VERSION}/install.sh"
less install.sh  # Inspect
bash install.sh --add-path
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
docker pull ghcr.io/happybigmtn/botcoin:v1.0.1
docker run -d --name botcoin --cpus=0.5 -v "$HOME/.botcoin:/home/botcoin/.botcoin" ghcr.io/happybigmtn/botcoin:v1.0.1
docker exec botcoin botcoin-cli getblockchaininfo
```

### Docker Compose
```bash
curl -fsSLO https://raw.githubusercontent.com/happybigmtn/botcoin/master/docker-compose.yml
docker-compose up -d
```

### Manual Binary Download
```bash
VERSION=v1.0.1; PLATFORM=linux-x86_64  # or: linux-arm64, macos-x86_64, macos-arm64
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
brew install cmake boost openssl@3 libevent sqlite
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

### Create Wallet & Mine
```bash
botcoin-cli createwallet "miner"
ADDR=$(botcoin-cli -rpcwallet=miner getnewaddress)
nice -n 19 botcoin-cli -rpcwallet=miner generatetoaddress 1 "$ADDR"
botcoin-cli -rpcwallet=miner getbalance
```

### Stop
```bash
pkill -f generatetoaddress; botcoin-cli stop
```

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

- ✅ **Docker:** `ghcr.io/happybigmtn/botcoin:v1.0.1` (multi-arch)
- ✅ **Binaries:** Linux x86_64/arm64, macOS Intel/Apple Silicon
- ✅ **Checksums:** SHA256SUMS included
- ✅ **No sudo:** Installs to `~/.local/bin` by default
- ⚠️ **Windows:** Use WSL2 or Docker

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
