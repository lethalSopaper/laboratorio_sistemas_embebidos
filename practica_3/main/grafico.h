#ifndef GRAFICO_H
#define GRAFICO_H

#include <stdint.h>

/* La figura y su dibujo viven en grafico.c. Es la carga de la
 * practica, no su tema: no hace falta abrirlo. */

/* Una ventana mide 64 x 48 pixeles. Como el OLED guarda un bit por
 * pixel, su mapa ocupa 384 bytes. Las dos ventanas van lado a lado
 * y empiezan en la fila 16 de la pantalla. */
#define V_ANCHO 64
#define V_ALTO 48
#define V_BYTES 384
#define V_Y0 16

/* CALCULA la imagen de la figura, girada segun valor (0 a 4095, la
 * lectura del potenciometro), y la deja en mapa[]. No toca la
 * pantalla: escribe en memoria.
 *
 * Es la parte cara, y es lo que las dos tareas se disputan. Cuanto
 * cuesta se ajusta con N_ESFERAS, al principio de grafico.c. */
void grafico_calcular(uint8_t *mapa, int valor);

/* DIBUJA en la pantalla los dos mapas ya calculados, uno al lado
 * del otro, y deja limpia la franja de arriba para el texto.
 *
 * Aca no se calcula nada: son los dos mapas copiados tal cual,
 * que es muchisimo mas rapido que mandarlos pixel por pixel. Es lo
 * unico del programa que no se hace con una funcion de u8g2. */
void grafico_dibujar(uint8_t *buffer, const uint8_t *izquierda,
                     const uint8_t *derecha);

#endif
