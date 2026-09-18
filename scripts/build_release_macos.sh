#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
QT_PREFIX="${QT_MACOS_PREFIX:-}"
PRESET="macos-clang-release"
TARGET_MINIMUM_VERSION="14.4"

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
if [[ "${MINIMUM_VERSIONS}" != "${TARGET_MINIMUM_VERSION}" ]]; then
    echo "Expected a macOS ${TARGET_MINIMUM_VERSION} deployment target, found: ${MINIMUM_VERSIONS}" >&2
    exit 1
fi

PLIST_MINIMUM_VERSION="$(
    plutil -extract LSMinimumSystemVersion raw "${STAGED_APP}/Contents/Info.plist"
)"
if [[ "${PLIST_MINIMUM_VERSION}" != "${TARGET_MINIMUM_VERSION}" ]]; then
    echo "Expected LSMinimumSystemVersion ${TARGET_MINIMUM_VERSION}, found: ${PLIST_MINIMUM_VERSION}" >&2
    exit 1
fi

MACH_O_COUNT=0
while IFS= read -r -d '' BINARY; do
    if [[ "$(file -b "${BINARY}")" != *"Mach-O"* ]]; then
        continue
    fi

    BINARY_ARCHITECTURES="$(lipo -archs "${BINARY}")"
    if [[ " ${BINARY_ARCHITECTURES} " != *" arm64 "* \
        || " ${BINARY_ARCHITECTURES} " != *" x86_64 "* ]]; then
        echo "Expected a universal arm64/x86_64 bundle binary, found ${BINARY_ARCHITECTURES}: ${BINARY}" >&2
        exit 1
    fi

    if ! BINARY_MINIMUM_VERSIONS="$(
        xcrun vtool -show-build "${BINARY}" \
            | awk '/minos/ { print $2 }' \
            | sort -u
    )"; then
        echo "Unable to inspect the minimum OS version of: ${BINARY}" >&2
        exit 1
    fi
    if [[ -z "${BINARY_MINIMUM_VERSIONS}" ]]; then
        echo "No minimum OS version found for bundle binary: ${BINARY}" >&2
        exit 1
    fi

    while IFS= read -r BINARY_MINIMUM_VERSION; do
        if awk -F. -v found="${BINARY_MINIMUM_VERSION}" -v target="${TARGET_MINIMUM_VERSION}" '
            BEGIN {
                split(found, actual_parts, "[.]")
                split(target, target_parts, "[.]")
                found_major = actual_parts[1] + 0
                found_minor = actual_parts[2] + 0
                target_major = target_parts[1] + 0
                target_minor = target_parts[2] + 0
                if (found_major > target_major) {
                    exit 0
                }
                if (found_major == target_major && found_minor > target_minor) {
                    exit 0
                }
                exit 1
            }
        '; then
            echo "Bundle binary requires macOS ${BINARY_MINIMUM_VERSION}, newer than ${TARGET_MINIMUM_VERSION}: ${BINARY}" >&2
            exit 1
        fi
    done <<< "${BINARY_MINIMUM_VERSIONS}"

    ((MACH_O_COUNT += 1))
done < <(find "${STAGED_APP}/Contents" -type f -print0)

if (( MACH_O_COUNT == 0 )); then
    echo "No Mach-O binaries were found in the staged app: ${STAGED_APP}" >&2
    exit 1
fi

echo "Validated ${MACH_O_COUNT} universal bundle binaries for macOS ${TARGET_MINIMUM_VERSION} compatibility."

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
