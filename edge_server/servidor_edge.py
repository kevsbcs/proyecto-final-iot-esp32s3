#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Servidor EDGE local para ESP32-S3.

Endpoints:
    POST /api/telemetria   -> acepta JSON del nodo.
    GET  /api/datos       -> ultimas 100 muestras para dashboard.
    GET  /firmware.bin    -> imagen OTA solo si existe en esta carpeta.
    GET  /                -> dashboard visual autoactualizable.

ADVERTENCIA: HTTP SIN TLS NI AUTENTICACION. Solo laboratorio en LAN confiable.
No exponga el puerto en Internet y no use cargas sensibles.
"""

from __future__ import annotations

import argparse
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import json
from pathlib import Path
import statistics
import threading
from datetime import datetime, timezone

BASE = Path(__file__).resolve().parent
FIRMWARE = BASE / "firmware.bin"
HISTORIAL = BASE / "telemetria.jsonl"
REGISTROS: list[dict] = []
LOCK = threading.Lock()

HTML = r"""<!doctype html>
<html lang="es"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<meta http-equiv="refresh" content="60">
<title>IoT ESP32-S3 | EDGE</title>
<style>
:root{color-scheme:dark}body{background:#080f1f;color:#e8edf9;font-family:system-ui,Arial;margin:30px auto;max-width:950px;padding:0 15px}
h1{font-size:27px;letter-spacing:-1px}.muted{color:#9daac2}.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(180px,1fr));gap:12px}
.card{border:1px solid #30415d;background:#122039;padding:18px;border-radius:15px}.value{font-size:27px;font-weight:700;margin-top:6px;color:#82d9e9}
section{margin-top:20px}table{border-collapse:collapse;width:100%;font-size:13px}td,th{text-align:left;border-bottom:1px solid #33425a;padding:10px 6px}
.notice{color:#f7c77b;margin-top:22px}code{color:#95e9bf}
</style></head><body>
<h1>ESP32-S3 · Panel EDGE</h1><p class="muted">Arquitectura IoT: IA en nodo • ESP-NOW nodo-nodo • HTTP nodo-edge • OTA</p>
<div class="grid">
 <div class="card"><div class="muted">Ultimo gesto IA</div><div class="value" id="gesture">---</div></div>
 <div class="card"><div class="muted">Estado LED remoto</div><div class="value" id="led">---</div></div>
 <div class="card"><div class="muted">RTT ESP-NOW</div><div class="value" id="rtt">---</div></div>
 <div class="card"><div class="muted">Muestras recibidas</div><div class="value" id="count">0</div></div>
</div>
<section class="card"><h2>Ultimas mediciones</h2><table><thead><tr><th>UTC</th><th>AX</th><th>AY</th><th>AZ</th><th>IA</th><th>Conf.</th><th>Firmware</th></tr></thead><tbody id="rows"></tbody></table></section>
<p id="firmware" class="muted"></p>
<p class="notice">Servidor de laboratorio, HTTP sin cifrado. Mantener en una red privada.</p>
<script>
function f(v,d=2){return Number.isFinite(Number(v))?Number(v).toFixed(d):'--'}
async function update(){try{const r=await fetch('/api/datos',{cache:'no-store'}),d=await r.json();
const xs=d.ultimas||[],x=xs[xs.length-1]||{};
document.getElementById('gesture').textContent=x.gesto||'SIN DATOS';
document.getElementById('led').textContent=x.led===1?'ENCENDIDO':x.led===0?'APAGADO':'SIN ACK';
document.getElementById('rtt').textContent=Number(x.rtt_espnow_ms)>=0?f(x.rtt_espnow_ms)+' ms':'---';
document.getElementById('count').textContent=d.cantidad_total;
document.getElementById('firmware').textContent=d.firmware_disponible?'Firmware OTA publicado en /firmware.bin':'Aun no hay firmware.bin para OTA';
document.getElementById('rows').innerHTML=xs.slice(-15).reverse().map(s=>`<tr><td>${s._utc||''}</td><td>${f(s.ax,3)}</td><td>${f(s.ay,3)}</td><td>${f(s.az,3)}</td><td>${s.gesto||''}</td><td>${f(Number(s.confianza)*100,0)}%</td><td>${s.version||''}</td></tr>`).join('');
}catch(e){document.getElementById('gesture').textContent='SIN CONEXION';}}
update();setInterval(update,2000);
</script></body></html>"""


def leer_historial() -> None:
    """Recupera solo las ultimas lineas previas del log sin librerias externas."""
    if not HISTORIAL.exists():
        return
    with HISTORIAL.open("r", encoding="utf-8") as f:
        for linea in f:
            try:
                REGISTROS.append(json.loads(linea))
            except json.JSONDecodeError:
                continue
            if len(REGISTROS) > 100:
                REGISTROS.pop(0)


class EdgeHandler(BaseHTTPRequestHandler):
    """API HTTP del PC edge."""

    def _respuesta(self, codigo: int, body: bytes, tipo: str) -> None:
        self.send_response(codigo)
        self.send_header("Content-Type", tipo)
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self) -> None:
        """Sirve dashboard, historico o imagen OTA."""
        if self.path in ("/", "/index.html"):
            self._respuesta(200, HTML.encode("utf-8"), "text/html; charset=utf-8")
            return
        if self.path == "/api/datos":
            with LOCK:
                datos = {
                    "ultimas": REGISTROS[-100:].copy(),
                    "cantidad_total": self.server.cantidad_total,
                    "firmware_disponible": FIRMWARE.exists(),
                }
            self._respuesta(200, json.dumps(datos).encode("utf-8"), "application/json")
            return
        if self.path == "/firmware.bin":
            if not FIRMWARE.is_file():
                self._respuesta(404, b"No hay firmware.bin", "text/plain")
                return
            length = FIRMWARE.stat().st_size
            self.send_response(200)
            self.send_header("Content-Type", "application/octet-stream")
            self.send_header("Content-Length", str(length))
            self.end_headers()
            with FIRMWARE.open("rb") as f:
                while bloque := f.read(64 * 1024):
                    self.wfile.write(bloque)
            return
        self._respuesta(404, b"No encontrado", "text/plain")

    def do_POST(self) -> None:
        """Recibe un JSON de telemetria y lo guarda como JSONL."""
        if self.path != "/api/telemetria":
            self._respuesta(404, b"No encontrado", "text/plain")
            return
        try:
            length = int(self.headers.get("Content-Length", "0"))
            if length <= 0 or length > 4096:
                raise ValueError("tamano invalido")
            dato = json.loads(self.rfile.read(length).decode("utf-8"))
            if not isinstance(dato, dict) or "nodo" not in dato or "gesto" not in dato:
                raise ValueError("JSON sin nodo o gesto")
            dato["_utc"] = datetime.now(timezone.utc).isoformat(timespec="seconds")
        except (ValueError, UnicodeDecodeError, json.JSONDecodeError) as e:
            self._respuesta(400, str(e).encode("utf-8"), "text/plain")
            return

        with LOCK:
            REGISTROS.append(dato)
            if len(REGISTROS) > 100:
                REGISTROS.pop(0)
            self.server.cantidad_total += 1
            with HISTORIAL.open("a", encoding="utf-8") as f:
                f.write(json.dumps(dato, ensure_ascii=False) + "\n")
        print(f"[EDGE] {dato['_utc']} {dato['gesto']} "
              f"LED={dato.get('led')} RTT_NOW={dato.get('rtt_espnow_ms')} ms")
        self._respuesta(200, b'{"ok":true}', "application/json")


def main() -> None:
    parser = argparse.ArgumentParser(description="Servidor PC edge IoT ESP32-S3")
    parser.add_argument("--host", default="0.0.0.0", help="IP de escucha, 0.0.0.0 para LAN")
    parser.add_argument("--port", type=int, default=8000)
    opts = parser.parse_args()
    leer_historial()
    servidor = ThreadingHTTPServer((opts.host, opts.port), EdgeHandler)
    servidor.cantidad_total = len(REGISTROS)
    print(f"[EDGE] Panel http://127.0.0.1:{opts.port}")
    print(f"[EDGE] Para el ESP32 usar IP LAN del PC, NO 127.0.0.1")
    print("[EDGE] Red local de laboratorio; HTTP sin autenticacion")
    print(f"[EDGE] Imagen OTA: {FIRMWARE} {'(LISTA)' if FIRMWARE.exists() else '(AUSENTE)'}")
    try:
        servidor.serve_forever()
    except KeyboardInterrupt:
        print("\n[EDGE] Cerrando...")
    finally:
        servidor.server_close()


if __name__ == "__main__":
    main()