"""Entrena y exporta clasificador LDA a C para ESP32-S3.

Ejemplo:
    python entrenar_modelo.py --csv gestos_imu.csv --salida main/modelo_gestos.h

La evaluacion se realiza separando rondas completas, sin mezclar filas
adyacentes del mismo movimiento entre entrenamiento y prueba.
"""

from __future__ import annotations

import argparse
from pathlib import Path

import numpy as np
import pandas as pd
from sklearn.discriminant_analysis import LinearDiscriminantAnalysis
from sklearn.metrics import accuracy_score, classification_report, confusion_matrix

CLASES = ("REPOSO", "X_NEG", "X_POS", "Y_NEG", "Y_POS")
COLUMNAS = ("ax", "ay", "az")


def leer_y_verificar_csv(archivo: Path) -> pd.DataFrame:
    """Carga el conjunto de datos y verifica su estructura basica."""
    df = pd.read_csv(archivo)
    esperadas = {"ronda", "etiqueta", *COLUMNAS}
    faltantes = esperadas - set(df.columns)
    if faltantes:
        raise ValueError(f"Faltan columnas: {sorted(faltantes)}")
    if df[["ronda", "etiqueta", *COLUMNAS]].isnull().any().any():
        raise ValueError("Existen celdas vacias en columnas requeridas")
    if not np.isfinite(df[list(COLUMNAS)].to_numpy(dtype=np.float64)).all():
        raise ValueError("Hay aceleraciones que no son numeros finitos")
    etiquetas = set(df["etiqueta"].unique())
    if etiquetas != set(CLASES):
        raise ValueError(f"Etiquetas inesperadas: {etiquetas}; requeridas: {CLASES}")
    rondas = sorted(df["ronda"].unique())
    if len(rondas) < 2:
        raise ValueError("Se necesitan al menos dos rondas independientes")
    for r in rondas:
        presentes = set(df.loc[df["ronda"] == r, "etiqueta"])
        if presentes != set(CLASES):
            raise ValueError(f"La ronda {r} no contiene las cinco clases")
    return df


def entrenar(df: pd.DataFrame) -> tuple[LinearDiscriminantAnalysis, str]:
    """Evalua por ronda y entrena un modelo final usando todos los datos."""
    x = df[list(COLUMNAS)].to_numpy(dtype=np.float64)
    y = df["etiqueta"].to_numpy()
    rondas = df["ronda"].to_numpy()
    resumen = [
        "PROYECTO IOT ESP32-S3 - REPORTE DE CLASIFICACION DE GESTOS",
        "Algoritmo: analisis discriminante lineal (LDA), supervisado.",
        "Entradas: ax, ay, az [g] - QMI8658.",
        f"Total de muestras: {len(df)}",
        "Las etiquetas describen gestos fisicos, no necesariamente signos de los ejes.",
        "",
        "Distribucion por ronda y etiqueta:",
        df.groupby(["ronda", "etiqueta"]).size().to_string(),
        "",
    ]

    predicciones_cv = np.empty(shape=len(y), dtype=object)
    exactitudes = []
    for ronda_prueba in sorted(np.unique(rondas)):
        ent = rondas != ronda_prueba
        eva = ~ent
        modelo = LinearDiscriminantAnalysis()
        modelo.fit(x[ent], y[ent])
        prediccion = modelo.predict(x[eva])
        predicciones_cv[eva] = prediccion
        exactitud = accuracy_score(y[eva], prediccion)
        exactitudes.append(float(exactitud))
        resumen.append(
            f"Ronda {ronda_prueba} reservada: {np.sum(eva)} muestras, "
            f"exactitud = {exactitud:.4%}"
        )
        if int(ronda_prueba) == 3:
            resumen.extend([
                "", "Matriz de confusion - ronda 3, filas reales/columnas predichas:",
                f"Clases: {', '.join(CLASES)}",
                np.array2string(confusion_matrix(y[eva], prediccion, labels=CLASES)),
                "",
            ])

    resumen.extend([
        "Validacion agrupada por rondas:",
        f"Exactitud global: {accuracy_score(y, predicciones_cv):.4%}",
        f"Exactitud media por ronda: {np.mean(exactitudes):.4%}",
        "", "Informe de clasificacion agrupada por ronda:",
        classification_report(y, predicciones_cv, labels=CLASES, digits=4, zero_division=0),
    ])

    final = LinearDiscriminantAnalysis().fit(x, y)
    if tuple(final.classes_) != CLASES:
        raise AssertionError(f"Orden inesperado: {final.classes_}")
    resumen.extend([
        "MODELO FINAL: reentrenado con las tres rondas completas.",
        "El 100% de exactitud observado NO garantiza rendimiento fuera de las poses capturadas.",
        "Pendiente validar inclinaciones intermedias, dinamicas y otros usuarios.",
        "Los comandos solo se muestran por terminal; ESP-NOW y OTA real estan pendientes.",
    ])
    return final, "\n".join(resumen) + "\n"


def generar_header(modelo: LinearDiscriminantAnalysis, destino: Path) -> None:
    """Exporta el clasificador lineal a constantes compatibles con C."""
    coef = modelo.coef_
    bias = modelo.intercept_
    lineas = [
        "/**",
        " * @file modelo_gestos.h",
        " * @brief Modelo LDA entrenado con gestos_imu.csv.",
        " * @note Generado automaticamente por entrenar_modelo.py.",
        " * @note Orden de entradas: ax, ay, az, medidas en g.",
        " */",
        "#ifndef MODELO_GESTOS_H",
        "#define MODELO_GESTOS_H",
        "",
        "#define MODELO_NUM_CLASES 5",
        "#define MODELO_NUM_ENTRADAS 3",
        "",
        "/* Orden fijo de clases, usado en el firmware. */",
        "static const char *const MODELO_ETIQUETAS[MODELO_NUM_CLASES] = {",
        *[f'    "{n}",' for n in CLASES],
        "};",
        "",
        "/* Coeficientes del discriminante lineal: score = W*x + b. */",
        "static const float MODELO_PESOS[MODELO_NUM_CLASES][MODELO_NUM_ENTRADAS] = {",
    ]
    for row in coef:
        lineas.append("    {" + ", ".join(f"{float(v):.9f}f" for v in row) + "},")
    lineas.extend([
        "};", "",
        "static const float MODELO_SESGOS[MODELO_NUM_CLASES] = {",
        *[f"    {float(v):.9f}f," for v in bias],
        "};", "",
        "#endif /* MODELO_GESTOS_H */", "",
    ])
    destino.parent.mkdir(parents=True, exist_ok=True)
    destino.write_text("\n".join(lineas), encoding="utf-8")


def verificar_precision_float32(modelo: LinearDiscriminantAnalysis, df: pd.DataFrame) -> None:
    """Comprueba que los coeficientes redondeados funcionan como en ESP32."""
    x = df[list(COLUMNAS)].to_numpy(dtype=np.float32)
    w = np.array(np.round(modelo.coef_, 9), dtype=np.float32)
    b = np.array(np.round(modelo.intercept_, 9), dtype=np.float32)
    scores = x @ w.T + b
    pred_c = np.asarray(CLASES)[np.argmax(scores, axis=1)]
    pred_python = modelo.predict(df[list(COLUMNAS)].to_numpy(dtype=np.float64))
    desacuerdos = np.flatnonzero(pred_c != pred_python)
    if len(desacuerdos):
        raise RuntimeError(
            f"El modelo exportado en float32 difiere en {len(desacuerdos)} filas"
        )


def main() -> None:
    parser = argparse.ArgumentParser(description="Entrena LDA y exporta modelo a C")
    parser.add_argument("--csv", default="gestos_imu.csv", type=Path)
    parser.add_argument("--salida", default=Path("main/modelo_gestos.h"), type=Path)
    parser.add_argument("--reporte", default=Path("reporte_modelo.txt"), type=Path)
    args = parser.parse_args()
    df = leer_y_verificar_csv(args.csv)
    modelo, reporte = entrenar(df)
    verificar_precision_float32(modelo, df)
    generar_header(modelo, args.salida)
    args.reporte.write_text(reporte, encoding="utf-8")
    print(reporte)
    print(f"Modelo generado: {args.salida}")
    print(f"Reporte generado: {args.reporte}")


if __name__ == "__main__":
    main()