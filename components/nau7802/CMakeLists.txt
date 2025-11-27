idf_component_register(SRCS "nau7802.c" "nau7802_calibration_storage.c"
                    INCLUDE_DIRS "include"
                    PRIV_REQUIRES driver
                    REQUIRES nvs_flash)

