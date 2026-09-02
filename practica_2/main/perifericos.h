#pragma once

#include "u8g2.h"
#include "esp_adc/adc_oneshot.h"

// Constantes
#define ADC_MAX 4095
#define PERIODO_MS 100
#define OLED_SDA 7
#define OLED_SCL 44
#define OLED_DIR 0x3C
#define CAB_X1 29
#define CAB_X2 19
#define CAB_Y1 7
#define CAB_Y2 15
#define TEXTO_Y 32
#define BARRA_X 14
#define BARRA_Y 45
#define BARRA_ANCHO 100
#define BARRA_ALTO 12
#define ADC_UNIDAD ADC_UNIT_1
#define ADC_CANAL ADC_CHANNEL_5
#define PWM_GPIO 9
#define PWM_TIMER LEDC_TIMER_0
#define PWM_CANAL LEDC_CHANNEL_0
#define PWM_MODO LEDC_LOW_SPEED_MODE
#define PWM_RES LEDC_TIMER_10_BIT
#define PWM_DUTY_MAX 1023
#define PWM_FREQ 60000

// Declaracion de handlers (extern)
extern u8g2_t u8g2;
extern adc_oneshot_unit_handle_t adc;

// Prototipos de funciones
void pantalla_iniciar(void);
void pantalla_mostrar(int lectura);
void adc_iniciar(void);
int adc_leer(void);
void pwm_iniciar(void);
void pwm_actualizar(int lectura);