#!/usr/bin/env python3
# Copyright (c) 2018 The Bitcoin Unlimited developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.


from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import *


class DeltaBlocksTest(BitcoinTestFramework):
    def __init__(self):
        self.rep = False
        BitcoinTestFramework.__init__(self)

    def setup_chain(self):
        print ("Initializing test directory " + self.options.tmpdir)
        initialize_chain_clean(self.options.tmpdir, 2)

    def setup_network(self, split=False):
        node_opts = [
            "-rpcservertimeout=0",
            "-debug=weakblocks",
            "-use-grapheneblocks=0",
            "-excessiveblocksize=6000000",
            "-blockprioritysize=6000000",
            "-blockmaxsize=6000000"]

        self.nodes = [
            start_node(0, self.options.tmpdir, node_opts),
            start_node(1, self.options.tmpdir, node_opts)
        ]

        self.is_network_split = False
        interconnect_nodes(self.nodes)
        self.sync_all()

    def run_test(self):
        # Generate some blocks
        self.nodes[0].generate(105)
        self.sync_blocks()

        logging.info("Send 5 transactions from node0 (to its own address)")
        addr = self.nodes[0].getnewaddress()
        for i in range(5):
            self.nodes[0].sendtoaddress(addr, Decimal("10"))

        node_count = 0
        miner_node = 0
        for i in range(30):
            new_block = self.nodes[miner_node].generatesubblocks(1)
            # TODO : fix this wait,
            # sync_blocks does not handle subblocks yet, so manually wait here for now
            time.sleep(1)
            assert_equal(new_block, self.nodes[miner_node].getdagtips())
            node_count = node_count + 1
            assert_equal(self.nodes[miner_node].getdaginfo()["size"], node_count)
            # compare node 0 and node 1 to check for proper relay
            assert_equal(self.nodes[0].getdaginfo()["size"], self.nodes[1].getdaginfo()["size"])
            assert_equal(self.nodes[0].getdagtips(), self.nodes[1].getdagtips())
            if miner_node == 0:
                miner_node = 1
            elif miner_node == 1:
                miner_node = 0

        self.sync_blocks()

if __name__ == '__main__':
    DeltaBlocksTest().main()