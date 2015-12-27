
blkaf1
======

It seems that the CBM DOS does not write back the BAM block correctly. After the test, the blocks
T/S= 1/2, 1/2, 1/3 should be allocated. This is correctly reflected in the resulting D64 file
from pcserver:

--- /tmp/tmp.8T4RMgqPmB/shouldbe_blk.d64.hex    2015-12-27 23:02:12.631026578 +0100
+++ /tmp/tmp.8T4RMgqPmB/blk.d64.hex     2015-12-27 23:02:12.635026685 +0100
@@ -1,6 +1,6 @@
 00000000  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
 *
-00016500  12 01 41 00 15 ff ff 1f  15 ff ff 1f 15 ff ff 1f  |..A.............|
+00016500  12 01 41 00 12 f1 ff 1f  15 ff ff 1f 15 ff ff 1f  |..A.............|
 00016510  15 ff ff 1f 15 ff ff 1f  15 ff ff 1f 15 ff ff 1f  |................|
 *
 00016540  15 ff ff 1f 15 ff ff 1f  11 fc ff 07 13 ff ff 07  |................|
blkaf1-4040.frs: File blk.d64 differs!

blkaf2
======

This is basically the same test as blkaf1, with a DIRECTORY moved around, and opening of a
direct file; Here the BAM is correctly written, but CBM DOS seems to incorrectly write
DIRECTORY output into the BAM block...:

--- /tmp/tmp.XCiaIP2sx1/shouldbe_blk.d64.hex    2015-12-27 23:17:41.191866281 +0100
+++ /tmp/tmp.XCiaIP2sx1/blk.d64.hex     2015-12-27 23:17:41.191866281 +0100
@@ -10,9 +10,7 @@
 00016580  11 ff ff 01 11 ff ff 01  11 ff ff 01 11 ff ff 01  |................|
 00016590  42 4c 4b a0 a0 a0 a0 a0  a0 a0 a0 a0 a0 a0 a0 a0  |BLK.............|
 000165a0  a0 a0 30 31 a0 32 41 a0  a0 a0 a0 00 00 00 00 00  |..01.2A.........|
-000165b0  00 00 00 00 42 4c 4f 43  4b 53 20 46 52 45 45 2e  |....BLOCKS FREE.|
-000165c0  20 20 20 20 20 20 20 20  20 20 20 20 20 20 20 00  |               .|
-000165d0  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
+000165b0  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
 *
 00016600  00 ff 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
 00016610  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
blkaf2-4040.frs: File blk.d64 differs!


