#ifndef PRICE_H_
#define PRICE_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Sanity bounds for a BTC/USD price in whole dollars. Anything outside is
// treated as a bad response rather than shown on the display.
#define PRICE_USD_MIN 1
#define PRICE_USD_MAX 100000000

// A price older than this is not shown, so a dead connection can't leave a
// stale number on screen.
#define PRICE_MAX_AGE_US (30LL * 60 * 1000000)

// Parses a mempool.space /api/v1/prices response, e.g.
// {"time":1791825908,"USD":82470,"EUR":73904,...}
// `json` need not be NUL-terminated; only `len` bytes are read.
// Returns true and writes whole dollars to *usd only for a plausible price;
// *usd is left untouched on failure.
bool price_parse_usd(const char * json, size_t len, uint32_t * usd);

// Formats whole dollars with thousands separators: 82470 -> "$82,470".
// Always NUL-terminates (truncating) when buf_size > 0.
void price_format_usd(uint32_t usd, char * buf, size_t buf_size);

// True when a price fetched at `fetched_at_us` is still fresh at `now_us`.
// fetched_at_us == 0 means no price has been fetched yet.
bool price_is_fresh(int64_t fetched_at_us, int64_t now_us);

// Sanity bounds for the server's unix time (2023-11 .. 2100-01).
#define PRICE_TIME_MIN 1700000000LL
#define PRICE_TIME_MAX 4102444800LL

// Reads the server's unix time ("time") from a /api/v1/prices response, so
// the 24h-ago query doesn't depend on the miner's clock.
bool price_parse_time(const char * json, size_t len, int64_t * time_s);

// Builds the mempool.space historical-price URL for 24h before `now_s`.
// Returns false for an implausible time or a buffer that is too small.
bool price_history_url(int64_t now_s, char * buf, size_t buf_size);

// Parses a /api/v1/historical-price response, e.g.
// {"prices":[{"time":1791741600,"USD":83756}],"exchangeRates":{...}}
// Same bounds and contract as price_parse_usd.
bool price_parse_history_usd(const char * json, size_t len, uint32_t * usd);

// Formats the change from `then_usd` to `now_usd`: "↑ 2.14%", "↓ 1.64%", or
// "0.00%" when it rounds to zero. Empty when either price is 0 (unknown).
void price_format_change(uint32_t now_usd, uint32_t then_usd, char * buf, size_t buf_size);

#endif /* PRICE_H_ */
