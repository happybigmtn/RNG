# Implementation Plan Archive

## Review Signoff (2026-01-30) - SIGNED OFF

- [x] Set mainnet `pchMessageStart` to `{0xB0, 0x7C, 0x01, 0x0E}` in `src/kernel/chainparams.cpp`


## Review Signoff (2026-01-30) - SIGNED OFF

- [x] Set testnet `pchMessageStart` to `{0xB0, 0x7C, 0x7E, 0x57}` in `src/kernel/chainparams.cpp`

- [x] Set regtest `pchMessageStart` to `{0xB0, 0x7C, 0x00, 0x00}` in `src/kernel/chainparams.cpp`

**Required Tests:**
```python

- [x] Set mainnet `nDefaultPort` to 8433 in `src/kernel/chainparams.cpp`

- [x] Set mainnet RPC port to 8432 in `src/chainparamsbase.cpp`

- [x] Set testnet P2P to 18433 in `src/kernel/chainparams.cpp`

- [x] Set testnet RPC to 18432 in `src/chainparamsbase.cpp`

- [x] Set regtest P2P to 18544 in `src/kernel/chainparams.cpp`

- [x] Set regtest RPC to 18543 in `src/chainparamsbase.cpp`

**Required Tests:**
```bash

- [x] Set `PROTOCOL_VERSION` to 70100 in `src/node/protocol_version.h`

- [x] Set mainnet `PUBKEY_ADDRESS` = 25 (prefix 'B') in `src/kernel/chainparams.cpp`

- [x] Set mainnet `SCRIPT_ADDRESS` = 5 (prefix 'A') - already correct

- [x] Set testnet `bech32_hrp` = "tbot" in `src/kernel/chainparams.cpp`

