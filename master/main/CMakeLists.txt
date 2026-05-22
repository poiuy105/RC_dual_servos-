idf_component_register(SRCS "main.c" "ble_client.c"
                      INCLUDE_DIRS "."
                      PRIV_REQUIRES nvs_flash bt esp_timer driver)
