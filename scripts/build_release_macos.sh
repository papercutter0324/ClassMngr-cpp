#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
QT_PREFIX="${QT_MACOS_PREFIX:-}"
PRESET="macos-clang-release"
TARGET_MINIMUM_VERSION="14.4"
QT_VERSION="${QT_VERSION:-6.12.0}"
BUILD_DIR="${CLASSMNGR_RELEASE_BUILD_DIR:-${PROJECT_ROOT}/build/macos-clang-release-next-package}"
OUTPUT_DIR="${CLASSMNGR_RELEASE_OUTPUT_DIR:-${BUILD_DIR}/release-output}"

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
    -B "${BUILD_DIR}" \
    "-DCMAKE_PREFIX_PATH=${QT_PREFIX}" \
    -DCLASSMNGR_NEXT_RELEASE_PACKAGE=ON \
    "-DCLASSMNGR_INSTALLER_OUTPUT_DIR=${OUTPUT_DIR}" \
    "$@"
cmake --build "${BUILD_DIR}" --target ClassMngrInstaller --parallel

STAGED_APP="${BUILD_DIR}/installer-stage/ClassMngr.app"
APP_EXECUTABLE="${STAGED_APP}/Contents/MacOS/ClassMngr"
DISK_IMAGE="${OUTPUT_DIR}/ClassMngr-${PROJECT_VERSION}-macos-universal.dmg"
CHECKSUMS="${OUTPUT_DIR}/checksums-macos.txt"
PACKAGE_REPORT="${BUILD_DIR}/reports/macos-next-package.json"

if [[ ! -f "${APP_EXECUTABLE}" ]]; then
    echo "Expected staged app was not created: ${STAGED_APP}" >&2
    exit 1
fi

python "${PROJECT_ROOT}/scripts/verify_next_package.py" verify \
    --package-root "${BUILD_DIR}/installer-stage" \
    --platform macos \
    --architecture universal \
    --version "${PROJECT_VERSION}" \
    --qt-version "${QT_VERSION}" \
    --report "${PACKAGE_REPORT}"

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
SIGNATURE_DETAILS="$(codesign -dv --verbose=4 "${STAGED_APP}" 2>&1)"
if [[ "${SIGNATURE_DETAILS}" != *"Signature=adhoc"* ]]; then
    echo "Expected an ad-hoc macOS app signature; no Developer ID signature was requested." >&2
    exit 1
fi

if [[ ! -f "${DISK_IMAGE}" ]]; then
    echo "Expected disk image was not created: ${DISK_IMAGE}" >&2
    exit 1
fi

hdiutil verify "${DISK_IMAGE}"

DMG_MOUNT_DIR="$(mktemp -d "${TMPDIR:-/tmp}/classmngr-release-dmg.XXXXXX")"
DMG_MOUNTED=0
detach_dmg_mount() {
    local max_attempts="${1:-1}"
    local attempt
    local status

    for ((attempt = 1; attempt <= max_attempts; attempt++)); do
        if hdiutil detach "${DMG_MOUNT_DIR}"; then
            DMG_MOUNTED=0
            return 0
        else
            status=$?
        fi

        echo "hdiutil detach attempt ${attempt}/${max_attempts} failed with exit status ${status}." >&2
        if (( status != 16 || attempt == max_attempts )); then
            return "${status}"
        fi

        if sleep "${attempt}"; then
            :
        else
            status=$?
            echo "Unable to wait before retrying hdiutil detach (exit status ${status})." >&2
            return "${status}"
        fi
    done
}

cleanup_dmg_mount() {
    local exit_status=$?
    local detach_status

    trap - EXIT
    if (( DMG_MOUNTED )); then
        if detach_dmg_mount; then
            :
        else
            detach_status=$?
            echo "Warning: final hdiutil detach attempt failed with exit status ${detach_status}." >&2
        fi
    fi
    if (( ! DMG_MOUNTED )); then
        if rmdir "${DMG_MOUNT_DIR}"; then
            :
        else
            echo "Warning: unable to remove temporary disk-image mount directory: ${DMG_MOUNT_DIR}" >&2
        fi
    fi
    exit "${exit_status}"
}
trap cleanup_dmg_mount EXIT
hdiutil attach "${DISK_IMAGE}" \
    -readonly \
    -nobrowse \
    -quiet \
    -mountpoint "${DMG_MOUNT_DIR}"
DMG_MOUNTED=1
if [[ ! -d "${DMG_MOUNT_DIR}/ClassMngr.app" ]]; then
    echo "The disk image does not contain ClassMngr.app at its root." >&2
    exit 1
fi
if [[ ! -L "${DMG_MOUNT_DIR}/Applications" \
    || "$(readlink "${DMG_MOUNT_DIR}/Applications")" != "/Applications" ]]; then
    echo "The disk image is missing its /Applications shortcut." >&2
    exit 1
fi
python "${PROJECT_ROOT}/scripts/verify_next_package.py" verify \
    --package-root "${DMG_MOUNT_DIR}" \
    --platform macos \
    --architecture universal \
    --version "${PROJECT_VERSION}" \
    --qt-version "${QT_VERSION}"
if detach_dmg_mount 3; then
    if rmdir "${DMG_MOUNT_DIR}"; then
        trap - EXIT
    else
        echo "Warning: unable to remove temporary disk-image mount directory: ${DMG_MOUNT_DIR}" >&2
    fi
else
    DETACH_STATUS=$?
    echo "Warning: verified package disk image remains mounted after detach attempt(s) (exit status ${DETACH_STATUS}); continuing to checksum generation. The EXIT trap will make one final detach attempt." >&2
fi

HASH="$(shasum -a 256 "${DISK_IMAGE}" | awk '{print $1}')"
printf '%s  %s\n' \
    "${HASH}" \
    "$(basename "${DISK_IMAGE}")" \
    > "${CHECKSUMS}"

echo "macOS release artifacts:"
echo "  ${DISK_IMAGE}"
echo "  ${CHECKSUMS}"
echo "  ${PACKAGE_REPORT}"
