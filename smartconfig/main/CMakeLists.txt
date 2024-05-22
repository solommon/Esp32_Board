idf_component_register(SRCS "wifi_smartconfig.c" "main.c" "button.c"
                    INCLUDE_DIRS ".")


target_compile_options(${COMPONENT_LIB} PUBLIC "-Wno-format-truncation")