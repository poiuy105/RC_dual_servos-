idf_component_register(SRCS "main.c" "ble_server.c"
                      INCLUDE_DIRS "."
                      PRIV_REQUIRES nvs_flash bt driver)
