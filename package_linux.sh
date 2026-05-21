#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "${SCRIPT_DIR}"

BUILD_DIR="build_linux"
PKG_ROOT="package"
PKG_DIR="${PKG_ROOT}/CodroidSDK-Linux-x64"

echo "=================================================="
echo "      Packaging Codroid SDK for Linux x64"
echo "=================================================="

if [ ! -f "${BUILD_DIR}/libCodroid.so" ]; then
    echo "[Error] ${BUILD_DIR}/libCodroid.so not found."
    echo "Run ./build_linux.sh first."
    exit 1
fi

rm -rf "${PKG_DIR}"
mkdir -p "${PKG_DIR}/include"
mkdir -p "${PKG_DIR}/include/Codroid"
mkdir -p "${PKG_DIR}/lib"
mkdir -p "${PKG_DIR}/examples"
mkdir -p "${PKG_DIR}/docs"

echo "[1/5] Copy headers..."
cp -a include/Codroid/. "${PKG_DIR}/include/Codroid/"

echo "[2/5] Copy runtime libraries..."
cp "${BUILD_DIR}/libCodroid.so" "${PKG_DIR}/lib/"
cp kinematics/libFk_Ik_so.so "${PKG_DIR}/lib/"
cp kinematics/libkdl.so "${PKG_DIR}/lib/"

echo "[3/5] Copy examples and docs..."
cp examples_client/*.cpp "${PKG_DIR}/examples/"
cp README.md "${PKG_DIR}/"
cp SDK_GUIDE.md "${PKG_DIR}/docs/"
if [ -f LICENSE ]; then
    cp LICENSE "${PKG_DIR}/"
fi

echo "[4/5] Generate package guide..."
cat > "${PKG_DIR}/README_PACKAGE.md" <<'EOF'
# Codroid SDK Linux x64 Package

## Contents

- `include/`: public headers
- `lib/`: runtime and link libraries
- `examples/`: example source files
- `docs/SDK_GUIDE.md`: SDK guide

## Link

Typical CMake setup:

```cmake
set(CODROID_SDK_ROOT "/path/to/CodroidSDK-Linux-x64")

target_include_directories(your_app PRIVATE
    ${CODROID_SDK_ROOT}/include
)

target_link_directories(your_app PRIVATE ${CODROID_SDK_ROOT}/lib)
target_link_libraries(your_app PRIVATE Codroid Fk_Ik_so kdl pthread)

set_target_properties(your_app PROPERTIES
    BUILD_WITH_INSTALL_RPATH TRUE
    INSTALL_RPATH "$ORIGIN;${CODROID_SDK_ROOT}/lib"
)
```

## Runtime

Keep these files reachable by the executable:

- `libCodroid.so`
- `libFk_Ik_so.so`
- `libkdl.so`

They can be placed beside the executable or found through `LD_LIBRARY_PATH` / RPATH.
EOF

ARCHIVE="${PKG_ROOT}/CodroidSDK-Linux-x64.tar.gz"
echo "[5/5] Create archive..."
rm -f "${ARCHIVE}"
tar -czf "${ARCHIVE}" -C "${PKG_ROOT}" "$(basename "${PKG_DIR}")"

echo "=================================================="
echo "Package created:"
echo "  dir : ${PKG_DIR}"
echo "  tar : ${ARCHIVE}"
echo "=================================================="
