idf_component_register(
    SRCS "src/ws2812_control.c"
    INCLUDE_DIRS "include"
    REQUIRES espressif__led_strip
)