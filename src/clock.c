#define _POSIX_C_SOURCE 199309L

#include <stdint.h>
#include <time.h>
#include <unistd.h>

static inline uint64_t now_ns_mono(void)
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
	return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}

void stw_replay_sleep_until(uint64_t target_ns)
{
	for (;;) {
		uint64_t t = now_ns_mono();
		if (t >= target_ns) return;
		uint64_t dt = target_ns - t;
		struct timespec rq;
		rq.tv_sec = (time_t)(dt / 1000000000ull);
		rq.tv_nsec = (long)(dt % 1000000000ull);
		nanosleep(&rq, NULL);
	}
}
