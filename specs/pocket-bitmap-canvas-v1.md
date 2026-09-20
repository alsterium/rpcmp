# Pocket bitmap canvas v1

Internal platform adapter for M6 slice 5, within the approved
[screen design](../docs/design/pocket-tracker-screen-design.md) and
[text profile](../docs/design/pocket-text-rendering-profile.md). This does not
change PlayerCanvas, commands, snapshots, playback timing, or the host SVG grid.

## Font and rasterization

RPCMP Bitmap JP is the OFL-1.1 derivative of pinned GNU Unifont Japanese 16.0.04.
The offline generator checks the CP932/font source hashes, derives NFC scalars
with ingestion's utf8proc 2.11.3 / Unicode 17.0.0, and includes the original
printable CP932 scalars plus the specified UI symbols. It rejects duplicate
source glyphs, missing selected glyphs, unsupported widths, more than 8192
glyphs or more than 512 KiB of index/bitmap. All glyphs are 16 pixels high;
advances come from their actual 8/16-pixel width. This particular font's
ellipsis and replacement character are **8 pixels**, unlike the SVG mock's
provisional 16-pixel convention. Glyph pixels are unchanged.

The generated, sorted read-only index is searched without allocation or a
cache. Unknown scalars, including controls absent from the font subset, draw
the visible replacement glyph. Their presence is observable on the canvas;
metadata bytes remain unchanged. Strict public UTF-8 validation happens before
decoding. Text is bounded to 96 source bytes, copied synchronously, clipped at
whole-glyph boundaries horizontally and to its box vertically. Elision reserves
one complete ellipsis, including a source-truncated prefix. If even that does
not fit, the box remains empty. No shaping or scrolling is claimed.

The 640x480 output uses 8-bit palette indices. The shared ten UI colors retain
their exact RGB values. Other valid RGB24 colors map to the nearest palette
color by squared RGB distance, with first entry winning ties. Rectangles cover
their half-open bounds; integer Bresenham lines include both endpoints. Invalid
geometry/color, malformed/oversized text, and command-capacity exhaustion make
the frame fail explicitly. Never present a failed or incomplete frame.

## Ownership and bounded work

`begin` starts one copied command list with capacity 2048, sufficient for all
three current shared views. `seal` closes recording; subsequent drawing fails
until a new begin. The canvas has no Core, audio, clock, input or storage access.
`pump` rasterizes at most its caller-provided pixel budget; transparent glyph
pixels count too. It validates the surface extent/stride before access, never
writes row padding, and rejects changing the surface or stride mid-frame.
Zero budget does no pixel work. Every recorded command and text is owned until
complete or explicitly abandoned by begin. Keep the workspace out of the stack.

The application owner must service audio between pump calls, record frames
from immutable observations, and present only complete successful frames.
Pixel work bounds are not microsecond timing guarantees. Command recording,
snapshot publication, input polling, framebuffer cache maintenance and OS
presentation still require target measurements. This adapter alone neither
owns the SDK triple buffers nor proves uninterrupted Pocket playback.

Tests use authored pixel coordinates, glyph bits compared directly against
the pinned source, independent NFC examples, malformed inputs and canary row
padding. All views must render identically with small and large pump budgets.
The canvas mock links UI/contracts without player, library or runtime.
