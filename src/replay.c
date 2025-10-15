#define _GNU_SOURCE
// #define _POSIX_C_SOURCE 199309L

#include "stw/replay.h"

#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

/* internal from parser.c */
bool _stw_parser_try_extract(const char* line, const char* filter, stw_log_frame_t* out);
void stw_replay_sleep_until(uint64_t target_ns);

struct stw_replay {
	stw_replay_opts_t opt;
	FILE* fp;
	uint64_t first_ns; /* ns of first accepted frame */
};

static FILE* xfopen(const char* path)
{
	FILE* f = fopen(path, "rb");
	if (!f) {
		fprintf(stderr, "replay: fopen('%s') failed: %s\n", path, strerror(errno));
	}
	return f;
}

static void reset_file(struct stw_replay* R)
{
	if (!R || !R->fp) return;
	fseek(R->fp, 0, SEEK_SET);
	R->first_ns = 0;
}

stw_replay_t* stw_replay_create(const stw_replay_opts_t* opts)
{
	if (!opts || !opts->logfile) return NULL;
	stw_replay_t* R = (stw_replay_t*)calloc(1, sizeof(*R));
	if (!R) return NULL;
	R->opt = *opts;
	if (R->opt.speed <= 0.0) R->opt.speed = 1.0;
	R->fp = xfopen(R->opt.logfile);
	if (!R->fp) {
		free(R);
		return NULL;
	}
	return R;
}

void stw_replay_destroy(stw_replay_t* R)
{
	if (!R) return;
	if (R->fp) fclose(R->fp);
	free(R);
}

static int run_once(stw_replay_t* R, stw_replay_msg_cb cb, void* user)
{
	char* line = NULL;
	size_t cap = 0;
	ssize_t n = 0;
	uint64_t base_ns = 0; // wall clock base when first frame delivered

	uint64_t delivered = 0;

	while ((n = getline(&line, &cap, R->fp)) != -1) {
		stw_log_frame_t f = {0};
		if (!_stw_parser_try_extract(line, R->opt.filter_substr, &f))
			continue; // not a WS frame we care about

		if (R->first_ns == 0) {
			R->first_ns = f.ns;
		}
		// Apply start offset (in seconds) by skipping frames earlier than first_ns + offset
		uint64_t start_cut = R->first_ns + (uint64_t)(R->opt.start_offset_s * 1e9);
		if (f.ns < start_cut) continue;

		if (!R->opt.no_sleep) {
			if (base_ns == 0) {
				base_ns = f.ns;
			}
			// replay time = (f.ns - base_ns)/speed
			double rel_ns = (double)(f.ns - base_ns) / R->opt.speed;
			static uint64_t t0 = 0;
			if (!t0) {
				t0 = 1;
			}
			// Use monotonic now as epoch 0
			uint64_t now0 = 0;
			{
				struct timespec ts;
				clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
				now0 = (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
			}
			uint64_t target = now0 + (uint64_t)rel_ns;
			stw_replay_sleep_until(target);
		}

		cb(user, f.json, f.json_len);
		if (R->opt.hard_stop_count && ++delivered >= R->opt.hard_stop_count) break;
	}

	free(line);
	return 0;
}

int stw_replay_run(stw_replay_t* R, stw_replay_msg_cb cb, void* user)
{
	if (!R || !cb) return -1;
	do {
		reset_file(R);
		int rc = run_once(R, cb, user);
		if (rc) return rc;
	} while (R->opt.loop);
	return 0;
}

int stw_replay_run_simple(const stw_replay_opts_t* opts, stw_replay_msg_cb cb, void* user)
{
	stw_replay_t* R = stw_replay_create(opts);
	if (!R) return -1;
	int rc = stw_replay_run(R, cb, user);
	stw_replay_destroy(R);
	return rc;
}

#ifdef STW_REPLAY_BUILD_CLI
/* Minimal CLI that just prints JSON or does nothing (useful for timing validation) */
static void sink(void* user, const char* json, size_t len)
{
	(void)user;
	fwrite(json, 1, len, stdout);
	fputc('\n', stdout);
	fflush(stdout);
}

static void usage(const char* argv0)
{
	fprintf(stderr,
		"Usage: %s -f <logfile> [-s speed] [-o start_s] [--loop] [--no-sleep] [--filter "
		"str] [--max N]\n",
		argv0);
}

int main(int argc, char** argv)
{
	stw_replay_opts_t opt = {0};
	opt.speed = 1.0;
	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "-f") && i + 1 < argc)
			opt.logfile = argv[++i];
		else if (!strcmp(argv[i], "-s") && i + 1 < argc)
			opt.speed = atof(argv[++i]);
		else if (!strcmp(argv[i], "-o") && i + 1 < argc)
			opt.start_offset_s = atof(argv[++i]);
		else if (!strcmp(argv[i], "--loop"))
			opt.loop = true;
		else if (!strcmp(argv[i], "--no-sleep"))
			opt.no_sleep = true;
		else if (!strcmp(argv[i], "--filter") && i + 1 < argc)
			opt.filter_substr = argv[++i];
		else if (!strcmp(argv[i], "--max") && i + 1 < argc)
			opt.hard_stop_count = (uint64_t)strtoull(argv[++i], NULL, 10);
		else {
			usage(argv[0]);
			return 2;
		}
	}
	if (!opt.logfile) {
		usage(argv[0]);
		return 2;
	}

	return stw_replay_run_simple(&opt, &sink, NULL);
}
#endif
