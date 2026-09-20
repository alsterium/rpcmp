# Album player UI v1

Status: adopted for M6 slice 3, 2026-09-13. Defines the interaction and mock
presentation contract for the [accepted layout](../docs/design/pocket-tracker-screen-design.md).
Core is accessed only through schema 2 snapshots/commands and CatalogReader.
The M0 UI and public v1 Core types remain unchanged. Physical bindings, focus
adjacency and rendering live in UI/platform, never in playback engines.
The user revised panel navigation on 2026-09-20 after the first hardware trial:
D-pad moves focus between panels; Back never changes panels or display mode.

## Policy execution feedback

To serialize rapid setting actions, PlayerSnapshot gains optional
`policy_commands: PolicyCommandObservation { optional last }`, with capability
bit 8 (`kPolicyCommandResults`). `last` contains command ID, policy revision and
Applied/Failed. It records the last accepted SetPlaybackPolicy consumed by the
Core step, including same-value commands. Revision is the final desired-policy
revision at the end of that command batch, not an audio or durable-save ACK.
Applied says the desired setting was applied (or already equal); subsequent
audio failure remains an independent transport error. Failed includes interrupted
or unexecuted commands. Ingress rejection is still returned synchronously.
Other commands, playback, saving and publication do not erase this result.

The optional wrapper distinguishes unsupported producers from supported ones
with no result yet. Presence matches the capability and requires the policy
observation capability; a result needs a nonzero ID/revision, revision no greater
than current and a valid outcome. Existing publishers default to absence.
This is the concrete adoption of the earlier `settings.last_policy_result`
proposal; its location is independent of persistent storage support.

One interactive UI owns the command-ID namespace and serializes policy inputs.
It keeps at most eight semantic RepeatStep/ToggleShuffle actions, including the
in-flight head, and sends only that head. On a matching result it removes the
head and constructs the next policy from the newest desired policy. A failure
is shown, then remaining actions use current truth; save completion is not a
barrier. Rejection removes the rejected input. No action is silently retried.
Missing result capability disables these controls. A later unrelated result
while waiting is an explicit protocol error, not an infinite spinner.

## Input and focus

An input adapter supplies a connected flag, 16-bit button mask and monotonic
microseconds. A replaceable table maps eight distinct single-bit masks to
Back, Previous, Next, Confirm, Up, Down, Left, Right. Invalid tables emit no
action. First connection/reconnection/rebinding masks held buttons until release.
Non-direction buttons require a fresh edge. Direction press moves once, repeats
after 350,000 us and then every 80,000 us; configurable durations must be nonzero.
A delayed sample emits at most one movement, without catching up missed repeats.
Reversed time rearms input instead of inventing an edge.

Opposite directions cancel; vertical wins over horizontal. In one sample choose
at most one action: Back, Previous/Next, Confirm, direction. Simultaneous held
Previous+Next produces neither neighbour action. Lower-priority edges are
discarded, not replayed later. These rules never use audio/video frame counts.

Focus is Main or one of Previous, PlayPause, Stop, Next, View, Repeat, Shuffle.
Main covers the current Tracker, Keyboard or Library view (`List` remains a
source-compatible alias for the same focus value).
The panel has top row Previous/PlayPause/Stop/Next and bottom View/Repeat/Shuffle.
A replaceable checked adjacency table maps four directions. Default: horizontal
neighbours, no wrap; down connects matching columns (Next to Shuffle), up reverses
them (Shuffle to Stop). Up from the top row enters Main; left from either row's
first icon also enters Main, including during playback errors. Main right goes
to PlayPause; Library up/down moves its cursor and left stays. Tracker/Keyboard
Down also enters PlayPause; their Up/Left stay. Empty lists remain
focusable. Disabled icons retain their positions and show a reason on Confirm.

View cycles Tracker -> Keyboard -> Library -> Tracker, keeping focus on View.
It sends no player command. Browsing keeps playing and never changes lower
metadata. Album Confirm enters its track list; track Confirm sends one PlayTrack
with copied generation/ID. Only Accepted returns to the last monitor view
(initially Tracker) and focuses PlayPause. Failure stays in the list.
Auto advance, neighbour commands, save results and new snapshots never steal
focus or browsing position. Policy symbols cycle Default -> Counted(3) ->
Counted(5) -> RepeatOne -> Default, preserving order. Counted(2) advances to 3;
other valid counts display their value and advance to Default. Shuffle toggles
only order. Focus movement never changes a setting.

PlayPause Confirm maps Playing to Pause, Paused to Resume and selected
Stopped/Ended to Play. It is disabled without selection, during preparation or
audio control, on Error, and while an accepted transport action is still
unobserved. A newer public sequence plus a new generation retires a selection
guard; a same-generation Play/Pause/Resume needs its resulting state and no
pending control. This only retires the guard: the snapshot alone declares
playback success/failure. A transport error retires the guard with an error.
Neighbour edges and icons use current-player can_previous/can_next, never the
browsing cursor, and preserve focus. Stop can supersede pending selection.

Main Tracker/Keyboard Confirm has the same meaning as PlayPause. Back in the
focused Library goes from tracks to albums, or stays at albums, regardless of
playback state; it sends no player command. Back elsewhere in
Playing/Paused/Loading/Ended sends Stop and stays in the view. Further
Back while that stop is pending is ignored, never queued. Completion requires
a newer sequence and generation, Stopped, confirmed silence and no preparation,
audio control or pending intent. Back on a stopped/empty monitor is a no-op;
it never opens a list or moves focus. Error Back preserves the error; D-pad and
View remain usable, and explicit selection provides recovery when Core is not
terminal. The Stop icon always means Stop, never Back.

Stop is idempotent in the Core. A Stop issued against an already-Stopped
observation with no unobserved local transport command can confirm that existing
generation once its pending work clears; already-confirmed silence needs no new
wait. It still sends Stop and never navigates. A pending Play/Pause/Resume guard
also retires on a newer, settled Ended observation, so natural completion cannot
leave the play control waiting for an obsolete intermediate state.

Commands use checked increasing nonzero IDs and the currently observed snapshot
sequence. Exhaustion disables sending with a visible reason. A replacement
controller needs a fresh owner-supplied ID seed; rebinding an existing controller
does not reset its IDs. Duplicate/unmatched malformed replies are protocol errors.

## Catalog and observation ownership

The controller copies and validates snapshots before access. Backward/invalid
observations disable commands and show invalid data; they do not mutate Core.
Repeated immutable sequence values do not replay acknowledgements or history.
Catalog pages are copied, validated against the exact query/generation/album and
bounded to sixteen entries. One page per visible level is sufficient; moving
across a page boundary queries the next page, never a container offset.
Opening Library starts at albums and retains that browsing position thereafter.
IDs and cursor survive view switches only within the same catalog generation;
generation replacement invalidates the browse path. Query failure disables
selection instead of activating a stale item.

Tracker rows are authored-event rows, not invented video frames or BPM ticks.
Greedily combine adjacent events at the same audio frame until a channel would
appear twice; then begin another row. A sequence gap also starts another row.
The row ID is its first source sequence. Keep the newest sixteen visible rows;
their cells are copied changes, empty where no change occurred. KeyOff shows
OFF, notes use MIDI-like C-4 for 60, known voice numbers use two hexadecimal
digits, and unknown values show --. Preserve Off/On and same-note retriggers.

Track prior generation/next sequence to distinguish a missed event from a skipped
snapshot publication. A new generation clears continuity. One persistent gap
indicator reports missed/capture-lost events; retention of earlier history has a
separate indicator. Do not fill gaps with synthetic notes. Waiting/Invalid/
unsupported/exhausted history remain distinct. Keyboard rows use independent
current channels, never reconstructed incomplete event history; unknown or
key-off has no active key, and Paused visibly labels held state.

## Mock presentation and remaining target work

A provisional 640x480 logical canvas has a 24-pixel header, main area through
y=367, shared information/control panel y=368..455 and status y=456..479.
The main view changes while the shared panel stays fixed. The dark palette,
teal text/current row and thin borders follow the accepted reference. The
keyboard shows eight rows across the MIDI range; the library shows sixteen
bounded names and a cursor. Unknown length/tempo/level is not fabricated.
Elapsed time comes only from position_frames/frame_rate.

The synchronous PlayerCanvas port uses RGB24 colors and bounded pixel boxes.
It consumes/copies text before returning; no borrowed label survives a drawing
call. The renderer uses no heap allocation. Keyboard note labels show OFF for a
known key-off and -- for an unknown gate; retained pitch/voice metadata cannot
turn a silent channel into an active key. Paused active keys use a distinct color.
The shared panel uses the selected track, confirmed transport/position, desired
policy and independent save status. Browsing only moves the list cursor/marker.

The host SVG port uses an explicit mock grid: 8 pixels below U+1100 and for
halfwidth katakana U+FF61..U+FF9F, otherwise 16 pixels. This is not Unicode width
classification or a Pocket font contract. Text is bounded to the public 96-byte
UTF-8 capacity. Elision preserves code-point boundaries and reserves a full
16-pixel ellipsis, including for a source-truncated prefix. C0/C1 controls and
U+FFFE/U+FFFF render as replacement glyphs; the latter two are outside the
[XML 1.0 character range](https://www.w3.org/TR/xml/#charsets). XML metacharacters
are escaped. Invalid UTF-8, oversized text or geometry outside the logical
canvas makes the output fail explicitly. No external asset or script is emitted.

Rendering uses a platform-facing canvas for bounded text, rectangles and lines.
The host SVG adapter must provide reviewable mock artifacts with escaped text and
clipping. It is not the Pocket font/framebuffer adapter or proof of Japanese
glyph metrics/readability. Snapshot/mock-only UI targets must link contracts
and UI without runtime/player/library. Rendering cannot submit commands or
advance the controller. Input-table replacement, event rows/gaps, stale catalogs,
rapid policy actions and stop/selection ACK races need authored host tests.
Licensed bitmap fonts, actual input bits, framebuffer ownership, display density
and real audio/storage timing remain M6 slices 4–6 acceptance work.
