# Album fixture provenance

`album.rpcmlib.hex` is a synthetic, independently encoded 1360-byte rpcmlib 1.0
file with optional ALBM 1.0. It contains two albums and four tracks sharing the
three arbitrary blob bytes `01 02 03`; these are not playable or copied music.
Display order is Beta, Alpha, with B2/B1 and A2/A1 member order, deliberately
different from stored ID order.

`generate_album_fixture.py` lays out the contract using Python `struct`,
`hashlib` and `zlib`. It does not import or run the C++ writer or reader and is
not run during builds/tests. It was authored before that implementation.
The decoded fixture SHA-256 is
`7f43755016e20abca671fc07a8e7e177a3c66d93f958ba733e45dda145fcddc7`.

Tests compare the writer's complete output against the committed bytes and
make CRC-correct mutations to exercise logical rejection. Change the fixture
only with independently reviewed contract evidence; never regenerate it to
make an implementation failure disappear.
