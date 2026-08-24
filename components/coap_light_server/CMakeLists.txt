idf_component_register(
    SRCS "src/coap_light_server.c"
    INCLUDE_DIRS "include"
    REQUIRES openthread ws2812_control espressif__cjson
)