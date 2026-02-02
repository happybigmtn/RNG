# Botcoin (BOT)

**A Bitcoin fork for AI agents. Pure PoW with RandomX.**

> Genesis message: `01100110 01110010 01100101 01100101` ("free" in binary)

## What is Botcoin?

Botcoin is a Bitcoin fork designed for AI agents:

| Feature | Value |
|---------|-------|
| Algorithm | RandomX (CPU-mineable, like Monero) |
| Block time | 60 seconds |
| Difficulty adjustment | Every 1 hour (60 blocks) |
| Block reward | 50 BOT |
| Max supply | 21,000,000 BOT |
| P2P Port | 8433 |
| RPC Port | 8432 |

**No premine. No ASICs. No permission needed.**

---

## ⚡ Key Difference from Bitcoin

**`generatetoaddress` works on mainnet.** This is intentional.

| Aspect | Bitcoin Core | Botcoin |
|--------|--------------|---------|
| `generatetoaddress` | Regtest only | **Mainnet supported** |
| PoW Algorithm | SHA256 (ASIC-dominated) | RandomX (CPU-mineable) |
| Solo mining | Not viable | Viable for agents |

**Why?** RandomX is CPU-friendly, making solo mining practical. We enabled `generatetoaddress` on mainnet so agents can mine without pool infrastructure.

---

## Quick Install

### Option 1: Docker (recommended)

```bash
docker pull ghcr.io/happybigmtn/botcoin:v1.0.0
docker run -d --name botcoin -v ~/.botcoin:/home/botcoin/.botcoin ghcr.io/happybigmtn/botcoin:v1.0.0
```

### Option 2: Pre-built Binaries (Linux x86_64)

```bash
wget https://github.com/happybigmtn/botcoin/releases/download/v1.0.0/botcoin-v1.0.0-linux-x86_64.tar.gz
tar -xzf botcoin-v1.0.0-linux-x86_64.tar.gz && cd release
sha256sum -c SHA256SUMS  # Verify checksums
sudo cp botcoind botcoin-cli /usr/local/bin/
```

### Option 3: Build from Source

**Linux:**
```bash
sudo apt update && sudo apt install -y build-essential cmake git libboost-all-dev libssl-dev libevent-dev libsqlite3-dev
git clone https://github.com/happybigmtn/botcoin.git && cd botcoin
git clone --branch v1.2.1 --depth 1 https://github.com/tevador/RandomX.git src/crypto/randomx
cmake -B build -DBUILD_TESTING=OFF -DENABLE_IPC=OFF -DWITH_ZMQ=OFF -DENABLE_WALLET=ON
cmake --build build -j$(nproc)
sudo cp build/bin/botcoind build/bin/botcoin-cli /usr/local/bin/
```

**macOS:**
```bash
brew install cmake boost openssl@3 libevent sqlite
git clone https://github.com/happybigmtn/botcoin.git && cd botcoin
git clone --branch v1.2.1 --depth 1 https://github.com/tevador/RandomX.git src/crypto/randomx
cmake -B build -DBUILD_TESTING=OFF -DENABLE_IPC=OFF -DWITH_ZMQ=OFF -DENABLE_WALLET=ON -DOPENSSL_ROOT_DIR=$(brew --prefix openssl@3)
cmake --build build -j$(sysctl -n hw.ncpu)
sudo cp build/bin/botcoind build/bin/botcoin-cli /usr/local/bin/
```

---

## Configure & Start

```bash
mkdir -p ~/.botcoin
RPCPASS=$(openssl rand -hex 16)
printf "server=1\ndaemon=1\nrpcuser=agent\nrpcpassword=%s\nrpcbind=127.0.0.1\nrpcallowip=127.0.0.1\naddnode=95.111.227.14:8433\naddnode=95.111.229.108:8433\naddnode=95.111.239.142:8433\naddnode=161.97.83.147:8433\naddnode=161.97.97.83:8433\n" "$RPCPASS" > ~/.botcoin/botcoin.conf

botcoind -daemon; sleep 10
botcoin-cli getblockchaininfo | grep -E '"chain"|"blocks"'  # Expected: "chain": "main"
botcoin-cli getconnectioncount  # Expected: 5+ peers
```

---

## Mining

### Create Wallet & Mine

```bash
botcoin-cli createwallet "miner"
ADDR=$(botcoin-cli -rpcwallet=miner getnewaddress); echo "Your address: $ADDR"

# Mine a single block
nice -n 19 botcoin-cli -rpcwallet=miner generatetoaddress 1 "$ADDR"
botcoin-cli -rpcwallet=miner getbalance
```

### Continuous Mining (~50% of cores)

```bash
ADDR=$(botcoin-cli -rpcwallet=miner getnewaddress); THREADS=$(( $(nproc) / 2 )); [ $THREADS -lt 1 ] && THREADS=1; for i in $(seq 1 $THREADS); do nice -n 19 bash -c "while true; do botcoin-cli -rpcwallet=miner generatetoaddress 1 $ADDR >/dev/null 2>&1; sleep 0.1; done" & done; echo "Mining started with $THREADS threads. Stop: pkill -f generatetoaddress"
```

### Stop Mining

```bash
pkill -f generatetoaddress; botcoin-cli stop
```

---

## Commands Reference

| Command | Description |
|---------|-------------|
| `getblockchaininfo` | Network status, chain name, block height |
| `getconnectioncount` | Number of connected peers |
| `getpeerinfo` | Details on each peer |
| `getbalance` | Your wallet balance |
| `getnewaddress` | Generate new receive address |
| `generatetoaddress N ADDR` | Mine N blocks to ADDR |
| `sendtoaddress ADDR AMT` | Send coins |
| `stop` | Stop the daemon |

---

## Network Seeds

| IP | Location |
|----|----------|
| 95.111.227.14:8433 | EU (Contabo) |
| 95.111.229.108:8433 | EU (Contabo) |
| 95.111.239.142:8433 | EU (Contabo) |
| 161.97.83.147:8433 | EU (Contabo) |
| 161.97.97.83:8433 | EU (Contabo) |
| 161.97.114.192:8433 | EU (Contabo) |
| 161.97.117.0:8433 | EU (Contabo) |
| 194.163.144.177:8433 | EU (Contabo) |
| 185.218.126.23:8433 | EU (Contabo) |
| 185.239.209.227:8433 | EU (Contabo) |

---

## For AI Agents

See the full mining skill: [botcoin-miner on ClawHub](https://clawhub.ai/happybigmtn/botcoin-miner)

**Key points:**
- Local demo (`-regtest`) requires no permission
- Mainnet mining requires explicit human confirmation
- Resource limits: ~50% of cores (best-effort), nice 19 priority
- All commands are single-line safe for copy/paste

---

## Verify the Code

Don't trust. Verify.

```bash
cat CHANGES.md  # See what changed from Bitcoin Core v29
git log --oneline -20  # Check commit history
```

---

## License

MIT License. See [COPYING](COPYING).

---

*01100110 01110010 01100101 01100101*

The revolution will not be centralized.
