#!/bin/zsh
# Build a Resolume FFGL source plugin as a universal macOS .bundle with only the Command Line
# Tools clang (no Xcode, no CMake), and install it where Resolume Arena hot-reloads it.
#
#   ./build_generator.sh <Name> [--no-install]
#
# Expects source/plugins/<Name>/FFGL<Name>.cpp (+ .h). The plugin cpp must define
# `vertexShaderCode` and `static std::string GetFragmentShaderSource()` so the GLSL can be
# compiled offscreen first; a shader error fails the build instead of producing a blank clip.
set -e
cd "$(dirname "$0")"

NAME=${1:?usage: build_generator.sh <Name> [--no-install]}
INSTALL=1
[[ "${2:-}" == "--no-install" ]] && INSTALL=0

SRC=source/plugins/$NAME/FFGL$NAME.cpp
[[ -f $SRC ]] || { echo "missing $SRC"; exit 1; }
OUT=binaries/$NAME.bundle
DEST="$HOME/Documents/Resolume Arena/Extra Effects"
COMMON_FLAGS=(-std=c++17 -DTARGET_OS_MAC=1 -DNDEBUG=1 -Wno-deprecated-declarations -Wno-implicit-const-int-float-conversion -Isource/lib -I.)
FRAMEWORKS=(-framework OpenGL -framework Carbon -framework AppKit)

echo "== 1/3 shader compile check"
mkdir -p tests/bin
clang++ $COMMON_FLAGS $FRAMEWORKS -DPLUGIN_CPP="\"$SRC\"" \
  source/lib/FFGLSDK.cpp tests/shader_compile_test.cpp -o tests/bin/shader_test_$NAME
./tests/bin/shader_test_$NAME

echo "== 2/3 build bundle"
rm -rf "$OUT"
mkdir -p "$OUT/Contents/MacOS"
clang++ $COMMON_FLAGS -O2 -arch arm64 -arch x86_64 -mmacosx-version-min=10.15 \
  -bundle $FRAMEWORKS \
  source/lib/FFGLSDK.cpp "$SRC" \
  -o "$OUT/Contents/MacOS/$NAME"
sed -e "s/\${EXECUTABLE_NAME}/$NAME/" \
    -e "s/\$(PRODUCT_BUNDLE_IDENTIFIER)/${FFGL_BUNDLE_PREFIX:-com.ffgl-generators}.$NAME/" \
    build/osx/FFGLPlugin-Info.plist > "$OUT/Contents/Info.plist"
lipo -info "$OUT/Contents/MacOS/$NAME"
nm -gU "$OUT/Contents/MacOS/$NAME" | grep -q _plugMain || { echo "plugMain not exported"; exit 1; }
echo "Built $OUT"

if [[ $INSTALL == 1 ]]; then
  echo "== 3/3 install"
  mkdir -p "$DEST"
  rm -rf "$DEST/$NAME.bundle"
  cp -R "$OUT" "$DEST/"
  echo "Installed to $DEST/$NAME.bundle (Arena watches this folder and reloads automatically)"
else
  echo "== 3/3 install skipped (--no-install)"
fi
