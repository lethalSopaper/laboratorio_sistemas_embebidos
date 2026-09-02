#include "perifericos.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

void app_main(void)
{
    pantalla_iniciar();
    adc_iniciar();
    pwm_iniciar();

    while (1)
    {
        int lectura = adc_leer();

        pwm_actualizar(lectura);
        pantalla_mostrar(lectura);

        vTaskDelay(pdMS_TO_TICKS(PERIODO_MS));
    }
}