#!/usr/bin/env python3
"""Copia la imagen compilada de la Waveshare al servidor edge.

Ejecutar despues de aumentar IOT_VERSION_FIRMWARE y ejecutar idf.py build.
"""
import argparse
from pathlib import Path
import shutil
import hashlib

p = argparse.ArgumentParser()
p.add_argument("binario", type=Path, help="Ruta a build/iot_waveshare.bin")
args = p.parse_args()
if not args.binario.is_file():
    p.error("El binario no existe; compila primero en el proyecto Waveshare")
if args.binario.stat().st_size < 32768:
    p.error("Imagen muy pequena: comprueba que es un firmware valido")

dest = Path(__file__).resolve().parent / "firmware.bin"
shutil.copy2(args.binario, dest)
print("Imagen publicada:", dest)
print("Bytes:", dest.stat().st_size)
print("SHA256:", hashlib.sha256(dest.read_bytes()).hexdigest())
print("El servidor edge publicara GET /firmware.bin")