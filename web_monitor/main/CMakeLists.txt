idf_component_register(SRCS "ws.c" "softap.c" "dht11.c" "main.c"
                    INCLUDE_DIRS ".")

spiffs_create_partition_image(html ../esp.html FLASH_IN_PROJECT)
