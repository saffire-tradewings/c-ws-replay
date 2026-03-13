#if !defined(_WIN32)
	#define _POSIX_C_SOURCE 199309L
#endif

#include <stdint.h>
#include <time.h>

#if defined(_WIN32)
	#define WIN32_LEAN_AND_MEAN
	#include <windows.h>
#else
	#include <unistd.h>
#endif

static inline uint64_t now_ns_mono(void)
{
#if defined(_WIN32)
	static LARGE_INTEGER freq = {0};
	if (freq.QuadPart == 0)
		QueryPerformanceFrequency(&freq);
	LARGE_INTEGER ctr;
	QueryPerformanceCounter(&ctr);
	return (uint64_t)((double)ctr.QuadPart / (double)freq.QuadPart * 1e9);
#else
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
	return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
#endif
}

void stw_replay_sleep_until(uint64_t target_ns)
{
	for (;;) {
		uint64_t t = now_ns_mono();
		if (t >= target_ns) return;
		uint64_t dt = target_ns - t;
#if defined(_WIN32)
		DWORD ms = (DWORD)(dt / 1000000ull);
		if (ms > 0) Sleep(ms);
		else SwitchToThread();
#else
		struct timespec rq;
		rq.tv_sec = (time_t)(dt / 1000000000ull);
		rq.tv_nsec = (long)(dt % 1000000000ull);
		nanosleep(&rq, NULL);
#endif
	}
}
