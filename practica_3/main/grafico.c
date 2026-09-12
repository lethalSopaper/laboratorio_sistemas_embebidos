/* ============================================================
 * grafico.c - la figura en 3D y el volcado a la pantalla
 *
 * Esto es la CARGA de la practica, no su tema: no hace falta
 * entenderlo, solo saber que dibujar cuesta caro y poder ajustar
 * cuanto, con N_ESFERAS aca abajo.
 *
 * Lo que se usa desde afuera esta en grafico.h.
 * ============================================================ */

#include "grafico.h"
#include "math.h"
#include "string.h"

#define ANCHO_OLED 128 // bytes por franja del buffer del OLED
#define PAG_VENTANAS 2 // las ventanas empiezan en la franja 2
#define V_PAGINAS 6    // y ocupan 6 franjas de 8 filas

/* EL VOLANTE DE CARGA: cuantas esferas tiene el anillo. Cada rayo
 * las prueba todas, asi que el doble de esferas es el doble de
 * trabajo. No cambian de tamano, solo cuantas hay.
 *
 * Para elegirlo: con una sola ventana trabajando, miren los
 * cuadros por segundo. Arriba de 25 sobra CPU y el experimento no
 * se nota; entre 10 y 15 esta bien. Rango util: 6 a 48. */
#define N_ESFERAS 10

/* La OTRA palanca de carga: rayos por pixel en cada eje, o sea que
 * la carga va con SUPER al cuadrado (3 es 9 rayos, 2 es 4, 1 es 1).
 * Sirve para suavizar los bordes; el sombreado no depende de el. */
#define SUPER 2

/* ----- La escena y la camara ----- */
#define ANILLO 1.15f     // radio del anillo
#define R_ESFERA 0.30f   // tamano de cada esfera, fijo
#define L_CUBO 0.35f     // media arista del cubo, fija
#define INCLINA_S 0.574f // inclinacion del anillo: seno y coseno de
#define INCLINA_C 0.819f // 35 grados. Sin ella se veria de canto.
#define CAMARA_Z (-3.5f) // que tan lejos mira la camara. Alejarla
                         // aplana; acercarla exagera la perspectiva.
#define LENTE 1.7f       // el zoom. Medio angulo = atan(1 / LENTE),
                         // o sea que va al reves: mas LENTE, mas cerca.

// La figura en un cuadro: se arma una vez y la consultan todos los
// rayos.
typedef struct
{
    float ex[N_ESFERAS]; // centros de las esferas; el lugar 0 del
    float ey[N_ESFERAS]; // anillo lo ocupa el cubo
    float ez[N_ESFERAS];
    float cx, cy, cz;    // centro del cubo
    float a[3][3];       // sus tres ejes, ya girados
} escena_t;

// Lanza un rayo por la muestra (u, w) y devuelve su tono, de 0
// (fondo) a 16 (blanco). Aca esta el costo de la practica: hay que
// probarlo contra todos los cuerpos y quedarse con el mas cercano,
// porque cualquiera podria ser el que tapa a los demas.
static int muestra(float u, float w, const escena_t *e)
{
    // direccion del rayo, ya normalizada
    float inv = 1.0f / sqrtf(u * u + w * w + LENTE * LENTE);
    float dx = u * inv, dy = w * inv, dz = LENTE * inv;

    float cerca = 1e9f;
    int cual = -1;  // -1 nada, -2 el cubo, >=1 el numero de esfera
    int cara = 0;   // que cara del cubo, si toco el cubo
    float hacia = 1.0f;

    // --- las esferas ---
    // El rayo toca la esfera si la ecuacion de segundo grado que
    // sale de cortarlos tiene solucion.
    for (int i = 1; i < N_ESFERAS; i++)
    {
        float ox = -e->ex[i];
        float oy = -e->ey[i];
        float oz = CAMARA_Z - e->ez[i];

        float b = dx * ox + dy * oy + dz * oz;
        float c = ox * ox + oy * oy + oz * oz - R_ESFERA * R_ESFERA;
        float disc = b * b - c;
        if (disc < 0.0f)
            continue; // pasa de largo

        float t = -b - sqrtf(disc); // la cara de adelante
        if (t > 0.0f && t < cerca)
        {
            cerca = t;
            cual = i;
        }
    }

    // --- el cubo ---
    // Un cubo son tres pares de caras paralelas. Se mira entre que
    // dos valores de t el rayo esta adentro de cada par, y si esas
    // tres franjas se pisan, entro. La ultima en abrirse dice por
    // que cara entro, que es la que va a recibir la luz.
    {
        float ox = -e->cx, oy = -e->cy, oz = CAMARA_Z - e->cz;
        float od[3], dd[3];
        for (int k = 0; k < 3; k++)
        {
            od[k] = ox * e->a[k][0] + oy * e->a[k][1] + oz * e->a[k][2];
            dd[k] = dx * e->a[k][0] + dy * e->a[k][1] + dz * e->a[k][2];
        }

        float dentro = -1e9f, fuera = 1e9f;
        int entra = 0;
        bool falla = false;

        for (int k = 0; k < 3; k++)
        {
            if (dd[k] > -1e-6f && dd[k] < 1e-6f)
            {
                // el rayo va paralelo a ese par de caras
                if (od[k] < -L_CUBO || od[k] > L_CUBO)
                    falla = true;
                continue;
            }
            float r = 1.0f / dd[k];
            float ta = (-L_CUBO - od[k]) * r;
            float tb = (L_CUBO - od[k]) * r;
            if (ta > tb)
            {
                float s = ta;
                ta = tb;
                tb = s;
            }
            if (ta > dentro)
            {
                dentro = ta;
                entra = k;
            }
            if (tb < fuera)
                fuera = tb;
        }

        if (!falla && fuera >= dentro && dentro > 0.0f && dentro < cerca)
        {
            cerca = dentro;
            cual = -2;
            cara = entra;
            hacia = (dd[entra] < 0.0f) ? 1.0f : -1.0f;
        }
    }

    if (cual == -1)
        return 0; // no toco nada: es fondo

    // hacia donde mira la superficie donde choco
    float nx, ny, nz;
    if (cual == -2)
    {
        nx = e->a[cara][0] * hacia;
        ny = e->a[cara][1] * hacia;
        nz = e->a[cara][2] * hacia;
    }
    else
    {
        nx = (dx * cerca - e->ex[cual]) / R_ESFERA;
        ny = (dy * cerca - e->ey[cual]) / R_ESFERA;
        nz = (CAMARA_Z + dz * cerca - e->ez[cual]) / R_ESFERA;
    }

    // cuanta luz le llega desde arriba a la izquierda
    float luz = nx * (-0.48f) + ny * (-0.60f) + nz * (-0.64f);
    if (luz < 0.0f)
        luz = 0.0f;

    int tono = (int)((0.18f + 0.82f * luz) * 16.0f);
    return (tono > 16) ? 16 : tono;
}

// Enciende un pixel de una ventana. El mapa esta en el formato del
// buffer del OLED: paginas de 8 filas, un byte por columna.
static void punto(uint8_t *v, int x, int y)
{
    v[(y / 8) * V_ANCHO + x] |= (uint8_t)(1 << (y % 8));
}

// Trama ordenada: el OLED es de un bit, asi que los tonos
// intermedios del borde se dibujan con este patron.
static const uint8_t TRAMA[4][4] = {{0, 8, 2, 10},
                                    {12, 4, 14, 6},
                                    {3, 11, 1, 9},
                                    {15, 7, 13, 5}};

// Calcula la imagen entera de la figura. La posicion de la perilla
// es directamente el angulo de giro del anillo.
void grafico_calcular(uint8_t *v, int valor)
{
    float ang = (float)valor * (6.2832f / 4095.0f);
    const float MEDIO = V_ANCHO / 2.0f;
    escena_t e;

    // La escena se arma UNA sola vez por cuadro, no por rayo:
    // girar el anillo es barato, lo caro es probarlo despues
    // contra los 27 648 rayos.
    for (int i = 0; i < N_ESFERAS; i++)
    {
        float a = ang + (6.2832f * i) / N_ESFERAS;
        float x = ANILLO * cosf(a);
        float z = ANILLO * sinf(a);
        e.ex[i] = x;                 // el anillo va inclinado, para
        e.ey[i] = -z * INCLINA_S;    // que se vea en perspectiva y
        e.ez[i] = z * INCLINA_C;     // no de canto
    }

    // El cubo va montado en el lugar 0 del anillo, y gira con el:
    // sus tres ejes son los del anillo, inclinados igual.
    float sa = sinf(ang), ca = cosf(ang);
    e.cx = e.ex[0];
    e.cy = e.ey[0];
    e.cz = e.ez[0];
    e.a[0][0] = ca;  e.a[0][1] = -sa * INCLINA_S; e.a[0][2] = sa * INCLINA_C;
    e.a[1][0] = 0.f; e.a[1][1] = INCLINA_C;       e.a[1][2] = INCLINA_S;
    e.a[2][0] = -sa; e.a[2][1] = -ca * INCLINA_S; e.a[2][2] = ca * INCLINA_C;

    memset(v, 0, V_BYTES);

    for (int y = 0; y < V_ALTO; y++)
        for (int x = 0; x < V_ANCHO; x++)
        {
            // SUPER x SUPER rayos por pixel; el promedio de sus
            // tonos es el tono del pixel
            int suma = 0;
            for (int sy = 0; sy < SUPER; sy++)
                for (int sx = 0; sx < SUPER; sx++)
                {
                    float u = ((float)x + (sx + 0.5f) / SUPER - MEDIO) / MEDIO;
                    float w = ((float)y + (sy + 0.5f) / SUPER -
                               V_ALTO / 2.0f) / MEDIO;
                    suma += muestra(u, w, &e);
                }

            int tono = suma / (SUPER * SUPER); // 0 a 16
            if (tono > TRAMA[y & 3][x & 3])
                punto(v, x, y);
        }
}

// El buffer del OLED es una tira de 1024 bytes partida en ocho
// franjas de 8 filas. La franja 0 lleva el texto y las seis de
// abajo las dos ventanas, lado a lado: en cada franja van 64 bytes
// de la imagen izquierda y 64 de la derecha.
void grafico_dibujar(uint8_t *buffer, const uint8_t *izquierda,
                     const uint8_t *derecha)
{
    // borrar donde va el texto; lo de abajo se sobrescribe entero
    memset(buffer, 0, PAG_VENTANAS * ANCHO_OLED);

    for (int p = 0; p < V_PAGINAS; p++)
    {
        uint8_t *franja = buffer + (PAG_VENTANAS + p) * ANCHO_OLED;
        memcpy(franja, izquierda + p * V_ANCHO, V_ANCHO);
        memcpy(franja + V_ANCHO, derecha + p * V_ANCHO, V_ANCHO);
    }
}
