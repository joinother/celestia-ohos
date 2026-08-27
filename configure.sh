#!/bin/bash
# Cross-compile Celestia for HarmonyOS (arm64-v8a) using Qt 6.12.0 OHOS kit
set -e

CEL=/Users/jiexuanyang/celestia-ohos
SRC=/Users/jiexuanyang/celestia-src
QTDIR=/Users/jiexuanyang/Qt/6.12.0/harmonyos_arm64_v8a
SDK=/Applications/DevEco-Studio.app/Contents/sdk/default/openharmony/native
ADDITIONAL=/Users/jiexuanyang/.local/opt/ohos/additional-packages
DEPPREFIX=$CEL/deps/prefix
NINJA=/Users/jiexuanyang/.local/opt/ninja/ninja

export PATH="/Users/jiexuanyang/.local/opt/cmake/CMake.app/Contents/bin:$PATH"

cmake -S "$SRC" -B "$CEL/build" -G Ninja \
  -DCMAKE_MAKE_PROGRAM=$NINJA \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_FLAGS="-Wno-register" \
  -DCMAKE_TOOLCHAIN_FILE=$QTDIR/lib/cmake/Qt6/qt.toolchain.cmake \
  -DQT_CHAINLOAD_TOOLCHAIN_FILE=$SDK/build/cmake/ohos.toolchain.cmake \
  -DQT_ADDITIONAL_PACKAGES_PREFIX_PATH="$SDK;$ADDITIONAL" \
  -DOHOS_ARCH=arm64-v8a \
  -DCMAKE_FIND_ROOT_PATH="$ADDITIONAL;$DEPPREFIX" \
  -DCMAKE_PREFIX_PATH="$DEPPREFIX" \
  -DBOOST_ROOT=$DEPPREFIX \
  -DEigen3_DIR=$DEPPREFIX/share/eigen3/cmake \
  -DCELESTIA_OHOS=ON \
  -DENABLE_QT6=ON \
  -DENABLE_GLES=ON \
  -DENABLE_CELX=OFF \
  -DENABLE_NLS=ON \
  -DENABLE_SDL=OFF \
  -DENABLE_WIN=OFF \
  -DENABLE_FFMPEG=OFF \
  -DENABLE_LIBAVIF=OFF \
  -DENABLE_MINIAUDIO=OFF \
  -DENABLE_TOOLS=OFF \
  -DENABLE_TESTS=OFF \
  -DENABLE_SPICE=OFF \
  -DUSE_ICU=OFF \
  -DUSE_MESHOPTIMIZER=OFF \
  -DUSE_WEFFCPP=OFF \
  -DCMAKE_INSTALL_PREFIX=$CEL/install \
  "$@"
