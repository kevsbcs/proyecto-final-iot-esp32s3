/**
 * @file modelo_ia.c
 * @brief Clasificador supervisado LDA exportado desde Python.
 * @details Pesos entrenados sobre 915 mediciones QMI8658 proporcionadas
 * por el estudiante. Las etiquetas describen los gestos fisicos, no el
 * signo numerico de los ejes. Probabilidades softmax son relativas.
 */
#include <math.h>
#include "modelo_ia.h"
#include "modelo_gestos.h"

#define IA_CONFIANZA_MIN 0.80f
#define IA_G_MIN2 (0.65f * 0.65f)
#define IA_G_MAX2 (1.35f * 1.35f)

const char *ia_nombre(ia_clase_t clase)
{
    if (clase < IA_REPOSO || clase > IA_Y_POS) return "DESCONOCIDO";
    return MODELO_ETIQUETAS[(int)clase];
}

ia_resultado_t ia_predecir(float ax, float ay, float az)
{
    ia_resultado_t out = { IA_DESCONOCIDO, 0.0f };
    const float g2 = ax * ax + ay * ay + az * az;
    if (g2 < IA_G_MIN2 || g2 > IA_G_MAX2) return out;

    const float entrada[MODELO_NUM_ENTRADAS] = {ax, ay, az};
    float scores[MODELO_NUM_CLASES] = {0};
    float maximo = -INFINITY;
    int mejor = 0;
    for (int k = 0; k < MODELO_NUM_CLASES; k++) {
        float s = MODELO_SESGOS[k];
        for (int n = 0; n < MODELO_NUM_ENTRADAS; n++)
            s += MODELO_PESOS[k][n] * entrada[n];
        scores[k] = s;
        if (s > maximo) { maximo = s; mejor = k; }
    }
    float total = 0.0f;
    for (int k = 0; k < MODELO_NUM_CLASES; k++)
        total += expf(scores[k] - maximo);

    out.confianza = 1.0f / total;
    if (out.confianza >= IA_CONFIANZA_MIN) out.clase = (ia_clase_t)mejor;
    return out;
}