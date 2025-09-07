#!/usr/bin/env bash
set -euo pipefail

# Проверка количества параметров
if [ $# -gt 1 ]; then
    echo "Error: too many parameters. Pass -d for debug-build or nothing for release-build"
    exit 1
fi

# Проверка параметра
if [ $# -eq 0 ]; then
    BUILD_MODE="release"
elif [ "$1" = "-d" ]; then
    BUILD_MODE="debug"
else
    echo "Error: Unexpected parameter. Use -d for debug-build or nothing for release-build"
    exit 1
fi
echo "[*] build: $BUILD_MODE"

# ---- НАСТРОЙКИ ----
ROOT_DIR="$(pwd)/../src/3rd_party/chromium"
CRASHPAD_DIR="$ROOT_DIR/crashpad/crashpad"
DEPOT_TOOLS_DIR="$ROOT_DIR/depot_tools"
BUILD_DIR="$CRASHPAD_DIR/out/release"

INCLUDE_DIR="$(pwd)/../src/3rd_party/crashpad/include/crashpad"
LIB_DIR="$(pwd)/../src/3rd_party/crashpad/include/crashpad"

echo "[*] Crashpad build script"
echo "    CRASHPAD_DIR: $CRASHPAD_DIR"
echo "    DEPOT_TOOLS_DIR: $DEPOT_TOOLS_DIR"
echo "    BUILD_DIR: $BUILD_DIR"
echo

# 1. Определяем ОС
OS="$(uname -s)"
case "$OS" in
    Linux*) PLATFORM=linux ;;
    Darwin*) PLATFORM=mac ;;
    MINGW*|MSYS*|CYGWIN*) PLATFORM=win ;;
    *) echo "Unsupported OS: $OS" && exit 1 ;;
esac

echo "[*] Detected platform: $PLATFORM"
echo

# 2. Скачиваем depot_tools, если нет
if [ ! -d "$DEPOT_TOOLS_DIR" ]; then
    echo "[*] Installing depot_tools..."
    git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git "$DEPOT_TOOLS_DIR"
else
    echo "[*] depot_tools already exists, updating..."
    cd "$DEPOT_TOOLS_DIR"
	git checkout main
	git pull
fi
export PATH="$DEPOT_TOOLS_DIR:$PATH"

if [ "$PLATFORM" = "win" ]; then
    echo "[*] Updating depot_tools..."
    cd "$DEPOT_TOOLS_DIR"
    ./update_depot_tools.bat
fi

# 3. Скачиваем crashpad, если нет
if [ ! -d "$CRASHPAD_DIR" ]; then
	echo "[*] Getting the crashpad source..."
    mkdir -p "$(dirname "$CRASHPAD_DIR")"
    cd "$(dirname "$CRASHPAD_DIR")"
	fetch crashpad
else
    echo "[*] crashpad already exists"
fi

# 4. Обновляем crashpad
echo "[*] Syncing crashpad..."
cd "$CRASHPAD_DIR"
git checkout main
git pull -r
gclient syn

# 5. Генерируем билд через gn
mkdir -p out/release
touch out/release/args.gn
if [ "$PLATFORM" = "win" ]; then
  if [ "$BUILD_MODE" = "debug" ]; then
    echo extra_cflags=\"/MDd\" > out/release/args.gn
  else
    echo extra_cflags=\"/MD\" > out/release/args.gn
  fi

elif [ "$PLATFORM" = "mac" ]; then
	echo target_cpu=\"x64\" > out/release/args.gn
fi
# Выводим содержимое args.gn для отладки
echo "[*] Contents of args.gn:"
cat out/release/args.gn

echo "[*] Generating build configuration..."
cd "$CRASHPAD_DIR"
gn gen out/release

# 6. Собираем через ninja
echo "[*] Building crashpad..."
ninja -v -C out/release

echo
echo "[*] Build completed successfully!"
echo "    crashpad_handler: $BUILD_DIR/crashpad_handler"
