/* This demo wires your provided cb_receive() logic to the replay engine.
 * You can replace/extend this file to run your real indicator modules.
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Bring your headers (you uploaded these already). */
#include <stw/candle.h>
#include <stw/exchange.h>
#include <stw/json.h>
#include <stw/time.h>

#include "stw/replay.h"

/* =====================
   Simplified copy of your cb_receive (adapted signature)
   ===================== */

static stw_timestamp_s_t p_ts = 0, ts = 0;
static stw_ohlc_t ohlc = {0.0f, 0.0f, 0.0f, 0.0f};
static stw_candle_s_t candle;
static stw_resampler_ctx_t rsctx;

static void cb_receive_compat(void* wsi, void* user, void* in, size_t len)
{
	(void)wsi;
	(void)user;

	const char* payload = (const char*)in;

	/* Parse the JSON in the WS frame */
	cJSON* json = cJSON_ParseWithLengthOpts(payload, len, NULL, 0);
	if (!json) {
		fprintf(stderr, "JSON parse error in demo cb\n");
		return;
	}

	const cJSON* response = cJSON_GetObjectItemCaseSensitive(json, "response");
	if (response) {
		const cJSON* b_cast_time = cJSON_GetObjectItemCaseSensitive(response, "BCastTime");
		if (b_cast_time && cJSON_IsString(b_cast_time) && b_cast_time->valuestring) {
			p_ts = ts;
			ts = (stw_timestamp_s_t)strtoull(b_cast_time->valuestring, NULL, 10);
		}
		const cJSON* data = cJSON_GetObjectItemCaseSensitive(response, "data");
		if (data) {
			const cJSON* ltp = cJSON_GetObjectItemCaseSensitive(data, "ltp");
			if (ltp && cJSON_IsString(ltp) && ltp->valuestring) {
				ohlc.close = (float)atof(ltp->valuestring);
				if (ohlc.open == 0.0f) {
					ohlc.open = ohlc.high = ohlc.low = ohlc.close;
				} else {
					if (ohlc.close > ohlc.high) ohlc.high = ohlc.close;
					if (ohlc.close < ohlc.low) ohlc.low = ohlc.close;
				}
			}
		}

		if (ts != p_ts && ts != 0) {
			stw_append_candle(&candle, &ohlc, ts, 1);
			/* Reset OHLC for next second */
			ohlc.open = ohlc.high = ohlc.low = ohlc.close;
		}
	}

	cJSON_Delete(json);
}

/* Adapter from replay → your cb_receive_compat */
static void adapter_cb(void* user, const char* json, size_t len)
{
	(void)user;
	cb_receive_compat(NULL, NULL, (void*)json, len);
}

static void usage(const char* argv0)
{
	fprintf(stderr,
		"Usage: %s -f <log> [-s speed] [-tf sec] [-cap N] [--loop] [--filter str] "
		"[--no-sleep]\n",
		argv0);
}

int main(int argc, char** argv)
{
	stw_replay_opts_t opt = {0};
	opt.speed = 1.0;

	/* Candle config */
	uint32_t tf = 1;
	size_t cap = 2048;

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
		else if (!strcmp(argv[i], "-tf") && i + 1 < argc)
			tf = (uint32_t)strtoul(argv[++i], NULL, 10);
		else if (!strcmp(argv[i], "-cap") && i + 1 < argc)
			cap = (size_t)strtoull(argv[++i], NULL, 10);
		else {
			usage(argv[0]);
			return 2;
		}
	}
	if (!opt.logfile) {
		usage(argv[0]);
		return 2;
	}

	/* Allocate candle buffer */
	if (stw_candle_alloc(&candle, cap) != 0) {
		fprintf(stderr, "candle alloc failed\n");
		return 1;
	}
	candle.tf = tf;
	candle.m_open = stw_nse_start_market_time();
	candle.m_close = stw_nse_end_market_time();

	/* Init (optional) resampler context if your upstream expects TF != 1s */
	stw_candle_resampler_init(&rsctx, &candle, 1);
	(void)rsctx; /* shown for completeness */

	int rc = stw_replay_run_simple(&opt, adapter_cb, NULL);

	/* Print a small summary */
	fprintf(stderr,
		"\nReplayed. Built %zu candles at tf=%us. Last: O=%.2f H=%.2f L=%.2f C=%.2f\n",
		candle.arr_size, candle.tf,
		candle.arr_size ? candle.ohlc[candle.arr_size - 1].open : 0.0f,
		candle.arr_size ? candle.ohlc[candle.arr_size - 1].high : 0.0f,
		candle.arr_size ? candle.ohlc[candle.arr_size - 1].low : 0.0f,
		candle.arr_size ? candle.ohlc[candle.arr_size - 1].close : 0.0f);

	stw_candle_free(&candle);
	return rc;
}
