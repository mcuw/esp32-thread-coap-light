#include "openthread/joiner.h"
#include "openthread/thread.h"
#include "openthread/dataset.h"
#include "esp_openthread.h"
#include "esp_openthread_lock.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "auto_joiner.h"

static const char *TAG = "auto_joiner";

// IMPORTANT: has to be equal to the PSKd, that is entered in the Border-Router UI
// in the Commissioner-Panel (default there: "J01NME")
#define JOINER_PSKD "J01NME"
#define RETRY_INTERVAL_US (15 * 1000 * 1000)  // 15 Sekunden

static esp_timer_handle_t s_retry_timer;

static void start_joiner_attempt(void);

static void joiner_callback(otError error, void *ctx)
{
    otInstance *instance = esp_openthread_get_instance();

    if (error == OT_ERROR_NONE) {
        ESP_LOGI(TAG, "Join successful - starting Thread");
        esp_openthread_lock_acquire(portMAX_DELAY);
        otThreadSetEnabled(instance, true);
        esp_openthread_lock_release();
        // No further retry needed - Auto-Attach in main.c will take over
        // from now on at every future boot, as the dataset is now persisted.
    } else {
        ESP_LOGW(TAG, "Join failed (%d) - next attempt in 15s", error);
        esp_timer_start_once(s_retry_timer, RETRY_INTERVAL_US);
    }
}

static void start_joiner_attempt(void)
{
    otInstance *instance = esp_openthread_get_instance();

    esp_openthread_lock_acquire(portMAX_DELAY);
    otIp6SetEnabled(instance, true);
    otError err = otJoinerStart(instance, JOINER_PSKD, NULL, NULL, NULL, NULL, NULL,
                                 joiner_callback, NULL);
    esp_openthread_lock_release();

    if (err != OT_ERROR_NONE) {
        ESP_LOGW(TAG, "otJoinerStart failed: %d - next attempt in 15s", err);
        esp_timer_start_once(s_retry_timer, RETRY_INTERVAL_US);
    } else {
        ESP_LOGI(TAG, "Joiner attempt started (PSKd: %s) - waiting for Commissioner approval", JOINER_PSKD);
    }
}

static void retry_timer_cb(void *arg)
{
    start_joiner_attempt();
}

void auto_joiner_init(void)
{
    otInstance *instance = esp_openthread_get_instance();

    esp_openthread_lock_acquire(portMAX_DELAY);
    bool commissioned = otDatasetIsCommissioned(instance);
    esp_openthread_lock_release();

    if (commissioned) {
        ESP_LOGI(TAG, "Already commissioned - Auto-Joiner not needed");
        return;
    }

    esp_timer_create_args_t timer_args = {
        .callback = retry_timer_cb,
        .name = "joiner_retry",
    };
    esp_timer_create(&timer_args, &s_retry_timer);

    ESP_LOGI(TAG, "Dataset does not exist - start trying to auto join");
    start_joiner_attempt();
}