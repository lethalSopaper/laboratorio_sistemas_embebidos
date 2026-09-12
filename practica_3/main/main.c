/* ============================================================
 * Graficos y procesos - FreeRTOS
 *
 * Cambiar prioridades y cores para que las animaciones corran 
 * de forma mas eficiente para el usuario final
 *
 * Tres tareas:
 *   vista1, vista2   dibujan cada una su ventana
 *   control          lee las perillas, decide cual tiene el foco,
 *                    reparte las prioridades y pinta la pantalla
 *
 * El dibujo de la figura esta en grafico.c: es la carga de la
 * practica, no su tema.
 *
 * Conexiones:
 *   OLED SSD1306 (I2C)  SDA -> GPIO7 (D8)   SCL -> GPIO44 (D7)
 *   Potenciometro 1     centro -> GPIO6 (D5), extremos 3V3 y GND
 *   Potenciometro 2     centro -> GPIO5 (D4), extremos 3V3 y GND
 * ============================================================ */

#include "esp_adc/adc_oneshot.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "grafico.h"
#include "stdlib.h"
#include "u8g2.h"
#include "u8g2_esp32_hal.h"

/* ========== LO QUE EDITAN USTEDES ========== */

// Que perilla se esta moviendo. Es el renglon de la tabla de abajo.
typedef enum
{
    SOLO1 = 0,
    SOLO2,
    AMBAS,
    NADIE,
    N_SIT
} situacion_t;

// Un renglon de esa tabla: que prioridad le toca a cada tarea.
typedef struct
{
    uint8_t vista1, vista2, control;
} prioridades_t;

// Prioridad de cada tarea, de 1 a 5, segun que perilla se mueve.
static const prioridades_t PRIO[N_SIT] = {
    /* SOLO1 */ {3, 1, 5},
    /* SOLO2 */ {1, 3, 5},
    /* AMBAS */ {1, 1, 5},
    /* NADIE */ {1, 1, 5},
};

// Nucleo de cada tarea. Solo compiten las que comparten core.
#define SIN_AFINIDAD tskNO_AFFINITY
#define CORE_VISTA1 0
#define CORE_VISTA2 0
#define CORE_CONTROL 1

/* ========== DE AQUI PARA ABAJO NO HACE FALTA TOCAR ========== */

#define PIN_SDA 7
#define PIN_SCL 44
#define CANAL_POT1 ADC_CHANNEL_5 // GPIO6, D5
#define CANAL_POT2 ADC_CHANNEL_4 // GPIO5, D4

#define CONTROL_MS 60    // cada cuanto se decide y se refresca
#define UMBRAL 50        // cambio del ADC que ya no es ruido
#define CICLOS_QUIETO 8  // vueltas que la perilla sigue en uso, 0.5 s
#define PRIO_ARRANQUE 6  // main mientras crea las tareas: arriba de todas
#define STACK 4096       // pila de cada tarea, en bytes

// como se escribe cada situacion en la pantalla
static const char *NOMBRE_SIT[N_SIT] = {"SOLO1", "SOLO2", "AMBAS", "NADIE"};

static u8g2_t oled;
static adc_oneshot_unit_handle_t adc1;
static TaskHandle_t h_vista1, h_vista2, h_control; // para cambiarles la prioridad

static volatile int valor1, valor2; // lectura de cada perilla

// Un mapa es la imagen de una ventana: 64 x 48 pixeles, un bit cada uno.
// Hay dos por ventana: la tarea dibuja en uno y publica el otro.
static uint8_t mapa1[2][V_BYTES];
static uint8_t mapa2[2][V_BYTES];
static volatile int listo1, listo2; // cual de los dos esta completo

// escritas mas abajo, en este mismo orden
static void vista1(void *pv);
static void vista2(void *pv);
static void control(void *pv);
static void oled_iniciar(void);
static void adc_iniciar(void);

/* ========== ARRANQUE ========== */

// Prepara todo y crea las tres tareas.
void app_main(void)
{
    // por encima de todas, o la primera tarea la desaloja aqui mismo
    vTaskPrioritySet(NULL, PRIO_ARRANQUE);
    oled_iniciar();
    adc_iniciar();
    xTaskCreatePinnedToCore(vista1, "vista1", STACK, NULL,
                            PRIO[NADIE].vista1, &h_vista1, CORE_VISTA1);
    xTaskCreatePinnedToCore(vista2, "vista2", STACK, NULL,
                            PRIO[NADIE].vista2, &h_vista2, CORE_VISTA2);
    xTaskCreatePinnedToCore(control, "control", STACK, NULL,
                            PRIO[NADIE].control, &h_control, CORE_CONTROL);
    vTaskDelete(NULL);
}

/* ========== LAS DOS VENTANAS ========== */

// Dibuja la ventana izquierda (mapa1) sin parar. Nunca duerme: sin CPU, se congela.
static void vista1(void *pv)
{
    while (1)
    {
        int otro = 1 - listo1;
        grafico_calcular(mapa1[otro], valor1);
        listo1 = otro;
    }
}

// Lo mismo con la ventana derecha (mapa2) y la perilla 2.
static void vista2(void *pv)
{
    while (1)
    {
        int otro = 1 - listo2;
        grafico_calcular(mapa2[otro], valor2);
        listo2 = otro;
    }
}

/* ========== LA TAREA DE CONTROL ========== */

// Le da a cada tarea la prioridad que la tabla marca para esta situacion.
static void prioridades_aplicar(situacion_t situacion)
{
    vTaskPrioritySet(h_vista1, PRIO[situacion].vista1);
    vTaskPrioritySet(h_vista2, PRIO[situacion].vista2);
    vTaskPrioritySet(h_control, PRIO[situacion].control); // se desaloja a si misma
}

// Manda al OLED las dos ventanas, la raya del medio y la situacion.
static void pantalla(situacion_t situacion)
{
    const char *estado = NOMBRE_SIT[situacion];
    // las dos imagenes ya calculadas
    grafico_dibujar(u8g2_GetBufferPtr(&oled), mapa1[listo1], mapa2[listo2]);
    // sin la raya del medio las dos figuras se tocan
    u8g2_DrawVLine(&oled, V_ANCHO - 1, V_Y0, V_ALTO);
    // el nombre de la situacion, centrado
    u8g2_SetFont(&oled, u8g2_font_7x13B_tr);
    int x = (2 * V_ANCHO - u8g2_GetStrWidth(&oled, estado)) / 2;
    u8g2_DrawStr(&oled, x, 13, estado);
    u8g2_SendBuffer(&oled); // aqui se bloquea esperando al I2C
}

// Lee las perillas, decide el foco, reparte prioridades y refresca.
static void control(void *pv)
{
    int espera1 = 0, espera2 = 0; // vueltas que le quedan a cada perilla
    situacion_t situacion = NADIE;
    while (1)
    {
        int v1 = 0, v2 = 0;
        adc_oneshot_read(adc1, CANAL_POT1, &v1);
        adc_oneshot_read(adc1, CANAL_POT2, &v2);
        // la perilla que se movio mas que el ruido queda en uso
        if (abs(v1 - valor1) > UMBRAL)
        {
            valor1 = v1;
            espera1 = CICLOS_QUIETO;
        }
        else if (espera1)
            espera1--;
        if (abs(v2 - valor2) > UMBRAL)
        {
            valor2 = v2;
            espera2 = CICLOS_QUIETO;
        }
        else if (espera2)
            espera2--;
        // quien tiene el foco
        if (espera1 && espera2)
            situacion = AMBAS;
        else if (espera1)
            situacion = SOLO1;
        else if (espera2)
            situacion = SOLO2;
        else
            situacion = NADIE;
        prioridades_aplicar(situacion);
        pantalla(situacion);
        vTaskDelay(pdMS_TO_TICKS(CONTROL_MS));
    }
}

/* ========== CONFIGURACION DEL HARDWARE ========== */

// Prepara el OLED: pines del I2C, modelo de pantalla y encendido.
static void oled_iniciar(void)
{
    u8g2_esp32_hal_t hal = U8G2_ESP32_HAL_DEFAULT;
    hal.bus.i2c.sda = PIN_SDA;
    hal.bus.i2c.scl = PIN_SCL;
    u8g2_esp32_hal_init(hal);
    u8g2_Setup_ssd1306_i2c_128x64_noname_f(&oled, U8G2_R0,
                                           u8g2_esp32_i2c_byte_cb,
                                           u8g2_esp32_gpio_and_delay_cb);
    u8x8_SetI2CAddress(&oled.u8x8, 0x78);
    u8g2_InitDisplay(&oled);
    u8g2_SetPowerSave(&oled, 0);
    u8g2_ClearBuffer(&oled);
}

// Prepara el ADC para leer las dos perillas.
static void adc_iniciar(void)
{
    adc_oneshot_unit_init_cfg_t unidad = {.unit_id = ADC_UNIT_1};
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&unidad, &adc1));
    adc_oneshot_chan_cfg_t canal = {.atten = ADC_ATTEN_DB_12,
                                    .bitwidth = ADC_BITWIDTH_12};
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1, CANAL_POT1, &canal));
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1, CANAL_POT2, &canal));
}
