/** @file modelo_ia.h @brief Interfaz del clasificador LDA entrenado. */
#ifndef IOT_IA_H
#define IOT_IA_H

typedef enum {
    IA_DESCONOCIDO = -1,
    IA_REPOSO = 0,
    IA_X_NEG = 1,
    IA_X_POS = 2,
    IA_Y_NEG = 3,
    IA_Y_POS = 4
} ia_clase_t;

typedef struct {
    ia_clase_t clase;
    float confianza;
} ia_resultado_t;

/** @brief Inferencia local con 3 variables y 5 clases LDA. */
ia_resultado_t ia_predecir(float ax, float ay, float az);
/** @brief Devuelve el nombre de una clase reconocida. */
const char *ia_nombre(ia_clase_t clase);

#endif