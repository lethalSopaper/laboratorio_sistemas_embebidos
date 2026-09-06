# idf_component_register(SRCS "main1.c"
idf_component_register(SRCS "main.c" "perifericos.c"
                       INCLUDE_DIRS "."
                       REQUIRES u8g2 u8g2-hal-esp-idf esp_adc esp_driver_ledc)