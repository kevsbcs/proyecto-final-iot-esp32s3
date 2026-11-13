/**
 * @file modelo_gestos.h
 * @brief Modelo LDA entrenado con gestos_imu.csv.
 * @note Generado automaticamente por entrenar_modelo.py.
 * @note Orden de entradas: ax, ay, az, medidas en g.
 */
#ifndef MODELO_GESTOS_H
#define MODELO_GESTOS_H

#define MODELO_NUM_CLASES 5
#define MODELO_NUM_ENTRADAS 3

/* Orden fijo de clases, usado en el firmware. */
static const char *const MODELO_ETIQUETAS[MODELO_NUM_CLASES] = {
    "REPOSO",
    "X_NEG",
    "X_POS",
    "Y_NEG",
    "Y_POS",
};

/* Coeficientes del discriminante lineal: score = W*x + b. */
static const float MODELO_PESOS[MODELO_NUM_CLASES][MODELO_NUM_ENTRADAS] = {
    {-62.434947700f, -13.862751604f, -81.671605737f},
    {-24.156539944f, -310.625974729f, -29.764874252f},
    {56.768220890f, 309.973297360f, 90.838090766f},
    {231.749327841f, 30.860585431f, 171.865677316f},
    {-201.926061087f, -16.345156457f, -151.267288093f},
};

static const float MODELO_SESGOS[MODELO_NUM_CLASES] = {
    -73.168185450f,
    -136.471847704f,
    -87.181955151f,
    55.739882922f,
    -183.235532773f,
};

#endif /* MODELO_GESTOS_H */