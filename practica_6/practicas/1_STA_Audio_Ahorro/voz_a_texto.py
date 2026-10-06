"""Transcribe audio UDP del programa 1 con faster-whisper."""

import socket

import numpy as np
from faster_whisper import WhisperModel


IP_ESP32 = "192.168.8.212"  # equipo 12
PUERTO = 3333
FRECUENCIA = 16000
SEGUNDOS_POR_BLOQUE = 3
BYTES_POR_BLOQUE = FRECUENCIA * SEGUNDOS_POR_BLOQUE * 2


sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.settimeout(0.2)
sock.sendto(b"R", (IP_ESP32, PUERTO))

print("Cargando modelo Whisper...")
modelo = WhisperModel("base", device="cpu", compute_type="int8")
print(f"Esperando audio de {IP_ESP32}... (Ctrl+C para salir)")

pendiente = bytearray()
intentos_sin_datos = 0
try:
    while True:
        try:
            datos, _ = sock.recvfrom(2048)
        except socket.timeout:
            intentos_sin_datos += 1
            if intentos_sin_datos % 10 == 0:
                sock.sendto(b"R", (IP_ESP32, PUERTO))
            continue

        intentos_sin_datos = 0
        pendiente.extend(datos)
        while len(pendiente) >= BYTES_POR_BLOQUE:
            bloque = bytes(pendiente[:BYTES_POR_BLOQUE])
            del pendiente[:BYTES_POR_BLOQUE]

            audio = np.frombuffer(bloque, dtype=np.int16).astype(np.float32)
            audio /= 32768.0
            segmentos, _ = modelo.transcribe(
                audio,
                language="es",
                vad_filter=True,
            )
            for segmento in segmentos:
                texto = segmento.text.strip()
                if texto:
                    print(texto, flush=True)
finally:
    sock.close()
