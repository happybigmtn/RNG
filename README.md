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
| Port | 8333 | 8433 |
| Address prefix | 1/3/bc1 | B/A/bot1 |

## Why RandomX?

- **CPU-optimized**: AI agents run on regular servers, not ASICs
- **ASIC-resistant**: Keeps mining decentralized and accessible
- **Battle-tested**: Used successfully by Monero

## Target Users

- 🤖 AI agents on [Moltbook](https://moltbook.com) and other platforms
- 🛠️ Developers building agent-to-agent payment systems
- 💡 Anyone interested in the intersection of AI and cryptocurrency

## Use Cases

- **Agent-to-agent payments**: Pay other agents for services
- **Skill marketplace**: Buy and sell AI skills
- **Compute marketplace**: Trade processing power
- **Tipping**: Reward quality content from agents

## Building

```bash
# Dependencies (Ubuntu/Debian)
sudo apt-get install build-essential libtool autotools-dev automake pkg-config bsdmainutils python3 libssl-dev libevent-dev libboost-system-dev libboost-filesystem-dev libboost-test-dev libboost-thread-dev libminiupnpc-dev libzmq3-dev libsqlite3-dev

# Build
./autogen.sh
./configure
make -j$(nproc)
```

## Running

```bash
# Start daemon
./src/botcoind -daemon

# Create wallet
./src/botcoin-cli createwallet "agent-wallet"

# Get new address
./src/botcoin-cli getnewaddress

# Check balance
./src/botcoin-cli getbalance
```

## Mining

```bash
# Solo mining (for testing)
./src/botcoin-cli generatetoaddress 1 <your_address>

# For production mining, use xmrig with RandomX
```

## License

Botcoin is released under the terms of the MIT license. See [COPYING](COPYING) for more information.

## Links

- Website: Coming soon
- Explorer: Coming soon
- Moltbook: [m/botcoin](https://moltbook.com/m/botcoin)

---

*Forked from [Bitcoin Core](https://github.com/bitcoin/bitcoin) with ❤️ for the agent economy.*
