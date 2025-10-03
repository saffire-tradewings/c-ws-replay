#ifndef STW_REPLAY_H
#define STW_REPLAY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// clang-format off

#ifdef __cplusplus
extern "C" {
#endif

/**
 * stw-ws-replay — Simulated WebSocket feed from stdolog logs
 * ==========================================================
 *
 * Purpose:
 * --------
 * This library allows you to **replay WebSocket messages** captured in your
 * stdolog-formatted log files, so you can test indicators, candle builders,
 * and pivot logic *off-market* without connecting to a broker.
 *
 * Input:
 * ------
 *  - Log files produced by your project’s stdolog system, where each WS frame
 *    looks like:
 *
 *    1756975187763637563 | WS    | 138519091494592:92223 | src/greeksoft.c:123 | [msg] {"response":{"BCastTime":"1727278234","data":{"ltp":"24519.35"}}}
 *
 *    The **leading number** is nanoseconds (from stw_time_ns). We use that to
 *    reconstruct relative timing between messages.
 *
 * Output:
 * -------
 *  - Calls your callback for each replayed JSON frame, in the exact same order
 *    as recorded.
 *  - Optionally honors the original timing (realtime), or skips it for fast
 *    backfill/replay.
 *
 *
 * Architecture Flow:
 * ------------------
 *
 *   ┌───────────────┐     ┌───────────────┐     ┌───────────────┐
 *   │ stdolog file  │ --> │ parser.c      │ --> │ replay.c      │
 *   │ (WS lines)    │     │  - detect WS  │     │  - timing     │
 *   │               │     │  - extract ns │     │  - loop/filter│
 *   └───────────────┘     └───────────────┘     └───────┬───────┘
 *                                                       │
 *                                                       ▼
 *                                              ┌───────────────────┐
 *                                              │ user callback     │
 *                                              │ cb(user,json,len) │
 *                                              └───────────────────┘
 *
 *   Example: user callback may build candles, run pivots, log to CSV, etc.
 *
 *
 * Key Concepts:
 * -------------
 * - **Replay options** (`stw_replay_opts_t`) configure speed, looping, filters.
 * - **Callback** (`stw_replay_msg_cb`) is invoked per WS frame with raw JSON.
 * - **User pointer** (`void* user`) allows you to pass state/context into the
 *   callback without using globals.
 * - **Timing** can be real (`nanosleep` to match log intervals) or disabled
 *   (`--no-sleep`) for offline simulation.
 *
 * Typical Usage:
 * --------------
 * ```c
 * static void my_cb(void* user, const char* json, size_t len) {
 *     (void)user;
 *     printf("Got WS frame: %.*s\n", (int)len, json);
 * }
 *
 * stw_replay_opts_t opt = {
 *     .logfile = "tests/sample.log",
 *     .speed   = 1.0,
 *     .no_sleep = true,
 * };
 *
 * stw_replay_run_simple(&opt, my_cb, NULL);
 * ```
 *
 * Integration:
 * ------------
 * - You can drop this into your trading system by wiring the replay engine to
 *   your existing `cb_receive` function. It will behave as if the data came
 *   from a live WebSocket.
 * - Use `demo_main.c` or `demo_ns.c` as starting points.
 *
 * Advanced Features:
 * ------------------
 * - **Speed factor**: Run faster/slower than realtime.
 * - **Start offset**: Skip into the log.
 * - **Looping**: Restart log on EOF.
 * - **Filtering**: Only replay lines that contain a substring (e.g., symbol).
 * - **Hard stop**: Stop after N frames.
 * - **Compatibility shim**: Wraps to call your `cb_receive(wsi,user,in,len)`
 *   signature unchanged.
 */

/** Callback type: invoked for each replayed JSON frame */
typedef void (*stw_replay_msg_cb)(void* user, const char* json, size_t len);

/** Replay options structure */
typedef struct stw_replay_opts {
    const char* logfile;       /**< Path to log file (required) */
    double      speed;         /**< Replay speed factor. 1.0 = realtime, 2.0 = twice as fast. Default = 1.0 */
    double      start_offset_s;/**< Skip this many seconds from the beginning. Default = 0.0 */
    bool        loop;          /**< Loop replay when reaching end-of-file. Default = false */
    bool        no_sleep;      /**< If true, disables nanosleep; replay as fast as possible. Default = false */
    const char* filter_substr; /**< Only replay lines containing this substring (e.g. instrument symbol). Default = NULL */
    uint64_t    hard_stop_count; /**< Stop after N messages. 0 = unlimited. Default = 0 */
    bool        verbose;       /**< If true, print debug info for each frame. Default = false */
} stw_replay_opts_t;

/** Parsed log frame (minimal fields we need) */
typedef struct stw_log_frame {
    uint64_t ns;       /**< Log timestamp in nanoseconds (from stw_time_ns) */
    const char* json;  /**< Pointer into buffer where JSON text starts */
    size_t   json_len; /**< Length of JSON text */
} stw_log_frame_t;

/** Opaque replay state */
typedef struct stw_replay stw_replay_t;

/**
 * Create a replay session.
 * - Returns a handle, or NULL on error.
 */
stw_replay_t* stw_replay_create(const stw_replay_opts_t* opts);

/**
 * Destroy a replay session.
 */
void          stw_replay_destroy(stw_replay_t* R);

/**
 * Run replay loop.
 * - Blocks until EOF (or hard stop count) reached.
 * - Calls `cb(user,json,len)` for each WS frame.
 * - Honors options: speed, no_sleep, loop, filter.
 * - Returns 0 on success, non-zero on error.
 */
int stw_replay_run(stw_replay_t* R, stw_replay_msg_cb cb, void* user);

/**
 * Convenience: create, run, and destroy in one call.
 * - Safer for most use cases.
 */
int stw_replay_run_simple(const stw_replay_opts_t* opts, stw_replay_msg_cb cb, void* user);

#ifdef __cplusplus
}
#endif

#endif /* STW_REPLAY_H */