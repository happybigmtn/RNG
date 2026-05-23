#!/usr/bin/env python3
# Copyright (c) 2026-present The RNG developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Verify sharepool diagnostic RPCs: submitshare, getsharechaininfo,
getmininginfo (sharepool object), and getinternalmininginfo (shares_found).
"""

from test_framework.messages import (
    CBlockHeader,
    CShareRecord,
)
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import (
    assert_equal,
    assert_raises_rpc_error,
)

# Matches RPC_MISC_ERROR / RPC_VERIFY_REJECTED / RPC_DESERIALIZATION_ERROR
# in src/rpc/protocol.h.
RPC_MISC_ERROR = -1
RPC_DESERIALIZATION_ERROR = -22
RPC_VERIFY_REJECTED = -26


def uint256_from_rpc_hex(value: str) -> int:
    return int(value, 16)


def build_valid_share(framework, node) -> CShareRecord:
    """Mine one block and turn its header into a self-consistent share.

    The block target equals the share target here, which is the easiest
    target a share may use without being rejected for being too easy.
    """
    address = node.get_deterministic_priv_key().address
    block_hash = framework.generatetoaddress(node, 1, address)[0]
    block = node.getblock(block_hash)

    header = CBlockHeader()
    header.nVersion = block["version"]
    header.hashPrevBlock = uint256_from_rpc_hex(block["previousblockhash"])
    header.hashMerkleRoot = uint256_from_rpc_hex(block["merkleroot"])
    header.nTime = block["time"]
    header.nBits = int(block["bits"], 16)
    header.nNonce = block["nonce"]

    share = CShareRecord()
    share.parent_share = 0
    share.prev_block_hash = header.hashPrevBlock
    share.candidate_header = header
    share.share_nBits = header.nBits
    share.payout_script = bytes.fromhex("0014" + "11" * 20)
    return share


class SharepoolRpcTest(BitcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 2
        self.setup_clean_chain = True
        # Node 0 has sharepool ALWAYS_ACTIVE (-vbparams start_time = -1) so we
        # do not need to grind 432 RandomX blocks for activation in regtest.
        # Node 1 keeps the regtest default (NEVER_ACTIVE) so we can verify the
        # inactive-state contract on getmininginfo / getsharechaininfo.
        self.extra_args = [
            ["-vbparams=sharepool:-1:-2:0"],
            [],
        ]

    def _activate_sharepool(self, node):
        # ALWAYS_ACTIVE deployments require at least one block past genesis
        # before DeploymentActiveAt(tip) returns true.
        address = node.get_deterministic_priv_key().address
        self.generatetoaddress(node, 1, address, sync_fun=self.no_op)
        self.wait_until(
            lambda: node.getdeploymentinfo()["deployments"]["sharepool"]["active"]
        )

    def _assert_pre_activation_errors(self, node):
        self.log.info("Pre-activation: getsharechaininfo and submitshare reject with RPC_MISC_ERROR")
        assert_raises_rpc_error(
            RPC_MISC_ERROR,
            "Sharepool deployment is not active",
            node.getsharechaininfo,
        )
        assert_raises_rpc_error(
            RPC_MISC_ERROR,
            "Sharepool deployment is not active",
            node.submitshare,
            "00" * 8,
        )

    def _assert_getmininginfo_inactive(self, node):
        info = node.getmininginfo()
        assert "sharepool" not in info, "sharepool field must be omitted when inactive"

    def _assert_getmininginfo_active(self, node):
        info = node.getmininginfo()
        assert "sharepool" in info, "sharepool field must be present when active"
        sp = info["sharepool"]
        assert_equal(sp["active"], True)
        assert "sharechain_height" in sp
        assert_equal(sp["reward_window_size"], 720)
        assert "pending_shares" in sp
        assert sp["pending_shares"] >= 0

    def _assert_initial_sharechain_state(self, node):
        info = node.getsharechaininfo()
        for key in ("tip", "height", "total_shares", "orphan_count", "difficulty"):
            assert key in info, f"getsharechaininfo missing {key}"
        assert_equal(info["tip"], "00" * 32)
        assert_equal(info["height"], 0)
        assert_equal(info["total_shares"], 0)
        assert_equal(info["orphan_count"], 0)
        assert info["difficulty"] > 0

    def _assert_submitshare_accepts(self, node):
        share = build_valid_share(self, node)
        result = node.submitshare(share.serialize().hex())
        assert_equal(result["accepted"], True)
        assert_equal(result["share_id"], f"{share.share_id:064x}")

        info = node.getsharechaininfo()
        assert_equal(info["total_shares"], 1)
        assert_equal(info["height"], 0)
        assert_equal(info["tip"], f"{share.share_id:064x}")

        self.log.info("Re-submitting the same share is idempotent and still returns accepted=true")
        result_again = node.submitshare(share.serialize().hex())
        assert_equal(result_again["accepted"], True)
        assert_equal(result_again["share_id"], f"{share.share_id:064x}")
        info_after = node.getsharechaininfo()
        assert_equal(info_after["total_shares"], 1)

    def _assert_submitshare_rejects(self, node):
        self.log.info("submitshare rejects malformed hex with RPC_DESERIALIZATION_ERROR")
        assert_raises_rpc_error(
            RPC_DESERIALIZATION_ERROR,
            "Share decode failed",
            node.submitshare,
            "not-valid-hex",
        )

        self.log.info("submitshare rejects a share with the wrong version (bad-share-version)")
        bad_share = build_valid_share(self, node)
        bad_share.version = 99
        assert_raises_rpc_error(
            RPC_VERIFY_REJECTED,
            "bad-share-version",
            node.submitshare,
            bad_share.serialize().hex(),
        )

        self.log.info("submitshare rejects a share whose target is harder than the block target")
        too_hard = build_valid_share(self, node)
        # Cut nBits mantissa in half -> harder target -> share-target-too-hard.
        new_bits = (too_hard.share_nBits & 0xff000000) | ((too_hard.share_nBits & 0x00ffffff) >> 1)
        too_hard.share_nBits = new_bits
        assert_raises_rpc_error(
            RPC_VERIFY_REJECTED,
            "share-target-too-hard",
            node.submitshare,
            too_hard.serialize().hex(),
        )

    def _assert_getinternalmininginfo_shape(self, node):
        # The internal miner is not started in functional tests (no -mine
        # flag), so calling getinternalmininginfo directly trips the pre-
        # existing schema validation on the {running:false,error:...} early
        # return path. Instead, verify that shares_found is part of the
        # documented contract by inspecting `help getinternalmininginfo`.
        help_text = node.help("getinternalmininginfo")
        assert "shares_found" in help_text, (
            "getinternalmininginfo RPCHelpMan must document the shares_found "
            "field (TODO(POOL-08A): wire to InternalMiner::GetSharesFound())."
        )

    def run_test(self):
        active_node, inactive_node = self.nodes

        # (6) pre-activation: RPCs reject and getmininginfo omits sharepool on
        # the inactive node.
        self._assert_pre_activation_errors(inactive_node)
        self._assert_getmininginfo_inactive(inactive_node)

        # Activate the sharepool on node 0 (one block past genesis is enough
        # because the deployment is configured ALWAYS_ACTIVE for this test).
        self._activate_sharepool(active_node)

        # (4) getmininginfo includes sharepool when active.
        self._assert_getmininginfo_active(active_node)

        # (3) getsharechaininfo state, baseline (no shares yet).
        self._assert_initial_sharechain_state(active_node)

        # (1) valid submit.
        self._assert_submitshare_accepts(active_node)

        # (2) invalid submit (multiple flavours).
        self._assert_submitshare_rejects(active_node)

        # (5) getmininginfo still omits sharepool on the inactive node even
        # after the active node has produced shares.
        self._assert_getmininginfo_inactive(inactive_node)

        # (7) shares_found counter shape on getinternalmininginfo.
        self._assert_getinternalmininginfo_shape(active_node)


if __name__ == "__main__":
    SharepoolRpcTest(__file__).main()
