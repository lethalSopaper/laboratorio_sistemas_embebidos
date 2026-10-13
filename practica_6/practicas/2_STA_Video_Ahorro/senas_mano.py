"""Detecta los puntos de una mano en el video UDP del programa 2."""

import math
from pathlib import Path
import socket

import cv2
import mediapipe as mp
import numpy as np
from mediapipe.tasks.python import BaseOptions, vision


IP_ESP32 = "192.168.8.212"  # equipo 12
PUERTO = 3333
ANCHO = 96
ALTO = 96
ESCALA = 5


def dibujar_mano(foto, landmarks):
    alto, ancho = foto.shape[:2]
    puntos = []
    for punto in landmarks:
        x = int(punto.x * ancho)
        y = int(punto.y * alto)
        puntos.append((x, y))
        cv2.circle(foto, (x, y), 2, (255, 255, 255), -1)

    for conexion in vision.HandLandmarksConnections.HAND_CONNECTIONS:
        inicio = puntos[conexion.start]
        fin = puntos[conexion.end]
        cv2.line(foto, inicio, fin, (255, 255, 255), 1)

    pulgar = puntos[4]
    indice = puntos[8]
    distancia = math.hypot(indice[0] - pulgar[0], indice[1] - pulgar[1])
    cv2.line(foto, pulgar, indice, (0, 255, 255), 2)
    cv2.putText(
        foto,
        f"Distancia: {distancia:.1f} px",
        (3, 12),
        cv2.FONT_HERSHEY_SIMPLEX,
        0.35,
        (0, 255, 255),
        1,
        cv2.LINE_AA,
    )


ruta_modelo = Path(__file__).parent / "hand_landmarker.task"
if not ruta_modelo.is_file():
    raise FileNotFoundError(
        f"No se encontro el modelo de MediaPipe: {ruta_modelo}"
    )

opciones = vision.HandLandmarkerOptions(
    base_options=BaseOptions(model_asset_path=str(ruta_modelo)),
    num_hands=1,
)
detector = vision.HandLandmarker.create_from_options(opciones)

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.settimeout(0.2)
try:
    sock.sendto(b"R", (IP_ESP32, PUERTO))
except OSError as error:
    print(
        f"No hay ruta hacia {IP_ESP32}: {error}. "
        "Conecta la PC a la red WiFi del ESP32."
    )

negro = np.zeros((ALTO * ESCALA, ANCHO * ESCALA, 3), dtype=np.uint8)
cv2.imshow("Senas de mano", negro)
cv2.waitKey(1)
print(f"Esperando video de {IP_ESP32}... (q o Ctrl+C para salir)")

esperas = 0
try:
    while True:
        try:
            datos, _ = sock.recvfrom(65535)
        except socket.timeout:
            esperas += 1
            cv2.waitKey(1)
            if esperas % 10 == 0:
                try:
                    sock.sendto(b"R", (IP_ESP32, PUERTO))
                except OSError:
                    print(
                        f"Sin ruta hacia {IP_ESP32}; esperando la red...",
                        flush=True,
                    )
            continue

        esperas = 0
        if len(datos) != ANCHO * ALTO:
            continue

        gris = np.frombuffer(datos, dtype=np.uint8).reshape(ALTO, ANCHO)
        rgb = cv2.cvtColor(gris, cv2.COLOR_GRAY2RGB)
        imagen = mp.Image(image_format=mp.ImageFormat.SRGB, data=rgb)
        resultado = detector.detect(imagen)

        foto = cv2.cvtColor(gris, cv2.COLOR_GRAY2BGR)
        for mano in resultado.hand_landmarks:
            dibujar_mano(foto, mano)

        foto = cv2.resize(
            foto,
            None,
            fx=ESCALA,
            fy=ESCALA,
            interpolation=cv2.INTER_NEAREST,
        )
        cv2.imshow("Senas de mano", foto)
        if cv2.waitKey(1) & 0xFF == ord("q"):
            break
finally:
    detector.close()
    sock.close()
    cv2.destroyAllWindows()
