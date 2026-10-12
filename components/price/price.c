#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "cJSON.h"
#include "price.h"

#define PRICE_HISTORY_URL "https://mempool.space/api/v1/historical-price?currency=USD&timestamp="
#define PRICE_DAY_S (24 * 60 * 60)

static bool usd_from_item(const cJSON * item, uint32_t * usd)
{
    if (!cJSON_IsNumber(item)) {
        return false;
    }

    double rounded = round(item->valuedouble);
    // Bounds are checked on the double so the cast below is always defined.
    if (rounded < PRICE_USD_MIN || rounded > PRICE_USD_MAX) {
        return false;
    }

    *usd = (uint32_t) rounded;
    return true;
}

bool price_parse_usd(const char * json, size_t len, uint32_t * usd)
{
    if (json == NULL || usd == NULL || len == 0) {
        return false;
    }

    cJSON * root = cJSON_ParseWithLength(json, len);
    if (root == NULL) {
        return false;
    }

    bool ok = usd_from_item(cJSON_GetObjectItemCaseSensitive(root, "USD"), usd);

    cJSON_Delete(root);
    return ok;
}

void price_format_usd(uint32_t usd, char * buf, size_t buf_size)
{
    if (buf_size == 0) {
        return;
    }

    char digits[11];
    int n = snprintf(digits, sizeof(digits), "%" PRIu32, usd);

    // "$" + 10 digits + 3 separators + NUL
    char out[15];
    size_t o = 0;
    out[o++] = '$';
    for (int i = 0; i < n; i++) {
        if (i > 0 && (n - i) % 3 == 0) {
            out[o++] = ',';
        }
        out[o++] = digits[i];
    }
    out[o] = '\0';

    snprintf(buf, buf_size, "%s", out);
}

bool price_is_fresh(int64_t fetched_at_us, int64_t now_us)
{
    if (fetched_at_us == 0 || now_us < fetched_at_us) {
        return false;
    }

    return now_us - fetched_at_us <= PRICE_MAX_AGE_US;
}

bool price_parse_time(const char * json, size_t len, int64_t * time_s)
{
    if (json == NULL || time_s == NULL || len == 0) {
        return false;
    }

    cJSON * root = cJSON_ParseWithLength(json, len);
    if (root == NULL) {
        return false;
    }

    bool ok = false;
    const cJSON * item = cJSON_GetObjectItemCaseSensitive(root, "time");
    if (cJSON_IsNumber(item) && item->valuedouble >= PRICE_TIME_MIN && item->valuedouble <= PRICE_TIME_MAX) {
        *time_s = (int64_t) item->valuedouble;
        ok = true;
    }

    cJSON_Delete(root);
    return ok;
}

bool price_history_url(int64_t now_s, char * buf, size_t buf_size)
{
    if (now_s < PRICE_TIME_MIN || now_s > PRICE_TIME_MAX) {
        return false;
    }

    int n = snprintf(buf, buf_size, PRICE_HISTORY_URL "%" PRId64, now_s - PRICE_DAY_S);
    return n > 0 && (size_t) n < buf_size;
}

bool price_parse_history_usd(const char * json, size_t len, uint32_t * usd)
{
    if (json == NULL || usd == NULL || len == 0) {
        return false;
    }

    cJSON * root = cJSON_ParseWithLength(json, len);
    if (root == NULL) {
        return false;
    }

    const cJSON * prices = cJSON_GetObjectItemCaseSensitive(root, "prices");
    bool ok = cJSON_IsArray(prices)
        && usd_from_item(cJSON_GetObjectItemCaseSensitive(cJSON_GetArrayItem(prices, 0), "USD"), usd);

    cJSON_Delete(root);
    return ok;
}

void price_format_change(uint32_t now_usd, uint32_t then_usd, char * buf, size_t buf_size)
{
    if (buf_size == 0) {
        return;
    }

    if (now_usd == 0 || then_usd == 0) {
        buf[0] = '\0';
        return;
    }

    // Round to hundredths of a percent first, so the sign and the arrow agree
    // with what is shown: no "↓ 0.00%" and no "-0.00%".
    double percent = ((double) now_usd - (double) then_usd) * 100.0 / then_usd;
    long long hundredths = llround(percent * 100.0);

    if (hundredths == 0) {
        snprintf(buf, buf_size, "0.00%%");
        return;
    }

    const char * arrow = hundredths > 0 ? "↑" : "↓";
    long long magnitude = llabs(hundredths);
    snprintf(buf, buf_size, "%s %lld.%02lld%%", arrow, magnitude / 100, magnitude % 100);
}
