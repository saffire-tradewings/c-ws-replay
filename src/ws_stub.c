/* Optional drop-in adapter: if you want to keep your existing cb_receive signature
 * (struct lws*, void*, void*, size_t) we provide a tiny shim that calls it.
 * You can pass a pointer to your existing cb as `user_cb` in demo_main.c
 */
#include "stw/replay.h"

#include <stddef.h>

typedef void (*stw_ws_compat_cb)(void* wsi, void* user, void* in, size_t len);

typedef struct {
	stw_ws_compat_cb cb;
	void* user;
} stw_ws_shim_t;

static void _shim_cb(void* u, const char* json, size_t len)
{
	stw_ws_shim_t* S = (stw_ws_shim_t*)u;
	if (S && S->cb) S->cb(NULL, S->user, (void*)json, len);
}

int stw_replay_run_compat(const stw_replay_opts_t* opt, stw_ws_compat_cb cb, void* user)
{
	stw_ws_shim_t S = {.cb = cb, .user = user};
	return stw_replay_run_simple(opt, _shim_cb, &S);
}

/* Not exposed via header to keep replay.h minimal; copy signature if needed. */
