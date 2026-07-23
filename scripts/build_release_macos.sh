#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
QT_PREFIX="${QT_MACOS_PREFIX:-}"
PRESET="macos-clang-release"

if [[ -z "${QT_PREFIX}" ]]; then
    echo "QT_MACOS_PREFIX must point at a Qt macOS installation." >&2
    exit 1
fi

if [[ ! -f "${QT_PREFIX}/lib/cmake/Qt6/Qt6Config.cmake" ]]; then
    echo "QT_MACOS_PREFIX does not contain a Qt CMake package: ${QT_PREFIX}" >&2
    exit 1
fi

PROJECT_VERSION="$(
    awk '
        /project[[:space:]]*[(][[:space:]]*ClassMngr/ {
            in_project = 1
        }
        in_project && /VERSION[[:space:]]+[0-9]+\.[0-9]+\.[0-9]+/ {
            for (field = 1; field <= NF; ++field) {
                if ($field == "VERSION") {
                    print $(field + 1)
                    exit
                }
            }
        }
    ' "${PROJECT_ROOT}/CMakeLists.txt"
)"

if [[ -z "${PROJECT_VERSION}" ]]; then
    echo "Unable to read the ClassMngr version from CMakeLists.txt." >&2
    exit 1
fi

echo "Building ${PRESET} installer"
cmake \
    --fresh \
    --preset "${PRESET}" \
    "-DCMAKE_PREFIX_PATH=${QT_PREFIX}" \
    "$@"
cmake --build --preset "${PRESET}-installer"

STAGED_APP="${PROJECT_ROOT}/build/${PRESET}/installer-stage/ClassMngr.app"
APP_EXECUTABLE="${STAGED_APP}/Contents/MacOS/ClassMngr"
DISK_IMAGE="${PROJECT_ROOT}/dist/ClassMngr-${PROJECT_VERSION}-macos-universal.dmg"
CHECKSUMS="${PROJECT_ROOT}/dist/checksums-macos.txt"

if [[ ! -f "${APP_EXECUTABLE}" ]]; then
    echo "Expected staged app was not created: ${STAGED_APP}" >&2
    exit 1
fi

ARCHITECTURES="$(lipo -archs "${APP_EXECUTABLE}")"
if [[ " ${ARCHITECTURES} " != *" arm64 "* \
    || " ${ARCHITECTURES} " != *" x86_64 "* ]]; then
    echo "Expected a universal arm64/x86_64 app, found: ${ARCHITECTURES}" >&2
    exit 1
fi

MINIMUM_VERSIONS="$(
    xcrun vtool -show-build "${APP_EXECUTABLE}" \
        | awk '/minos/ { print $2 }' \
        | sort -u
)"
if [[ "${MINIMUM_VERSIONS}" != "13.0" ]]; then
    echo "Expected a macOS 13.0 deployment target, found: ${MINIMUM_VERSIONS}" >&2
    exit 1
fi

for driver in libqsqlmimer.dylib libqsqlodbc.dylib libqsqlpsql.dylib; do
    if [[ -e "${STAGED_APP}/Contents/PlugIns/sqldrivers/${driver}" ]]; then
        echo "Unexpected SQL driver was deployed: ${driver}" >&2
        exit 1
    fi
done

codesign --verify --deep --strict "${STAGED_APP}"

if [[ ! -f "${DISK_IMAGE}" ]]; then
    echo "Expected disk image was not created: ${DISK_IMAGE}" >&2
    exit 1
fi

hdiutil verify "${DISK_IMAGE}"

HASH="$(shasum -a 256 "${DISK_IMAGE}" | awk '{print $1}')"
printf '%s  %s\n' \
    "${HASH}" \
    "$(basename "${DISK_IMAGE}")" \
    > "${CHECKSUMS}"

echo "macOS release artifacts:"
echo "  ${DISK_IMAGE}"
echo "  ${CHECKSUMS}"
