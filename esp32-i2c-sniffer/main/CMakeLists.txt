idf_component_register(SRCS "main.c" "i2c_sniffer.c" "trace_output.c"
                    INCLUDE_DIRS "."
                    REQUIRES driver esp_common freertos)
