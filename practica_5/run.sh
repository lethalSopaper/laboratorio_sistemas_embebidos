#!/bin/bash

# --- Configuración ---
PORT="/dev/ttyACM0"
ESP_IDF_PATH="/home/lethalsopaper/.espressif/v6.0.2/esp-idf"

echo "==========================================="
echo "  Automatización de ESP-IDF (Práctica 5)   "
echo "==========================================="

# 1. Liberar el puerto serial
echo "[1/4] Liberando el puerto $PORT..."
if [ -e "$PORT" ]; then
    # Intenta cerrar cualquier proceso que esté usando el puerto (silenciando errores)
    fuser -k $PORT 2>/dev/null
    echo "Puerto liberado o listo para usarse."
else
    echo "Advertencia: No se detecta $PORT. Verifica la conexión física USB."
fi

# 2. Exportar el entorno
echo "[2/4] Cargando entorno de ESP-IDF..."
if [ -f "$ESP_IDF_PATH/export.sh" ]; then
    # Usamos 'source' para que las variables apliquen a esta sesión
    source "$ESP_IDF_PATH/export.sh"
else
    echo "Error: No se encontró export.sh en $ESP_IDF_PATH"
    exit 1
fi

# 3. Limpieza profunda
echo "[3/4] Eliminando caché de compilación (Build desde cero)..."
rm -rf build/

# 4. Compilar, flashear y monitorear
echo "[4/4] Iniciando idf.py..."
idf.py -p $PORT flash monitor