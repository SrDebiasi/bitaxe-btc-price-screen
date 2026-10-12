#ifndef PRICE_TASK_H_
#define PRICE_TASK_H_

#include <stdbool.h>
#include <stdint.h>

// Fetches the BTC/USD price, and the price 24h ago, from mempool.space over
// verified HTTPS every few minutes, for the display. Read-only: no settings,
// no API, no NVS.
void price_task(void * pvParameters);

// True and *usd set when a fresh price is available. *usd_24h is the price
// 24h earlier, or 0 when it couldn't be fetched.
bool price_task_get_usd(uint32_t * usd, uint32_t * usd_24h);

#endif /* PRICE_TASK_H_ */
