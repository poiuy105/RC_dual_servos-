idf_component_register(SRCS "ble_server.c"
                    PRIV_REQUIRES bt nvs_flash driver
                    INCLUDE_DIRS ".")
