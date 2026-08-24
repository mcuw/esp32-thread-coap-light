/*
 * SPDX-FileCopyrightText: 2021-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 *
 * OpenThread Command Line Example
 *
 * This example code is in the Public Domain (or CC0 licensed, at your option.)
 *
 * Unless required by applicable law or agreed to in writing, this
 * software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
 * CONDITIONS OF ANY KIND, either express or implied.
*/

#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include "sdkconfig.h"
#include "esp_err.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_netif_types.h"
#include "esp_openthread.h"
#include "esp_openthread_lock.h"
#include "esp_openthread_netif_glue.h"
#include "esp_openthread_types.h"
#include "esp_openthread_lock.h"
#include "esp_ot_config.h"
#include "esp_vfs_eventfd.h"
#include "nvs_flash.h"
#include "ot_examples_common.h"
#include "openthread/thread.h"
#include "openthread/dataset.h"

// WS2812 control component
#include "ws2812_control.h"

// CoAP light server component
#include "coap_light_server.h"

// auto-joiner component
#include "auto_joiner.h"

#if CONFIG_OPENTHREAD_STATE_INDICATOR_ENABLE
#include "ot_led_strip.h"
#endif

#if CONFIG_OPENTHREAD_CLI_ESP_EXTENSION
#include "esp_ot_cli_extension.h"
#endif // CONFIG_OPENTHREAD_CLI_ESP_EXTENSION

#if CONFIG_ESP_COEX_EXTERNAL_COEXIST_ENABLE
#include "ext_coex_cmd.h"
#endif

#define TAG "ot_esp_cli"

void app_main(void)
{
    // Used eventfds:
    // * netif
    // * ot task queue
    // * radio driver
    esp_vfs_eventfd_config_t eventfd_config = {
        .max_fds = 3,
    };

    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
#if CONFIG_OPENTHREAD_PLATFORM_NETIF
    ESP_ERROR_CHECK(esp_netif_init());
#endif
    ESP_ERROR_CHECK(esp_vfs_eventfd_register(&eventfd_config));

#if CONFIG_OPENTHREAD_CLI
    ot_console_start();
    ot_register_external_commands();
#if CONFIG_ESP_COEX_EXTERNAL_COEXIST_ENABLE
    register_cmd_extcoex();
#endif
#endif

    static esp_openthread_config_t config = {
#if CONFIG_OPENTHREAD_PLATFORM_NETIF
        .netif_config = ESP_NETIF_DEFAULT_OPENTHREAD(),
#endif
        .platform_config = {
            .radio_config = ESP_OPENTHREAD_DEFAULT_RADIO_CONFIG(),
            .host_config = ESP_OPENTHREAD_DEFAULT_HOST_CONFIG(),
            .port_config = ESP_OPENTHREAD_DEFAULT_PORT_CONFIG(),
        },
    };

    ESP_ERROR_CHECK(esp_openthread_start(&config));

    // auto start the OpenThread network if enabled in menuconfig
    #if CONFIG_OPENTHREAD_CLI_ESP_EXTENSION
        esp_cli_custom_command_init();
    #endif

    // Auto-Attach: Falls bereits ein Dataset aus einer frueheren Session
    // persistiert ist, automatisch dem Netzwerk beitreten - kein manuelles
    // ifconfig/joiner/thread-start mehr noetig nach einem Reboot.
    if (esp_openthread_lock_acquire(pdMS_TO_TICKS(1000))) {
        otInstance *instance = esp_openthread_get_instance();
        if (otDatasetIsCommissioned(instance)) {
            otError err1 = otIp6SetEnabled(instance, true);
            otError err2 = otThreadSetEnabled(instance, true);
            if (err1 == OT_ERROR_NONE && err2 == OT_ERROR_NONE) {
                ESP_LOGI(TAG, "Auto-attach: persistiertes Dataset gefunden, Thread gestartet");
            } else {
                ESP_LOGW(TAG, "Auto-attach fehlgeschlagen (ip6: %d, thread: %d)", err1, err2);
            }
        } else {
            auto_joiner_init();
        }
        esp_openthread_lock_release();
    }

    //
    // CoAP light server
    // to control WS2812 RGB LED strip via CoAP commands
    //
    ws2812_control_init();
    coap_light_server_init();

#if CONFIG_OPENTHREAD_CLI_ESP_EXTENSION
    esp_cli_custom_command_init();
#endif
#if CONFIG_OPENTHREAD_STATE_INDICATOR_ENABLE
    ESP_ERROR_CHECK(esp_openthread_state_indicator_init(esp_openthread_get_instance()));
#endif
#if CONFIG_OPENTHREAD_NETWORK_AUTO_START
    ot_network_auto_start();
#endif
}
