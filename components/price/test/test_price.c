#include "unity.h"
#include "price.h"
#include <string.h>

#define MEMPOOL_RESPONSE "{\"time\":1791825908,\"USD\":82470,\"EUR\":73904,\"GBP\":62555,\"CAD\":117912,\"CHF\":68867,\"AUD\":118628,\"JPY\":13123544}"

static bool parse(const char * json, uint32_t * usd)
{
    return price_parse_usd(json, strlen(json), usd);
}

TEST_CASE("price_parse_usd reads USD from a mempool.space response", "[price]")
{
    uint32_t usd = 0;
    TEST_ASSERT_TRUE(parse(MEMPOOL_RESPONSE, &usd));
    TEST_ASSERT_EQUAL_UINT32(82470, usd);
}

TEST_CASE("price_parse_usd rounds a fractional price", "[price]")
{
    uint32_t usd = 0;
    TEST_ASSERT_TRUE(parse("{\"USD\":82470.6}", &usd));
    TEST_ASSERT_EQUAL_UINT32(82471, usd);
    TEST_ASSERT_TRUE(parse("{\"USD\":82470.4}", &usd));
    TEST_ASSERT_EQUAL_UINT32(82470, usd);
}

TEST_CASE("price_parse_usd accepts the bounds", "[price]")
{
    uint32_t usd = 0;
    TEST_ASSERT_TRUE(parse("{\"USD\":1}", &usd));
    TEST_ASSERT_EQUAL_UINT32(PRICE_USD_MIN, usd);
    TEST_ASSERT_TRUE(parse("{\"USD\":100000000}", &usd));
    TEST_ASSERT_EQUAL_UINT32(PRICE_USD_MAX, usd);
}

TEST_CASE("price_parse_usd rejects implausible prices", "[price]")
{
    uint32_t usd = 42;
    TEST_ASSERT_FALSE(parse("{\"USD\":0}", &usd));
    TEST_ASSERT_FALSE(parse("{\"USD\":0.4}", &usd));
    TEST_ASSERT_FALSE(parse("{\"USD\":-82470}", &usd));
    TEST_ASSERT_FALSE(parse("{\"USD\":100000001}", &usd));
    TEST_ASSERT_FALSE(parse("{\"USD\":1e300}", &usd));
    TEST_ASSERT_EQUAL_UINT32(42, usd);
}

TEST_CASE("price_parse_usd rejects a missing or non-numeric USD", "[price]")
{
    uint32_t usd = 42;
    TEST_ASSERT_FALSE(parse("{\"EUR\":73904}", &usd));
    TEST_ASSERT_FALSE(parse("{\"USD\":\"82470\"}", &usd));
    TEST_ASSERT_FALSE(parse("{\"USD\":null}", &usd));
    TEST_ASSERT_FALSE(parse("{\"USD\":{\"price\":82470}}", &usd));
    TEST_ASSERT_FALSE(parse("[82470]", &usd));
    TEST_ASSERT_FALSE(parse("82470", &usd));
    TEST_ASSERT_EQUAL_UINT32(42, usd);
}

TEST_CASE("price_parse_usd rejects malformed input", "[price]")
{
    uint32_t usd = 42;
    TEST_ASSERT_FALSE(parse("", &usd));
    TEST_ASSERT_FALSE(parse("{\"USD\":82470", &usd));
    TEST_ASSERT_FALSE(parse("<html>502 Bad Gateway</html>", &usd));
    TEST_ASSERT_FALSE(price_parse_usd(NULL, 10, &usd));
    TEST_ASSERT_FALSE(price_parse_usd(MEMPOOL_RESPONSE, strlen(MEMPOOL_RESPONSE), NULL));
    TEST_ASSERT_EQUAL_UINT32(42, usd);
}

TEST_CASE("price_parse_usd reads only len bytes", "[price]")
{
    // Not NUL-terminated after the cut: a truncated body must not parse.
    const char body[] = "{\"USD\":82470}trailing";
    uint32_t usd = 42;
    TEST_ASSERT_FALSE(price_parse_usd(body, 10, &usd));
    TEST_ASSERT_EQUAL_UINT32(42, usd);
    TEST_ASSERT_TRUE(price_parse_usd(body, 13, &usd));
    TEST_ASSERT_EQUAL_UINT32(82470, usd);
}

TEST_CASE("price_format_usd adds thousands separators", "[price]")
{
    char buf[16];
    price_format_usd(7, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("$7", buf);
    price_format_usd(999, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("$999", buf);
    price_format_usd(1000, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("$1,000", buf);
    price_format_usd(82470, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("$82,470", buf);
    price_format_usd(112340, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("$112,340", buf);
    price_format_usd(1234567, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("$1,234,567", buf);
    price_format_usd(UINT32_MAX, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("$4,294,967,295", buf);
}

TEST_CASE("price_format_usd truncates to the buffer", "[price]")
{
    char buf[5];
    memset(buf, 'x', sizeof(buf));
    price_format_usd(82470, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("$82,", buf);

    char one[1] = {'x'};
    price_format_usd(82470, one, sizeof(one));
    TEST_ASSERT_EQUAL_CHAR('\0', one[0]);
}

TEST_CASE("price_is_fresh expires after the max age", "[price]")
{
    const int64_t fetched = 5000000;
    TEST_ASSERT_FALSE(price_is_fresh(0, 60000000));
    TEST_ASSERT_TRUE(price_is_fresh(fetched, fetched));
    TEST_ASSERT_TRUE(price_is_fresh(fetched, fetched + PRICE_MAX_AGE_US));
    TEST_ASSERT_FALSE(price_is_fresh(fetched, fetched + PRICE_MAX_AGE_US + 1));
    TEST_ASSERT_FALSE(price_is_fresh(fetched, fetched - 1));
}

#define HISTORY_RESPONSE "{\"prices\":[{\"time\":1791741600,\"USD\":83756}],\"exchangeRates\":{\"USDEUR\":0.9,\"USDGBP\":0.76,\"USDCAD\":1.43,\"USDCHF\":0.83,\"USDAUD\":1.44,\"USDJPY\":158.94}}"

static bool parse_time(const char * json, int64_t * time_s)
{
    return price_parse_time(json, strlen(json), time_s);
}

static bool parse_history(const char * json, uint32_t * usd)
{
    return price_parse_history_usd(json, strlen(json), usd);
}

TEST_CASE("price_parse_time reads the server time", "[price]")
{
    int64_t time_s = 0;
    TEST_ASSERT_TRUE(parse_time(MEMPOOL_RESPONSE, &time_s));
    TEST_ASSERT_TRUE(time_s == 1791825908LL); // Unity here has no 64-bit asserts
}

TEST_CASE("price_parse_time rejects a missing or implausible time", "[price]")
{
    int64_t time_s = 42;
    TEST_ASSERT_FALSE(parse_time("{\"USD\":82470}", &time_s));
    TEST_ASSERT_FALSE(parse_time("{\"time\":\"1791825908\"}", &time_s));
    TEST_ASSERT_FALSE(parse_time("{\"time\":0}", &time_s));
    TEST_ASSERT_FALSE(parse_time("{\"time\":-1791825908}", &time_s));
    TEST_ASSERT_FALSE(parse_time("{\"time\":1e300}", &time_s));
    TEST_ASSERT_FALSE(parse_time("{\"time\":1791825908", &time_s));
    TEST_ASSERT_FALSE(price_parse_time(NULL, 10, &time_s));
    TEST_ASSERT_FALSE(price_parse_time(MEMPOOL_RESPONSE, strlen(MEMPOOL_RESPONSE), NULL));
    TEST_ASSERT_TRUE(time_s == 42);
}

TEST_CASE("price_history_url asks for the price 24h before the server time", "[price]")
{
    char url[128];
    TEST_ASSERT_TRUE(price_history_url(1791825908, url, sizeof(url)));
    TEST_ASSERT_EQUAL_STRING("https://mempool.space/api/v1/historical-price?currency=USD&timestamp=1791739508", url);
}

TEST_CASE("price_history_url rejects an implausible time or a short buffer", "[price]")
{
    char url[128] = "untouched";
    TEST_ASSERT_FALSE(price_history_url(0, url, sizeof(url)));
    TEST_ASSERT_FALSE(price_history_url(-1791825908, url, sizeof(url)));
    TEST_ASSERT_FALSE(price_history_url(INT64_MAX, url, sizeof(url)));

    char small[20];
    TEST_ASSERT_FALSE(price_history_url(1791825908, small, sizeof(small)));
}

TEST_CASE("price_parse_history_usd reads USD from a historical-price response", "[price]")
{
    uint32_t usd = 0;
    TEST_ASSERT_TRUE(parse_history(HISTORY_RESPONSE, &usd));
    TEST_ASSERT_EQUAL_UINT32(83756, usd);
}

TEST_CASE("price_parse_history_usd rejects bad responses", "[price]")
{
    uint32_t usd = 42;
    TEST_ASSERT_FALSE(parse_history("{\"prices\":[]}", &usd));
    TEST_ASSERT_FALSE(parse_history("{\"prices\":[{\"time\":1791741600}]}", &usd));
    TEST_ASSERT_FALSE(parse_history("{\"prices\":[{\"USD\":\"83756\"}]}", &usd));
    TEST_ASSERT_FALSE(parse_history("{\"prices\":[{\"USD\":0}]}", &usd));
    TEST_ASSERT_FALSE(parse_history("{\"prices\":[{\"USD\":100000001}]}", &usd));
    TEST_ASSERT_FALSE(parse_history("{\"prices\":{\"USD\":83756}}", &usd));
    TEST_ASSERT_FALSE(parse_history(MEMPOOL_RESPONSE, &usd));
    TEST_ASSERT_FALSE(parse_history("{\"prices\":[{\"USD\":83756}]", &usd));
    TEST_ASSERT_FALSE(price_parse_history_usd(NULL, 10, &usd));
    TEST_ASSERT_FALSE(price_parse_history_usd(HISTORY_RESPONSE, strlen(HISTORY_RESPONSE), NULL));
    TEST_ASSERT_EQUAL_UINT32(42, usd);
}

TEST_CASE("price_format_change shows an arrow and the 24h percent", "[price]")
{
    char buf[24];
    price_format_change(102, 100, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("↑ 2.00%", buf);
    price_format_change(98, 100, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("↓ 2.00%", buf);
    price_format_change(82380, 83756, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("↓ 1.64%", buf);
    price_format_change(85549, 83756, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("↑ 2.14%", buf);
    price_format_change(200, 100, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("↑ 100.00%", buf);
    price_format_change(PRICE_USD_MAX, PRICE_USD_MIN, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("↑ 9999999900.00%", buf);
}

TEST_CASE("price_format_change shows no arrow when flat", "[price]")
{
    char buf[24];
    price_format_change(100, 100, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("0.00%", buf);
    // +/-0.004% rounds to zero: no arrow and no "-0.00%".
    price_format_change(100004, 100000, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("0.00%", buf);
    price_format_change(99996, 100000, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("0.00%", buf);
}

TEST_CASE("price_format_change is empty without a 24h price", "[price]")
{
    char buf[24] = "stale";
    price_format_change(82380, 0, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("", buf);
    strcpy(buf, "stale");
    price_format_change(0, 83756, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("", buf);

    char one[1] = {'x'};
    price_format_change(102, 100, one, sizeof(one));
    TEST_ASSERT_EQUAL_CHAR('\0', one[0]);
}
