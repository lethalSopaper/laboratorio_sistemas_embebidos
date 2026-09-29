#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c.h"
#include "u8g2.h"
#include "esp_rom_sys.h"
#include <esp_http_server.h>
#include "nvs_flash.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_adc/adc_oneshot.h" // Librería para el potenciómetro

// Punteros a la página web incrustada
extern const uint8_t index_html_start[] asm("_binary_index_html_start");
extern const uint8_t index_html_end[]   asm("_binary_index_html_end");

// Configuración de pines para el bus I2C
#define I2C_MASTER_SCL_IO           44
#define I2C_MASTER_SDA_IO           7
#define I2C_MASTER_NUM              I2C_NUM_0
#define I2C_MASTER_FREQ_HZ          400000
#define WIFI_SSID "TP-Link_F271"
#define WIFI_PASS "036A410E"

static i2c_cmd_handle_t handle_i2c;
u8g2_t u8g2;

// Variables globales para el servidor y el potenciómetro
httpd_handle_t servidor_global = NULL;
adc_oneshot_unit_handle_t adc1_handle;

// --- PUENTES DE U8G2 ---
uint8_t u8x8_byte_esp32_i2c(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr) {
    switch(msg) {
        case U8X8_MSG_BYTE_INIT: break;
        case U8X8_MSG_BYTE_START_TRANSFER: {
            uint8_t i2c_address = u8x8_GetI2CAddress(u8x8);
            handle_i2c = i2c_cmd_link_create();
            i2c_master_start(handle_i2c);
            i2c_master_write_byte(handle_i2c, i2c_address | I2C_MASTER_WRITE, true);
            break;
        }
        case U8X8_MSG_BYTE_SEND: {
            uint8_t *data = (uint8_t *)arg_ptr;
            while(arg_int > 0) {
                i2c_master_write_byte(handle_i2c, *data, true);
                data++; arg_int--;
            }
            break;
        }
        case U8X8_MSG_BYTE_END_TRANSFER: {
            i2c_master_stop(handle_i2c);
            i2c_master_cmd_begin(I2C_MASTER_NUM, handle_i2c, pdMS_TO_TICKS(1000));
            i2c_cmd_link_delete(handle_i2c);
            break;
        }
    }
    return 1;
}

uint8_t u8x8_gpio_and_delay_esp32(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr) {
    switch(msg) {
        case U8X8_MSG_DELAY_MILLI: vTaskDelay(pdMS_TO_TICKS(arg_int)); break;
        case U8X8_MSG_DELAY_10MICRO: esp_rom_delay_us(10); break;
        case U8X8_MSG_DELAY_100NANO: esp_rom_delay_us(1); break;
    }
    return 1;
}

// --- SERVIDOR WEB Y WEBSOCKETS ---
esp_err_t manejador_raiz_get(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/html");
    size_t html_len = index_html_end - index_html_start;
    return httpd_resp_send(req, (const char *)index_html_start, html_len);
}

esp_err_t manejador_ws(httpd_req_t *req) {
    if (req->method == HTTP_GET) return ESP_OK;

    httpd_ws_frame_t ws_pkt;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ws_pkt.type = HTTPD_WS_TYPE_TEXT;

    esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, 0);
    if (ret != ESP_OK) return ret;

    if (ws_pkt.len > 0) {
        uint8_t *buf = calloc(1, ws_pkt.len + 1);
        if (buf == NULL) return ESP_ERR_NO_MEM;
        ws_pkt.payload = buf;
        ret = httpd_ws_recv_frame(req, &ws_pkt, ws_pkt.len);
        if (ret == ESP_OK) {
            u8g2_ClearBuffer(&u8g2);
            u8g2_SetFont(&u8g2, u8g2_font_ncenB08_tr);
            u8g2_DrawStr(&u8g2, 0, 20, (char *)buf);
            u8g2_SendBuffer(&u8g2);
        }
        free(buf);
    }
    return ret;
}

httpd_handle_t iniciar_servidor_web(void) {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    httpd_handle_t server = NULL;

    if (httpd_start(&server, &config) == ESP_OK) {
        servidor_global = server; // Guardamos el servidor para la tarea del ADC
        
        httpd_uri_t ruta_raiz = { .uri = "/", .method = HTTP_GET, .handler = manejador_raiz_get, .user_ctx = NULL };
        httpd_register_uri_handler(server, &ruta_raiz);

        httpd_uri_t ruta_ws = { .uri = "/ws", .method = HTTP_GET, .handler = manejador_ws, .user_ctx = NULL, .is_websocket = true };
        httpd_register_uri_handler(server, &ruta_ws);
    }
    return server;
}

// --- TAREA DEL POTENCIÓMETRO (ADC) ---
void tarea_adc(void *pvParameter) {
    int adc_raw = 0;
    char buffer_adc[16];
    
    httpd_ws_frame_t ws_pkt;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ws_pkt.type = HTTPD_WS_TYPE_TEXT;

    while (1) {
        if (servidor_global != NULL) {
            // Leer el potenciómetro en ADC1 Canal 5
            adc_oneshot_read(adc1_handle, ADC_CHANNEL_5, &adc_raw);
            snprintf(buffer_adc, sizeof(buffer_adc), "%d", adc_raw);

            ws_pkt.payload = (uint8_t *)buffer_adc;
            ws_pkt.len = strlen(buffer_adc);

            // Enviar a los navegadores conectados
            size_t max_clientes = 4;
            int fds[4];
            if (httpd_get_client_list(servidor_global, &max_clientes, fds) == ESP_OK) {
                for (int i = 0; i < max_clientes; i++) {
                    httpd_ws_send_frame_async(servidor_global, fds[i], &ws_pkt);
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(500)); // Enviar cada medio segundo
    }
}

// --- EVENTOS WIFI ---
static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        esp_wifi_connect();
        printf("Reintentando conexión WiFi...\n");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        printf("¡Conectado! IP asignada: " IPSTR "\n", IP2STR(&event->ip_info.ip));
    }
}

void inicializar_wifi(void) {
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);
    esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL);
    esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL);
    wifi_config_t wifi_config = { .sta = { .ssid = WIFI_SSID, .password = WIFI_PASS, }, };
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    esp_wifi_start();
}

// --- FUNCIÓN PRINCIPAL ---
void app_main(void) {
    // 1. Inicializar NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase(); nvs_flash_init();
    }

    // 2. Inicializar I2C y OLED
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO, .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_io_num = I2C_MASTER_SCL_IO, .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };
    i2c_param_config(I2C_MASTER_NUM, &conf);
    i2c_driver_install(I2C_MASTER_NUM, conf.mode, 0, 0, 0);

    u8g2_Setup_ssd1306_i2c_128x64_noname_f(&u8g2, U8G2_R0, u8x8_byte_esp32_i2c, u8x8_gpio_and_delay_esp32);
    u8g2_SetI2CAddress(&u8g2, 0x3C * 2);
    u8g2_InitDisplay(&u8g2);
    u8g2_SetPowerSave(&u8g2, 0);

    u8g2_ClearBuffer(&u8g2);
    u8g2_SetFont(&u8g2, u8g2_font_ncenB08_tr);
    u8g2_DrawStr(&u8g2, 0, 20, "Conectando WiFi...");
    u8g2_SendBuffer(&u8g2);

    // 3. Inicializar el Potenciómetro en GPIO 6 (ADC1 Canal 5)
    adc_oneshot_unit_init_cfg_t init_config1 = { .unit_id = ADC_UNIT_1 };
    adc_oneshot_new_unit(&init_config1, &adc1_handle);
    adc_oneshot_chan_cfg_t config_adc = { .bitwidth = ADC_BITWIDTH_DEFAULT, .atten = ADC_ATTEN_DB_12 };
    adc_oneshot_config_channel(adc1_handle, ADC_CHANNEL_5, &config_adc);

    // 4. Conectar a WiFi
    inicializar_wifi();
    vTaskDelay(pdMS_TO_TICKS(4000));

    // 5. Arrancar el servidor web y la tarea del potenciómetro
    iniciar_servidor_web();
    xTaskCreate(tarea_adc, "tarea_adc", 4096, NULL, 5, NULL);

    u8g2_ClearBuffer(&u8g2);
    u8g2_DrawStr(&u8g2, 0, 20, "Servidor Activo!");
    u8g2_SendBuffer(&u8g2);

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}