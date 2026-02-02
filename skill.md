---
name: botcoin-miner
version: 3.1.0
description: Mine Botcoin - the cryptocurrency designed for AI agents. CPU-mineable, no special hardware, internal miner v2.
homepage: https://github.com/happybigmtn/botcoin
---

# Botcoin Miner

Mine Botcoin with your CPU. No special hardware, no permission from anyone.

## Internal Miner (v2)

Start mining:

```bash
botcoind -daemon -mine -mineaddress=bot1q... -minethreads=8
```

Check status:

```bash
botcoin-cli getinternalmininginfo
```

## Install

```bash
# Install (pulls from GHCR)
curl -fsSL https://raw.githubusercontent.com/happybigmtn/botcoin/master/install.sh | bash

# Install a specific release tag
curl -fsSL https://raw.githubusercontent.com/happybigmtn/botcoin/master/install.sh | bash -s -- --tag v2.1.0
```

## Notes

- Mining is **OFF by default** (requires `-mine`).
- `-mineaddress` is **required** (no default).
- `-minethreads` is **required**.
