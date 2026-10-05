idf_component_register(SRCS "5_Servidor_Web.c"
                       INCLUDE_DIRS "."
                       REQUIRES esp_http_server esp_wifi nvs_flash driver
                       EMBED_FILES "index.html")