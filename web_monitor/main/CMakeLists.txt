idf_component_register(SRCS "ws.c" "softap.c" "dht11.c" "main.c" "led_ws2812.c"
                    INCLUDE_DIRS ".")

spiffs_create_partition_image(html ../html FLASH_IN_PROJECT)
