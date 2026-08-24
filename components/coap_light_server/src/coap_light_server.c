#include <string.h>
#include "openthread/coap.h"
#include "esp_openthread.h"
#include "esp_openthread_lock.h"
#include "esp_log.h"
#include "cJSON.h"
#include "ws2812_control.h"

static const char *TAG = "coap_light";
static otCoapResource s_light_resource;

// Baut die aktuelle Zustands-Antwort als JSON: {"on":true,"r":255,"g":0,"b":0}
static void build_state_json(char *out, size_t out_len)
{
    bool on; uint8_t r, g, b;
    ws2812_control_get(&on, &r, &g, &b);
    cJSON *j = cJSON_CreateObject();
    cJSON_AddBoolToObject(j, "on", on);
    cJSON_AddNumberToObject(j, "r", r);
    cJSON_AddNumberToObject(j, "g", g);
    cJSON_AddNumberToObject(j, "b", b);
    char *str = cJSON_PrintUnformatted(j);
    strncpy(out, str, out_len - 1);
    out[out_len - 1] = '\0';
    free(str);
    cJSON_Delete(j);
}

static void send_response(otInstance *instance, otMessage *request,
                           const otMessageInfo *info, otCoapCode code,
                           const char *payload)
{
    otMessage *response = otCoapNewMessage(instance, NULL);
    if (!response) return;

    otCoapMessageInitResponse(response, request, OT_COAP_TYPE_ACKNOWLEDGMENT, code);

    if (payload) {
        otCoapMessageAppendContentFormatOption(response, OT_COAP_OPTION_CONTENT_FORMAT_JSON);
        otCoapMessageSetPayloadMarker(response);
        otMessageAppend(response, payload, strlen(payload));
    }

    otError err = otCoapSendResponse(instance, response, info);
    if (err != OT_ERROR_NONE) {
        ESP_LOGW(TAG, "Antwort senden fehlgeschlagen: %d", err);
        otMessageFree(response);
    }
}

static void light_handler(void *ctx, otMessage *message, const otMessageInfo *info)
{
    ESP_LOGI(TAG, "Received request, Code: %d", otCoapMessageGetCode(message));

    otInstance *instance = esp_openthread_get_instance();
    otCoapCode code = otCoapMessageGetCode(message);

    if (code == OT_COAP_CODE_GET) {
        char json[64];
        build_state_json(json, sizeof(json));
        send_response(instance, message, info, OT_COAP_CODE_CONTENT, json);
        return;
    }

    if (code == OT_COAP_CODE_PUT || code == OT_COAP_CODE_POST) {
        uint16_t offset = otMessageGetOffset(message);
        uint16_t length = otMessageGetLength(message) - offset;
        if (length == 0 || length >= 128) {
            send_response(instance, message, info, OT_COAP_CODE_BAD_REQUEST, NULL);
            return;
        }

        char body[128] = {0};
        otMessageRead(message, offset, body, length);

        cJSON *json = cJSON_Parse(body);
        if (!json) {
            send_response(instance, message, info, OT_COAP_CODE_BAD_REQUEST, NULL);
            return;
        }

        cJSON *on_item = cJSON_GetObjectItem(json, "on");
        cJSON *r_item = cJSON_GetObjectItem(json, "r");
        cJSON *g_item = cJSON_GetObjectItem(json, "g");
        cJSON *b_item = cJSON_GetObjectItem(json, "b");

        bool cur_on; uint8_t cur_r, cur_g, cur_b;
        ws2812_control_get(&cur_on, &cur_r, &cur_g, &cur_b);

        bool new_on = cJSON_IsBool(on_item) ? cJSON_IsTrue(on_item) : cur_on;
        uint8_t new_r = cJSON_IsNumber(r_item) ? (uint8_t)r_item->valueint : cur_r;
        uint8_t new_g = cJSON_IsNumber(g_item) ? (uint8_t)g_item->valueint : cur_g;
        uint8_t new_b = cJSON_IsNumber(b_item) ? (uint8_t)b_item->valueint : cur_b;

        cJSON_Delete(json);
        ws2812_control_set(new_on, new_r, new_g, new_b);

        char response_json[64];
        build_state_json(response_json, sizeof(response_json));
        send_response(instance, message, info, OT_COAP_CODE_CHANGED, response_json);
        return;
    }

    send_response(instance, message, info, OT_COAP_CODE_METHOD_NOT_ALLOWED, NULL);
}

void coap_light_server_init(void)
{
    otInstance *instance = esp_openthread_get_instance();

    esp_openthread_lock_acquire(portMAX_DELAY);
    otError err = otCoapStart(instance, OT_DEFAULT_COAP_PORT);
    if (err != OT_ERROR_NONE) {
        ESP_LOGE(TAG, "CoAP-Start fehlgeschlagen: %d", err);
        esp_openthread_lock_release();
        return;
    }

    s_light_resource.mUriPath = "light";
    s_light_resource.mHandler = light_handler;
    s_light_resource.mContext = NULL;
    s_light_resource.mNext = NULL;
    otCoapAddResource(instance, &s_light_resource);
    esp_openthread_lock_release();

    ESP_LOGI(TAG, "CoAP-Ressource '/light' registriert auf Port %d", OT_DEFAULT_COAP_PORT);
}