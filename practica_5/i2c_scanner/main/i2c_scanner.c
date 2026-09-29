#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c.h"
#include "esp_log.h"

// Pines basados en tu diagrama de la XIAO ESP32-S3
#define I2C_MASTER_SCL_IO           44
#define I2C_MASTER_SDA_IO           7
#define I2C_MASTER_NUM              I2C_NUM_0
#define I2C_MASTER_FREQ_HZ          100000 // 100kHz es más estable para escaneo

static const char *TAG = "I2C_SCAN";

void app_main(void) {
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };
    i2c_param_config(I2C_MASTER_NUM, &conf);
    i2c_driver_install(I2C_MASTER_NUM, conf.mode, 0, 0, 0);

    ESP_LOGI(TAG, "Iniciando escaneo I2C en SDA (GPIO%d) y SCL (GPIO%d)...", I2C_MASTER_SDA_IO, I2C_MASTER_SCL_IO);
    
    int devices_found = 0;
    
    for (uint8_t i = 1; i < 127; i++) {
        i2c_cmd_handle_t cmd = i2c_cmd_link_create();
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (i << 1) | I2C_MASTER_WRITE, true);
        i2c_master_stop(cmd);
        
        esp_err_t ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, pdMS_TO_TICKS(50));
        i2c_cmd_link_delete(cmd);

        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "¡Dispositivo encontrado en la direccion 0x%02X!", i);
            devices_found++;
        }
    }
    
    if (devices_found == 0) {
        ESP_LOGE(TAG, "No se encontraron dispositivos. Revisa jumpers, protoboard y alimentación.");
    } else {
        ESP_LOGI(TAG, "Escaneo finalizado. %d dispositivo(s) detectado(s).", devices_found);
    }

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}