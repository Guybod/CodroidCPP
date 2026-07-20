#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "${SCRIPT_DIR}"

BUILD_DIR="build_linux"
PKG_ROOT="package"
PKG_DIR="${PKG_ROOT}/CodroidSDK-Linux-x64"

PUBLIC_HEADERS=(
    "client.hpp"
    "types.hpp"
    "CodroidExport.h"
    "console_utf8.hpp"
    "cri_realtime_dispatcher.hpp"
    "trajectory_generator.hpp"
    "trajectory_types.hpp"
)

echo "=================================================="
echo "      Packaging Codroid SDK for Linux x64"
echo "=================================================="

if [ ! -f "${BUILD_DIR}/libCodroid.so" ]; then
    echo "[Error] ${BUILD_DIR}/libCodroid.so not found."
    echo "Run ./build_linux.sh first."
    exit 1
fi

if [ ! -d "third_party/nlohmann" ]; then
    echo "[Error] third_party/nlohmann not found. Run ./build_linux.sh first."
    exit 1
fi

rm -rf "${PKG_DIR}"
mkdir -p "${PKG_DIR}/include/Codroid"
mkdir -p "${PKG_DIR}/include/nlohmann"
mkdir -p "${PKG_DIR}/lib"
mkdir -p "${PKG_DIR}/examples"
mkdir -p "${PKG_DIR}/docs"

echo "[1/5] Copy public headers + vendored nlohmann..."
for h in "${PUBLIC_HEADERS[@]}"; do
    cp "include/Codroid/${h}" "${PKG_DIR}/include/Codroid/"
done
cp -a third_party/nlohmann/. "${PKG_DIR}/include/nlohmann/"

# Fail closed: no Asio / Controller in package
if [ -f "${PKG_DIR}/include/Codroid/CodroidController.h" ] || [ -f "${PKG_DIR}/include/Codroid/CodroidDefine.h" ]; then
    echo "[Error] Internal headers leaked into package."
    exit 1
fi
if grep -REn 'asio\.hpp' "${PKG_DIR}/include" >/dev/null 2>&1; then
    echo "[Error] Asio headers must not ship in customer package."
    exit 1
fi

echo "[2/5] Copy runtime library..."
cp "${BUILD_DIR}/libCodroid.so" "${PKG_DIR}/lib/"

echo "[3/5] Copy examples and docs..."
cp examples/*.cpp "${PKG_DIR}/examples/"
cp README.md "${PKG_DIR}/"
cp SDK_GUIDE.md "${PKG_DIR}/docs/" 2>/dev/null || true
if [ -f LICENSE ]; then
    cp LICENSE "${PKG_DIR}/"
fi

echo "[4/5] Generate package guide..."
cat > "${PKG_DIR}/README_PACKAGE.md" <<'EOF'
# Codroid SDK Linux x64 Package (v3.0.0)

## Customer usage

业务代码**只需**：

```cpp
#include "Codroid/client.hpp"
```

即可得到 `CodroidClient`、DTO、轨迹/CRI 下发，以及 `nlohmann::json`（已由 `client.hpp` 引入）。

**不需要** Asio、KDL，也**不必**再写 `#include <nlohmann/json.hpp>`。

## Contents

- `include/Codroid/`：公开头（入口 `client.hpp`）
- `include/nlohmann/`：随包提供的 JSON 头（供 `client.hpp` 使用）
- `lib/libCodroid.so`
- `examples/`：`CodroidClient` 示例

## Link

```cmake
set(CODROID_SDK_ROOT "/path/to/CodroidSDK-Linux-x64")

target_include_directories(your_app PRIVATE ${CODROID_SDK_ROOT}/include)
target_link_directories(your_app PRIVATE ${CODROID_SDK_ROOT}/lib)
target_link_libraries(your_app PRIVATE Codroid pthread)

set_target_properties(your_app PROPERTIES
    BUILD_WITH_INSTALL_RPATH TRUE
    INSTALL_RPATH "$ORIGIN;${CODROID_SDK_ROOT}/lib"
)
```

## Runtime

Keep `libCodroid.so` beside the executable or on `LD_LIBRARY_PATH` / RPATH.
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
