# stw-ws-replay/
# ├── Makefile
# ├── README.md
# ├── include/
# │   └── stw/replay.h
# ├── src/
# │   ├── replay.c
# │   ├── parser.c
# │   ├── clock.c
# │   ├── ws_stub.c
# │   └── demo_main.c
# └── tests/
#     └── sample.log

# =====================
# Makefile
# =====================
# Notes:
# - Uses pkg-config for cJSON and libwebsockets if available.
# - Builds a static lib (libstwreplay.a) and two executables:
#     * wsreplay  — CLI log→callback runner (no websockets needed)
#     * demo      — minimal integration demo that runs your indicator callback chain
# - You can link libstwreplay.a into your big project and call stw_replay_run(...)

CC      := gcc
CSTD    := -std=c11
CFLAGS  := -O3 -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wdouble-promotion -fno-strict-aliasing
CFLAGS  += $(CSTD)
INCLUDES:= -Iinclude -I.
AR      := ar
ARFLAGS := rcs

# Optional deps via pkg-config (safe if missing)
PKGCFG  := $(shell pkg-config --silence-errors --cflags --libs libwebsockets 2>/dev/null)
LWS_OK  := $(if $(PKGCFG),1,0)
CJSONCF := $(shell pkg-config --silence-errors --cflags --libs cjson 2>/dev/null)

# If pkg-config not found, allow overriding CJSON/LWS from env
CFLAGS  += $(shell pkg-config --silence-errors --cflags cjson 2>/dev/null)
LDFLAGS := $(shell pkg-config --silence-errors --libs cjson 2>/dev/null)

# If you don’t have cJSON via pkg-config, compile with -lcjson manually or adjust here.

SRC := src/replay.c src/parser.c src/clock.c src/ws_stub.c
OBJ := $(SRC:.c=.o)
LIB := build/libstwreplay.a

BIN_DIR := build/bin
BIN1 := $(BIN_DIR)/wsreplay
BIN2 := $(BIN_DIR)/demo

DEMO_SRC := src/demo_main.c
DEMO_OBJ := $(DEMO_SRC:.c=.o)

all: $(LIB) $(BIN1) $(BIN2)

$(LIB): $(OBJ) | build
	$(AR) $(ARFLAGS) $@ $^

$(BIN1): src/replay.c src/parser.c src/clock.c src/ws_stub.c | build
	$(CC) $(CFLAGS) $(INCLUDES) -DSTW_REPLAY_BUILD_CLI -o $@ $^ $(LDFLAGS)

$(BIN2): $(DEMO_OBJ) $(LIB) | build
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $(DEMO_OBJ) $(LIB) $(LDFLAGS)

build:
	@mkdir -p build $(BIN_DIR)

clean:
	rm -rf build src/*.o $(OBJ) $(DEMO_OBJ)

.PHONY: all clean build

# =====================
# End of Makefile
# =====================