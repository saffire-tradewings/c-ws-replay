#include "stw/replay.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
Expected log shape (see your stdolog.c):
<ns> | <LEVEL> | <tid:pid> | <file:line> | [msg] <JSON>\n
We only accept LEVEL starting with 'WS'.
*/

static bool starts_with(const char* s, const char* p) { return strncmp(s, p, strlen(p)) == 0; }

/* Extract leading uint64 ns from start of line. Returns 0 on failure. */
static bool parse_ns_prefix(const char* line, uint64_t* out)
{
	char* end = NULL;
	unsigned long long v = strtoull(line, &end, 10);
	if (end == line) return false;
	if (out) *out = (uint64_t)v;
	return true;
}

/* Find the JSON after the literal "[msg] " */
static const char* find_json(const char* line)
{
	const char* p = strstr(line, "[msg]");
	if (!p) return NULL;
	p += strlen("[msg]");
	while (*p == ' ' || *p == '\t')
		++p;
	return (*p == '{' || *p == '[') ? p : NULL;
}

bool stw_parser_try_extract(const char* line, const char* filter, stw_log_frame_t* out)
{
	if (!line || !out) return false;

	// Quickly ensure "| WS" exists to avoid work
	const char* level_sep = strstr(line, "| WS");
	if (!level_sep) return false; // not WS level

	if (filter && *filter) {
		if (!strstr(line, filter)) return false;
	}

	uint64_t ns = 0;
	if (!parse_ns_prefix(line, &ns)) return false;

	const char* json = find_json(line);
	if (!json) return false;

	size_t len = strlen(json);
	// Trim trailing newlines/whitespace
	while (len > 0 && (json[len - 1] == '\n' || json[len - 1] == '\r' || json[len - 1] == ' ' ||
			   json[len - 1] == '\t'))
		len--;

	out->ns = ns;
	out->json = json;
	out->json_len = len;
	return true;
}

/* Expose as non-header function (internal linkage from replay.c) */
bool _stw_parser_try_extract(const char* line, const char* filter, stw_log_frame_t* out);
bool _stw_parser_try_extract(const char* line, const char* filter, stw_log_frame_t* out)
{
	return stw_parser_try_extract(line, filter, out);
}
