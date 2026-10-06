#!/bin/bash

# --- Configuracion ---
PORT="${PORT:-/dev/ttyACM0}"
ESP_IDF_PATH="${ESP_IDF_PATH:-$HOME/.espressif/v6.0.2/esp-idf}"
TARGET="esp32s3"
ACCION="${1:-build}"

echo "==========================================="
echo "        Automatizacion de ESP-IDF          "
echo "==========================================="

# 1. Liberar el puerto serial solo cuando se necesita hardware
if [[ "$ACCION" == "flash" || "$ACCION" == "monitor" ]]; then
    echo "[1/3] Liberando el puerto $PORT..."
    if [[ -e "$PORT" ]]; then
        fuser -k "$PORT" 2>/dev/null || true
    else
        echo "Error: no se detecta $PORT. Usa PORT=/dev/ttyACM1 si cambio el puerto."
        exit 1
    fi
else
    echo "[1/2] Compilacion sin puerto serial."
fi

# 2. Exportar el entorno
echo "[2/3] Cargando entorno de ESP-IDF..."
if [[ -f "$ESP_IDF_PATH/export.sh" ]]; then
    source "$ESP_IDF_PATH/export.sh"
else
    echo "Error: No se encontró export.sh en $ESP_IDF_PATH"
    exit 1
fi

# 3. Ejecutar la accion solicitada. Por defecto solo compila.
case "$ACCION" in
    build)
        echo "[3/3] Compilando desde cero..."
        rm -rf build/
        idf.py set-target "$TARGET"
        idf.py build
        ;;
    flash)
        echo "[3/3] Compilando y flasheando..."
        idf.py -p "$PORT" build flash
        ;;
    monitor)
        echo "[3/3] Abriendo monitor..."
        idf.py -p "$PORT" monitor
        ;;
    *)
        echo "Uso: $0 [build|flash|monitor]"
        exit 2
        ;;
esac