#include "5_Servidor_Web.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_http_server.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Librerias para I2C (OLED) y ADC (Potenciometro)
#include "driver/i2c.h"
#include "driver/adc.h"

// Pines definidos en la Práctica 5
#define ADC_PIN 6             // Potenciómetro: D5 (GPIO 6, ADC1 canal 5)
#define ADC_CHANNEL ADC1_CHANNEL_5
#define I2C_SDA_PIN 7         // OLED SDA: D8 (GPIO 7)
#define I2C_SCL_PIN 44        // OLED SCL: D7 (GPIO 44)
#define I2C_PORT I2C_NUM_0

#define WIFI_SSID "fr2.4g"
#define WIFI_PASS "GrayArrow123"

#define PERIODO_ENVIO_US 500000 // Enviar ADC cada 500ms
#define NUCLEO_ENVIO 1

static const char *TAG = "practica5";
static volatile int s_adc_valor = 0;
static httpd_handle_t s_servidor = NULL; 

// Símbolos del HTML embebido
extern const char index_html_inicio[] asm("_binary_index_html_start");
extern const char index_html_fin[] asm("_binary_index_html_end");

// Prototipos
static void wifi_iniciar(void);
static void servidor_iniciar(void);
static void hardware_iniciar(void);
static void eventos_wifi(void *arg, esp_event_base_t base, int32_t id, void *data);
static esp_err_t handler_raiz(httpd_req_t *req);
static esp_err_t handler_ws(httpd_req_t *req);
static void ws_enviar_a_todos(void);
static void tarea_adc(void *arg);

// Función dummy para simular escribir en la OLED (reemplazar con librería SSD1306)
static void oled_escribir_texto(const char* texto) {
    ESP_LOGI(TAG, "OLED MUESTRA: %s", texto);
    // TODO: Usar funciones I2C reales para enviar 'texto' a la pantalla OLED
}

void servidor_web_app_main(void)
{
    if (nvs_flash_init() != ESP_OK)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }

    hardware_iniciar();
    wifi_iniciar();
    servidor_iniciar();
    
    xTaskCreatePinnedToCore(tarea_adc, "tarea_adc", 4096, NULL, 5, NULL, NUCLEO_ENVIO);
}

static void hardware_iniciar(void)
{
    // Configurar ADC (Potenciómetro)
    adc1_config_width(ADC_WIDTH_BIT_12); // Rango 0 a 4095
    adc1_config_channel_atten(ADC_CHANNEL, ADC_ATTEN_DB_11);

    // Configurar I2C (OLED)
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_SDA_PIN,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_io_num = I2C_SCL_PIN,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 400000,
    };
    i2c_param_config(I2C_PORT, &conf);
    i2c_driver_install(I2C_PORT, conf.mode, 0, 0, 0);
}

static void wifi_iniciar(void)
{
    ESP_ERROR_CHECK(esp_netif_init());                
    ESP_ERROR_CHECK(esp_event_loop_create_default()); 
    esp_netif_create_default_wifi_sta();              

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, &eventos_wifi, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, &eventos_wifi, NULL, NULL));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "Conectando a %s ...", WIFI_SSID);
}

static void servidor_iniciar(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    ESP_ERROR_CHECK(httpd_start(&s_servidor, &config));

    static const httpd_uri_t uri_raiz = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = handler_raiz,
    };
    
    static const httpd_uri_t uri_ws = {
        .uri = "/ws",
        .method = HTTP_GET,
        .handler = handler_ws,
        .is_websocket = true, // Importante para Práctica 5
    };

    httpd_register_uri_handler(s_servidor, &uri_raiz);
    httpd_register_uri_handler(s_servidor, &uri_ws);

    ESP_LOGI(TAG, "Servidor HTTP iniciado.");
}

static void eventos_wifi(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        esp_wifi_connect();
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t e = *(ip_event_got_ip_t *)data;
        ESP_LOGI(TAG, "IP obtenida. Abre en navegador: http://" IPSTR, IP2STR(&e.ip_info.ip));
    }
}

static esp_err_t handler_raiz(httpd_req_t *req)
{
    const size_t largo = (index_html_fin - index_html_inicio) - 1;
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, index_html_inicio, largo);
}

// Handler WebSocket: Recibe el mensaje de texto de la página
static esp_err_t handler_ws(httpd_req_t *req)
{
    if (req->method == HTTP_GET) {
        return ESP_OK; // Handshake
    }

    // Leer trama de longitud desconocida según instrucciones
    httpd_ws_frame_t trama = {0};
    
    // Llamada 1: Largo de la trama
    esp_err_t err = httpd_ws_recv_frame(req, &trama, 0);
    if (err != ESP_OK) return err;

    if (trama.len > 0) {
        // Reservar memoria + 1 para el caracter nulo
        uint8_t *buf = calloc(1, trama.len + 1);
        if (buf == NULL) return ESP_ERR_NO_MEM;

        trama.payload = buf;
        // Llamada 2: Leer el texto
        err = httpd_ws_recv_frame(req, &trama, trama.len);
        if (err == ESP_OK) {
            // El buffer ya tiene el \0 al final por el calloc, por lo que es una cadena C válida
            ESP_LOGI(TAG, "Mensaje WebSocket recibido: %s", (char*)trama.payload);
            
            // Enviar a la OLED
            oled_escribir_texto((char*)trama.payload);
        }
        free(buf);
    }
    return err;
}

static void ws_enviar_a_todos(void)
{
    char texto[32];
    int clientes[CONFIG_LWIP_MAX_SOCKETS];
    size_t n = CONFIG_LWIP_MAX_SOCKETS;

    if (s_servidor == NULL || httpd_get_client_list(s_servidor, &n, clientes) != ESP_OK) {
        return;
    }

    // Estructurar como JSON para facilitar la lectura en JS
    httpd_ws_frame_t trama = {
        .type = HTTPD_WS_TYPE_TEXT,
        .payload = (uint8_t *)texto,
        .len = snprintf(texto, sizeof(texto), "{\"adc\":%d}", s_adc_valor),
    };

    for (size_t i = 0; i < n; i++) {
        if (httpd_ws_get_fd_info(s_servidor, clientes[i]) == HTTPD_WS_CLIENT_WEBSOCKET) {
            httpd_ws_send_data(s_servidor, clientes[i], &trama);
        }
    }
}

static void tarea_adc(void *arg)
{
    int64_t siguiente_us = esp_timer_get_time();
    int64_t ultimo_ceder_us = siguiente_us;

    while (true)
    {
        // Leer Potenciómetro (0 - 4095)
        s_adc_valor = adc1_get_raw(ADC_CHANNEL);
        
        // Empujar valor a clientes WebSocket
        ws_enviar_a_todos();

        siguiente_us += PERIODO_ENVIO_US;

        if (esp_timer_get_time() - ultimo_ceder_us > 200000) {
            vTaskDelay(1);
            ultimo_ceder_us = esp_timer_get_time();
        }
        while (esp_timer_get_time() < siguiente_us) {}
    }
}