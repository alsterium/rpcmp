# MDX Engine

The v1 structural parser validates an immutable MDX byte view and publishes
bounded borrowed views, fixed track targets, and decoded voice records. It does
not execute track commands, resolve PDX paths, render UI, or bind to a concrete
YM2151/PCM implementation. The caller must keep accepted input bytes alive and
immutable for the document lifetime.
