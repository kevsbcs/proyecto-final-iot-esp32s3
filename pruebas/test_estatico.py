#!/usr/bin/env python3
"""Pruebas host del proyecto IoT. No sustituyen la prueba en ESP32 real.

Ejecutar desde carpeta raiz: python pruebas/test_estatico.py
Necesita sklearn/pandas/numpy para revisar exactitud de rondas.
"""
from __future__ import annotations
from pathlib import Path
import ctypes
import importlib.util
import os
import subprocess
import sys
import tempfile
import threading
import urllib.request
import json

ROOT = Path(__file__).resolve().parents[1]
S3 = ROOT / "nodo_waveshare" / "main"


def probar_protocolo(d: Path) -> None:
    codigo = r'''
#include "protocolo.h"
int main(void) {
    iot_trama_t t = iot_trama(IOT_T_COMANDO, IOT_CMD_ON, 1, 33);
    return (sizeof(t) == 12 && iot_trama_valida(&t) && t.secuencia == 33) ? 0 : 1;
}
'''
    p = d / "proto_test.c"
    p.write_text(codigo, encoding="utf-8")
    exe = d / "proto_test"
    subprocess.run(["gcc", "-std=c11", "-Wall", "-Wextra", "-Werror", "-I", str(S3),
                    str(p), "-o", str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
    print("[OK] Protocolo 12 bytes, magic, secuencia, gcc")


def probar_inferencia_host(d: Path) -> None:
    import pandas as pd
    import numpy as np
    from sklearn.discriminant_analysis import LinearDiscriminantAnalysis

    biblioteca = d / "libia.so"
    subprocess.run(["gcc", "-std=c11", "-Wall", "-Wextra", "-O2", "-shared", "-fPIC",
                    "-I", str(S3), str(S3 / "modelo_ia.c"), "-lm", "-o", str(biblioteca)],
                   check=True)
    lib = ctypes.CDLL(str(biblioteca))

    class Respuesta(ctypes.Structure):
        _fields_ = [("clase", ctypes.c_int), ("confianza", ctypes.c_float)]

    lib.ia_predecir.argtypes = [ctypes.c_float, ctypes.c_float, ctypes.c_float]
    lib.ia_predecir.restype = Respuesta
    df = pd.read_csv(ROOT / "datos_ia" / "gestos_imu.csv")
    clases = ["REPOSO", "X_NEG", "X_POS", "Y_NEG", "Y_POS"]
    errores = []
    for _, r in df.iterrows():
        p = lib.ia_predecir(float(r.ax), float(r.ay), float(r.az))
        if p.clase < 0 or clases[p.clase] != r.etiqueta:
            errores.append((r.ronda, r.etiqueta, p.clase, p.confianza))
    print(f"[OK] Inferencia C gcc: {len(df)-len(errores)}/{len(df)} capturas clasificadas")
    assert not errores, f"Las primeras muestras fallidas: {errores[:4]}"

    # Verificacion externa de las metricas reportadas: leave-one-round-out.
    rondas = sorted(df.ronda.unique())
    x = df[["ax", "ay", "az"]].to_numpy()
    y = df.etiqueta.to_numpy()
    for ronda in rondas:
        train = df.ronda.to_numpy() != ronda
        clf = LinearDiscriminantAnalysis().fit(x[train], y[train])
        acc = (clf.predict(x[~train]) == y[~train]).mean()
        print(f"[OK] Ronda {ronda} reservada: exactitud {100*acc:.1f}%")
        assert acc >= 0.95


def probar_servidor(d: Path) -> None:
    spec = importlib.util.spec_from_file_location("servidor_edge", ROOT / "edge_server" / "servidor_edge.py")
    mod = importlib.util.module_from_spec(spec)
    assert spec.loader
    spec.loader.exec_module(mod)
    mod.HISTORIAL = d / "telemetria.jsonl"
    mod.FIRMWARE = d / "firmware.bin"
    httpd = mod.ThreadingHTTPServer(("127.0.0.1", 0), mod.EdgeHandler)
    httpd.cantidad_total = 0
    th = threading.Thread(target=httpd.serve_forever, daemon=True)
    th.start()
    port = httpd.server_port
    try:
        payload = json.dumps({"nodo": "PRUEBA", "gesto": "REPOSO", "ax": 0.1,
                              "ay": 0.0, "az": -1.0, "led": 0, "confianza": 0.8}).encode()
        req = urllib.request.Request(f"http://127.0.0.1:{port}/api/telemetria",
                                     data=payload, headers={"Content-Type": "application/json"},
                                     method="POST")
        with urllib.request.urlopen(req, timeout=3) as r:
            assert r.status == 200
        with urllib.request.urlopen(f"http://127.0.0.1:{port}/api/datos", timeout=3) as r:
            data = json.load(r)
        assert data["cantidad_total"] == 1 and data["ultimas"][0]["gesto"] == "REPOSO"
        print("[OK] API edge: POST telemetria, GET datos, JSONL")
    finally:
        httpd.shutdown()
        httpd.server_close()
        th.join(timeout=3)


def main() -> None:
    with tempfile.TemporaryDirectory() as td:
        d = Path(td)
        probar_protocolo(d)
        probar_inferencia_host(d)
        probar_servidor(d)
    print("[OK] Pruebas de HOST completadas. Faltan build ESP-IDF y pruebas fisicas.")


if __name__ == "__main__":
    main()