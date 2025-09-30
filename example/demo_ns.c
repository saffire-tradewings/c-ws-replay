/* Demo variant showing how to build candles at arbitrary ns/µs/ms/minute bins. */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "stw/replay.h"
#include <stw/candle.h>
#include <stw/json.h>
#include <stw/time.h>

/* Simple OHLC state */
static stw_ohlc_t ohlc = {0.0f, 0.0f, 0.0f, 0.0f};

/* Candle buffers for multiple TFs */
static stw_candle_ns_t candle_500us;
static stw_candle_ns_t candle_500ms;
static stw_candle_ns_t candle_1s;
static stw_candle_ns_t candle_3m;

/* Convert seconds + ltp into OHLC updates */
static void update_ohlc(float price)
{
	if (ohlc.open == 0.0f) {
		ohlc.open = ohlc.high = ohlc.low = ohlc.close = price;
	} else {
		ohlc.close = price;
		if (price > ohlc.high) ohlc.high = price;
		if (price < ohlc.low) ohlc.low = price;
	}
}

static void cb_receive_ns(void* wsi, void* user, void* in, size_t len)
{
	(void)wsi;
	(void)user;
	const char* payload = (const char*)in;

	cJSON* json = cJSON_ParseWithLengthOpts(payload, len, NULL, 0);
	if (!json) return;
	const cJSON* resp = cJSON_GetObjectItemCaseSensitive(json, "response");
	if (resp) {
		const cJSON* btime = cJSON_GetObjectItemCaseSensitive(resp, "BCastTime");
		const cJSON* data = cJSON_GetObjectItemCaseSensitive(resp, "data");
		if (btime && data) {
			uint64_t ts_s = strtoull(btime->valuestring, NULL, 10);
			const cJSON* ltp = cJSON_GetObjectItemCaseSensitive(data, "ltp");
			if (ltp && ltp->valuestring) {
				float price = (float)atof(ltp->valuestring);
				update_ohlc(price);
				uint64_t ts_ns = ts_s * 1000000000ull;
				stw_append_candle_ns(&candle_500us, &ohlc, ts_ns, 500 * 1000ull);
				stw_append_candle_ns(&candle_500ms, &ohlc, ts_ns, 500 * 1000000ull);
				stw_append_candle_ns(&candle_1s, &ohlc, ts_ns, 1000000000ull);
				stw_append_candle_ns(&candle_3m, &ohlc, ts_ns, 180 * 1000000000ull);
			}
		}
	}
	cJSON_Delete(json);
}

static void adapter_cb(void* user, const char* json, size_t len)
{
	(void)user;
	cb_receive_ns(NULL, NULL, (void*)json, len);
}

int main(int argc, char** argv)
{
	if (argc < 2) {
		fprintf(stderr, "Usage: %s <logfile>\n", argv[0]);
		return 1;
	}
	stw_replay_opts_t opt = {0};
	opt.logfile = argv[1];
	opt.speed = 1.0;

	stw_candle_ns_alloc(&candle_500us, 4096);
	stw_candle_ns_alloc(&candle_500ms, 4096);
	stw_candle_ns_alloc(&candle_1s, 4096);
	stw_candle_ns_alloc(&candle_3m, 4096);

	stw_replay_run_simple(&opt, adapter_cb, NULL);

	fprintf(stderr, "500us candles=%zu, 500ms=%zu, 1s=%zu, 3m=%zu\n", candle_500us.arr_size,
		candle_500ms.arr_size, candle_1s.arr_size, candle_3m.arr_size);

	stw_candle_ns_free(&candle_500us);
	stw_candle_ns_free(&candle_500ms);
	stw_candle_ns_free(&candle_1s);
	stw_candle_ns_free(&candle_3m);
	return 0;
}

/*

# =====================
# Makefile additions
# =====================
# Add demo_ns target
BIN3 := $(BIN_DIR)/demo_ns
DEMO_NS_SRC := src/demo_ns.c
DEMO_NS_OBJ := $(DEMO_NS_SRC:.c=.o)

all: $(LIB) $(BIN1) $(BIN2) $(BIN3)

$(BIN3): $(DEMO_NS_OBJ) $(LIB) | build
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $(DEMO_NS_OBJ) $(LIB) $(LDFLAGS)

clean:
	rm -rf build src/*.o $(OBJ) $(DEMO_OBJ) $(DEMO_NS_OBJ)


*/
