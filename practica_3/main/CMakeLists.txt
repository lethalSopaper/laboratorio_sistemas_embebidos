idf_component_register(SRCS "main.c" "grafico.c"
                       INCLUDE_DIRS "."
                       REQUIRES esp_adc
                                u8g2
                                u8g2-hal-esp-idf)
