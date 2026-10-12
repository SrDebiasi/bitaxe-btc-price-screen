#include <inttypes.h>

#include "esp_log.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "global_state.h"
#include "price.h"
#include "price_task.h"

#define PRICE_URL "https://mempool.space/api/v1/prices"
#define PRICE_REFRESH_MS (5 * 60 * 1000)
#define PRICE_RETRY_MS (60 * 1000)
#define PRICE_TIMEOUT_MS 10000
// The real response is ~110 bytes; anything that doesn't fit is rejected.
#define PRICE_RESPONSE_MAX 512

static const char * TAG = "price";

static portMUX_TYPE price_lock = portMUX_INITIALIZER_UNLOCKED;
static uint32_t price_usd;
static uint32_t price_24h_usd;
static int64_t price_fetched_at_us;

// GETs `url` into `body`. Returns the body length, or -1 on any failure.
static int http_get(const char * url, char * body, size_t body_size)
{
    esp_http_client_config_t config = {
        .url = url,
        // Verify the server against the built-in CA bundle (same as stratum TLS).
        .crt_bundle_attach = esp_crt_bundle_attach,
        .timeout_ms = PRICE_TIMEOUT_MS,
        .disable_auto_redirect = true,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) {
        return -1;
    }

    int result = -1;

    if (esp_http_client_open(client, 0) == ESP_OK) {
        int64_t content_length = esp_http_client_fetch_headers(client);
        int status = esp_http_client_get_status_code(client);

        if (status == 200 && content_length >= 0 && content_length < (int64_t) body_size) {
            int len = esp_http_client_read_response(client, body, body_size);
            // A full buffer may be a truncated body, so it is rejected too.
            if (len > 0 && len < (int) body_size && esp_http_client_is_complete_data_received(client)) {
                result = len;
            }
        } else {
            ESP_LOGW(TAG, "Unexpected response: status %d, length %" PRId64, status, content_length);
        }
    }

    esp_http_client_cleanup(client);
    return result;
}

// Fetches the current price and, when that works, the price 24h ago.
// The 24h price is optional: *usd_24h stays 0 when it can't be fetched.
static bool fetch_prices(uint32_t * usd, uint32_t * usd_24h)
{
    char body[PRICE_RESPONSE_MAX];
    *usd_24h = 0;

    int len = http_get(PRICE_URL, body, sizeof(body));
    if (len <= 0 || !price_parse_usd(body, len, usd)) {
        return false;
    }

    int64_t server_time_s;
    char url[128];
    if (!price_parse_time(body, len, &server_time_s) || !price_history_url(server_time_s, url, sizeof(url))) {
        return true;
    }

    len = http_get(url, body, sizeof(body));
    if (len <= 0 || !price_parse_history_usd(body, len, usd_24h)) {
        ESP_LOGW(TAG, "24h price fetch failed");
    }

    return true;
}

void price_task(void * pvParameters)
{
    GlobalState * GLOBAL_STATE = (GlobalState *) pvParameters;

    while (1) {
        uint32_t delay_ms = PRICE_RETRY_MS;

        if (GLOBAL_STATE->SYSTEM_MODULE.is_connected) {
            uint32_t usd, usd_24h;
            if (fetch_prices(&usd, &usd_24h)) {
                taskENTER_CRITICAL(&price_lock);
                price_usd = usd;
                price_24h_usd = usd_24h;
                price_fetched_at_us = esp_timer_get_time();
                taskEXIT_CRITICAL(&price_lock);
                delay_ms = PRICE_REFRESH_MS;
            } else {
                ESP_LOGW(TAG, "Price fetch failed, retrying in %d s", PRICE_RETRY_MS / 1000);
            }
        }

        vTaskDelay(pdMS_TO_TICKS(delay_ms));
    }
}

bool price_task_get_usd(uint32_t * usd, uint32_t * usd_24h)
{
    int64_t now_us = esp_timer_get_time();

    taskENTER_CRITICAL(&price_lock);
    bool fresh = price_is_fresh(price_fetched_at_us, now_us);
    if (fresh) {
        *usd = price_usd;
        *usd_24h = price_24h_usd;
    }
    taskEXIT_CRITICAL(&price_lock);

    return fresh;
}
