#include "perifericos.h"
#include <stdio.h>
#include "u8g2_esp32_hal.h"
#include "driver/ledc.h"

// Definicion de handlers
u8g2_t u8g2;
adc_oneshot_unit_handle_t adc;

void pantalla_iniciar(void)
{
    u8g2_esp32_hal_t hal = U8G2_ESP32_HAL_DEFAULT;
    hal.bus.i2c.sda = OLED_SDA;
    hal.bus.i2c.scl = OLED_SCL;
    u8g2_esp32_hal_init(hal);

    u8g2_Setup_ssd1306_i2c_128x64_noname_f(
        &u8g2, U8G2_R0,
        u8g2_esp32_i2c_byte_cb,
        u8g2_esp32_gpio_and_delay_cb);

    u8x8_SetI2CAddress(&u8g2.u8x8, OLED_DIR << 1);
    u8g2_InitDisplay(&u8g2);
    u8g2_SetPowerSave(&u8g2, 0);
    u8g2_SetFont(&u8g2, u8g2_font_6x10_tf);
}

void pantalla_mostrar(int lectura)
{
    static int lectura_previa = -1;
    char texto[32];

    if (lectura == lectura_previa) return;
    lectura_previa = lectura;

    int porcentaje = (lectura * 100) / ADC_MAX;

    u8g2_ClearBuffer(&u8g2); 
    u8g2_SetFont(&u8g2, u8g2_font_6x10_tf);
    u8g2_DrawStr(&u8g2, CAB_X1, CAB_Y1, "Fundamentos de");
    u8g2_DrawStr(&u8g2, CAB_X2, CAB_Y2, "Sistemas Embebidos");

    snprintf(texto, sizeof(texto), "ADC: %04d | %3d%%", lectura, porcentaje);
    int x_texto = (128 - u8g2_GetStrWidth(&u8g2, texto)) / 2;
    u8g2_DrawStr(&u8g2, x_texto, TEXTO_Y, texto);

    u8g2_DrawFrame(&u8g2, BARRA_X, BARRA_Y, BARRA_ANCHO, BARRA_ALTO);
    if (porcentaje > 0) {
        u8g2_DrawBox(&u8g2, BARRA_X, BARRA_Y, porcentaje, BARRA_ALTO);
    }
    u8g2_SendBuffer(&u8g2);
}

void adc_iniciar(void)
{
    adc_oneshot_unit_init_cfg_t cfg_unidad = {
        .unit_id = ADC_UNIDAD,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&cfg_unidad, &adc));

    adc_oneshot_chan_cfg_t cfg_canal = {
        .bitwidth = ADC_BITWIDTH_12,
        .atten = ADC_ATTEN_DB_12,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc, ADC_CANAL, &cfg_canal));
}

int adc_leer(void)
{
    int lectura = 0;
    adc_oneshot_read(adc, ADC_CANAL, &lectura);
    return lectura;
}

void pwm_iniciar(void)
{
    ledc_timer_config_t cfg_timer = {
        .speed_mode = PWM_MODO,
        .timer_num = PWM_TIMER,
        .duty_resolution = PWM_RES,
        .freq_hz = PWM_FREQ,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&cfg_timer));

    ledc_channel_config_t cfg_canal = {
        .speed_mode = PWM_MODO,
        .channel = PWM_CANAL,
        .timer_sel = PWM_TIMER,
        .intr_type = LEDC_INTR_DISABLE,
        .gpio_num = PWM_GPIO,
        .duty = 0,
        .hpoint = 0,
    };
    ESP_ERROR_CHECK(ledc_channel_config(&cfg_canal));
}

void pwm_actualizar(int lectura)
{
    uint32_t duty = ((uint32_t)lectura * PWM_DUTY_MAX) / ADC_MAX;
    ledc_set_duty(PWM_MODO, PWM_CANAL, duty);
    ledc_update_duty(PWM_MODO, PWM_CANAL);
}