## What it does
This library and CLI read your stdolog-formatted files (produced by your existing
trading code), extract only the **WS** level lines that contain `Message received: <JSON>`,
and then **replay** those messages with original inter-message timing (scaled by a speed factor).

From your app’s perspective, it looks like a live WebSocket feed.

---

## Why use it
- Run your indicator stack **off-market** with real captured sessions.
- Test at arbitrary timeframes: 500µs, 500ms, 1s, 5s, 3m…
- Deterministic re-runs (same log → same results).
- Debug safely with AddressSanitizer/Valgrind against replayed sessions.

---

## Architecture
```
   stdolog log file
         │
         ▼
   parser.c — extracts WS lines + timestamps
         │
         ▼
   replay.c — schedules frames (speed / sleep / filter / loop)
         │
         ▼
   user callback — your cb_receive, candle builder, pivot detector, etc.
```

---

## Build
Clone and build:
```bash
git clone https://github.com/saffire-tradewings/c-ws-replay.git
cd c-ws-replay
make
```

Outputs:
- `build/libstwreplay.a` — static library
- `build/bin/wsreplay`   — CLI (log → JSON stdout)
- `build/bin/demo`       — demo: 1s candles from cb_receive
- `build/bin/demo_ns`    — demo: multi-timeframe candles + CSV export

---

## Usage Recipes

### 1. Quick sanity check
```bash
./build/bin/wsreplay -f tests/sample.log -s 1.0
```
Prints JSON frames to stdout, realtime speed.

### 2. Fast backfill (ignore sleeps)
```bash
./build/bin/wsreplay -f tests/sample.log --no-sleep
```
Replays the file as fast as possible.

### 3. Run demo with candles
```bash
./build/bin/demo -f tests/sample.log -tf 1 -cap 4096 --no-sleep
```
Builds 1-second candles and shows last candle summary.

### 4. Multi-timeframe with CSV export
```bash
./build/bin/demo_ns tests/sample.log ./out --no-sleep
```
Exports:
- `out/candle_500us.csv`
- `out/candle_500ms.csv`
- `out/candle_1s.csv`
- `out/candle_3m.csv`

Each CSV has:
```
ts,open,high,low,close
```

### 5. Filter for a symbol
```bash
./build/bin/demo -f tests/sample.log --filter NIFTY
```
Only replays WS frames containing `NIFTY`.

### 6. Start later in log
```bash
./build/bin/wsreplay -f tests/sample.log -o 30
```
Skip the first 30 seconds of replay.

### 7. Loop forever
```bash
./build/bin/wsreplay -f tests/sample.log --loop
```
Useful for continuous burn-in testing.

---

## Integration into your project

In code:
```c
#include <stw/replay.h>

static void my_cb(void* user, const char* json, size_t len) {
    cb_receive(NULL, user, (void*)json, len); // reuse your existing WS handler
}

stw_replay_opts_t opt = {
  .logfile = "tests/sample.log",
  .speed   = 1.0,
  .no_sleep = true,
};

stw_replay_run_simple(&opt, my_cb, NULL);
```

This preserves your entire pipeline — candles, pivots, indicators — but fed from logs.

---

## Developer Notes
- Modify **`cb_receive_compat`** in `demo_main.c` to test your own indicators.
- `demo_ns.c` shows how to build multiple timeframe candles in parallel.
- The `user` pointer in callbacks lets you avoid globals — pass custom structs for cleaner multi-module testing.
- Use `--no-sleep` for speed, or leave it off to respect original WS timing.
- Filtering, looping, offsets, and speed scaling make replay flexible.

---

## Next steps
- Extend `cb_receive_compat` to extract bid/ask/volume.
- Add unit tests under `tests/` to validate candle counts.
- Optional: integrate GitHub Actions for CI builds.
