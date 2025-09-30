# =====================
# README.md
# =====================
# stw-ws-replay — log→"live" feed simulator

## What it does
This tiny library and CLI read your stdolog-formatted files (the ones produced by `stw_stdolog_msg`),
extract only the **WS** level lines that contain `Message received: <JSON>`, and then **replay** those
messages with original inter-message timing (scaled by a speed factor). From your app’s perspective,
it looks like a live WebSocket feed.

## Why this is nice for you
- Run your indicator stack off-market at 500µs, 500ms, 1s, 5s, 3m… by **replaying** real sessions.
- Deterministic re-runs (same log → same candles/pivots), great for debugging ASan issues.
- Minimal glue: you can reuse your existing `cb_receive(...)` as-is.

## Inputs it expects
Your log lines like (from your `stdolog.c`):
```
1756975187763637563 | WS    | 138519091494592:92223 | src/greeksoft.c:123 | Message received: {"response":{"BCastTime":"1727278234","data":{"ltp":"24519.35"}}}
```
The **first field is nanoseconds** (from `stw_time_ns`). We use that to reproduce timing.

## Two ways to use
1) **Embed the library** in your project and call `stw_replay_run(...)` with your callback pointer.
2) **Use the demo** binary here (or copy it) that wires your provided `cb_receive` logic to the replay.

## Quick start
```
make
# Run CLI (prints each JSON or just sleeps to simulate):
./build/bin/wsreplay -f tests/sample.log -s 1.0

# Run demo (feeds your cb_receive to build candles):
./build/bin/demo -f tests/sample.log -s 1.0 -tf 1 -cap 2048
```

### Demo flags
- `-f <path>`: log file to replay
- `-s <speed>`: 1.0 = realtime; 2.0 = twice as fast; 0.5 = half-speed
- `-o <offset>`: start from `offset` seconds into the log (based on the first line’s ns)
- `-tf <sec>`: target candle TF in seconds (e.g., 1, 5, 60, 180)
- `-cap <N>`: candle capacity
- `--loop`: loop forever
- `--no-sleep`: deliver as fast as possible (useful for long backfills)
- `--filter "substr"`: only replay WS lines that contain this substring (e.g., a symbol)

## Sub-second timeframes
If you want 500ms or 500µs candles, use your `stw_candle_ns_t` pipeline. The demo shows how to
keep seconds TF. Mapping tick ns to candle ns is straightforward with the provided ns timestamps.

---
