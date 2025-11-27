idf_component_register(SRCS "system_config.c"
                            "mcp_config.c"
                            "i2c_config.c"
                    INCLUDE_DIRS "include"
                    PRIV_REQUIRES nvs_flash lwip)

