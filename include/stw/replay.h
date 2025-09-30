#ifndef STW_REPLAY_H
#define STW_REPLAY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Callback invoked for each replayed JSON frame. */
typedef void (*stw_replay_msg_cb)(void* user, const char* json, size_t len);

/** Per-run options */
typedef struct stw_replay_opts {
	const char* logfile;	   /* required */
	double speed;		   /* 1.0 = realtime; 2.0 twice as fast; default 1.0 */
	double start_offset_s;	   /* skip this many seconds from the beginning */
	bool loop;		   /* loop the file */
	bool no_sleep;		   /* deliver as fast as possible (ignores timing) */
	const char* filter_substr; /* if set, only lines containing this (e.g., a symbol) */
	uint64_t hard_stop_count;  /* stop after N messages (0 = unlimited) */
} stw_replay_opts_t;

/** Parsed line (minimal fields we need) */
typedef struct stw_log_frame {
	uint64_t ns;	  /* leading nanoseconds in the log */
	const char* json; /* pointer into owned buffer */
	size_t json_len;
} stw_log_frame_t;

/** Opaque state for streaming parser */
typedef struct stw_replay stw_replay_t;

/** Create/destroy a replay session */
stw_replay_t* stw_replay_create(const stw_replay_opts_t* opts);
void stw_replay_destroy(stw_replay_t* R);

/** Run the replay: blocks; calls cb in order honoring timing */
int stw_replay_run(stw_replay_t* R, stw_replay_msg_cb cb, void* user);

/** Convenience one-shot */
int stw_replay_run_simple(const stw_replay_opts_t* opts, stw_replay_msg_cb cb, void* user);

#ifdef __cplusplus
}
#endif

#endif /* STW_REPLAY_H */
