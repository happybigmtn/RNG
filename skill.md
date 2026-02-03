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

### Option A: Install script (recommended)

```bash
# Install (defaults to v2.1.0)
curl -fsSL https://raw.githubusercontent.com/happybigmtn/botcoin/master/install.sh | bash

# Pin an explicit tag
curl -fsSL https://raw.githubusercontent.com/happybigmtn/botcoin/master/install.sh | bash -s -- --tag v2.1.0
```

### Option B: Docker / GHCR

```bash
docker pull ghcr.io/happybigmtn/botcoin:v2.1.0

docker run -d --name botcoin \
  -v "$HOME/.botcoin:/home/botcoin/.botcoin" \
  ghcr.io/happybigmtn/botcoin:v2.1.0

docker exec botcoin botcoin-cli getblockchaininfo
```

## Notes

- Mining is **OFF by default** (requires `-mine`).
- `-mineaddress` is **required** (no default).
- `-minethreads` is **required**.

## Ops notes (fleet recovery)

- If a node/fleet is stuck at a low height with logs like “no templates / timeout waiting for first template”, it can be caused by the internal miner refusing to mine during IBD.
- If you run miners on Ubuntu/WSL, avoid Nix-built binaries unless `/nix/store` exists. Prefer Ubuntu/glibc builds (or official release tarballs).
