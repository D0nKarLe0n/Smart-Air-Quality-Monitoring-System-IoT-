idf_component_register(SRCS "rgr_main.c"
                    INCLUDE_DIRS "."
                    REQUIRES driver esp_adc nvs_flash esp_wifi esp_netif mqtt json)