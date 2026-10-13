---
pagestyle: empty
lang: es
geometry: margin=1.5cm
papersize: letter
fontsize: 10pt
mainfont: DejaVu Sans
---

\begin{center}\Large\textbf{Clase de energía: qué hacer}\end{center}

# 1. Tu IP

IP del ESP32 = **192.168.8.(200 + número de equipo)**. Para el equipo 12:
**192.168.8.212**.

# 2. Cambiar la IP (4 lugares)

En `practicas/1_STA_Audio_Ahorro/`:

- `1_STA_Audio_Ahorro.c` → `#define IP_ESP32 ESP_IP4TOADDR(192, 168, 8, 2xx)`
- `graficar_audio.py` → `IP_ESP32 = "192.168.8.2xx"`

En `practicas/2_STA_Video_Ahorro/`:

- `2_STA_Video_Ahorro.c` → `#define IP_ESP32 ESP_IP4TOADDR(192, 168, 8, 2xx)`
- `ver_vigia.py` → `IP_ESP32 = "192.168.8.2xx"`

No cambies `WIFI_SSID`, `WIFI_PASS` ni `IP_ROUTER`.

Para el equipo 12, usa `192.168.8.212` en cada archivo que tenga `IP_ESP32`.

# 3. Elegir el programa

En `main/main.c` deja **una sola** línea `#define PRACTICA` sin comentar.

# 4. Compilar y cargar

duerme y no se deja cargar). Una de dos: - Desconecta, deja presionado **B**, conecta, suelta **B**. - Con la placa conectada: deja presionado **B**, presiona y suelta
**R**, suelta **B**.
terminación (`/dev/ttyACM0` → `/dev/ttyACM1`, `COM3` → `COM4`).
Vuelve a elegirlo en la barra inferior de VS Code antes de Flash.

# 5. Correr en la PC (conectada al mismo WiFi)

Activa tu ambiente:

```
source ~/venv_embebidos/bin/activate        # Linux
$HOME\venv_embebidos\Scripts\Activate.ps1   # Windows
```

Solo si te falta alguna librería, instálala:

```
pip install numpy            # los dos scripts
pip install pyqtgraph        # graficar_audio.py
pip install PyQt5            # graficar_audio.py
pip install opencv-python    # ver_vigia.py
```

Corre el script del programa que cargaste:

```
python practicas/1_STA_Audio_Ahorro/graficar_audio.py
python practicas/2_STA_Video_Ahorro/ver_vigia.py
```

El video necesita `ffmpeg` instalado. Ctrl+C lo guarda en un `.mp4`.

# 6. Reconocimiento con IA

Los scripts de la practica parten de los mismos receptores UDP, pero agregan
el reconocimiento en la PC:

- Audio: `voz_a_texto.py` junta 3 segundos y usa `faster-whisper` en espanol.
- Video: `senas_mano.py` usa MediaPipe Hand Landmarker y dibuja los 21 puntos,
  las conexiones, la linea entre pulgar e indice y su distancia en pixeles.

Con `venv_embebidos` activado, instala las librerias:

```
pip install numpy faster-whisper opencv-python mediapipe
```

Descarga `hand_landmarker.task` y guardalo junto a `senas_mano.py`:

```
https://storage.googleapis.com/mediapipe-models/hand_landmarker/hand_landmarker/float16/latest/hand_landmarker.task
```

Despues de compilar y cargar el firmware, ejecuta el script correspondiente:

```
python practicas/1_STA_Audio_Ahorro/voz_a_texto.py
python practicas/2_STA_Video_Ahorro/senas_mano.py
```

El script `run.sh` compila por defecto. No requiere conectar la placa para
compilar y fija el target correcto de la XIAO ESP32-S3:

```
./run.sh build
PORT=/dev/ttyACM1 ./run.sh flash
PORT=/dev/ttyACM1 ./run.sh monitor
```

Antes de `flash`, entra en modo descarga y verifica el puerto. El monitor no
es necesario para esta practica: la salida de audio y video se observa en la
PC mediante los scripts.
