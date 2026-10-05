idf_component_register(SRCS "main.c"
                       INCLUDE_DIRS "."
                       EMBED_TXTFILES "index.html"
                       PRIV_REQUIRES driver esp_http_server nvs_flash esp_wifi esp_netif esp_adc u8g2)