# Self-authored MDX fixtures

`minimal-fm.mdx.hex` is a 77-byte RPCMP-authored structural fixture. It contains
no third-party music, driver binary, ROM data, or PDX sample.

```text
00..07  ASCII title "RPCMP M3"
08..0a  title terminator 0d 0a 1a
0b      empty PDX reference terminator
0c..1f  big-endian offsets: voice=0026, A=0014 through P=0024
20..31  nine inert tracks, each f1 00
32..4c  one 27-byte voice: id=01, feedback/connection=00, mask=0f,
        all six four-operator parameter groups zero
```

The `.hex` text is only a reviewable representation. Tests decode it and assert
the exact binary length and every parsed field before generating malformed
copies in memory.

`../rpcmlib/mdx-session.rpcmlib.hex` is the deterministic M5 library produced
from `oracle-fm.mdx.hex` with title `Self-authored FM fixture` and an empty
artist. It contains no third-party music data.
