#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_TYPE="${1:-debug}"
LOCAL_PROPERTIES="${SCRIPT_DIR}/local.properties"

normalize_local_path() {
  local raw="$1"
  local path="${raw//\\\\/\\}"
  path="${path//\\:/:}"

  if command -v cygpath >/dev/null 2>&1; then
    cygpath -u "$path"
  else
    printf '%s\n' "$path"
  fi
}

read_local_property() {
  local key="$1"
  if [[ ! -f "${LOCAL_PROPERTIES}" ]]; then
    return 1
  fi

  local line
  line="$(grep -E "^${key}=" "${LOCAL_PROPERTIES}" | tail -n 1 || true)"
  if [[ -z "${line}" ]]; then
    return 1
  fi

  normalize_local_path "${line#*=}"
}

case "${BUILD_TYPE}" in
  debug|Debug)
    TASK="assembleDebug"
    APK_PATH="${SCRIPT_DIR}/app/build/outputs/apk/debug/app-debug.apk"
    ;;
  release|Release)
    TASK="assembleRelease"
    APK_PATH="${SCRIPT_DIR}/app/build/outputs/apk/release/app-release.apk"
    ;;
  *)
    echo "Usage: $0 [debug|release]" >&2
    exit 1
    ;;
esac

if [[ -z "${JAVA_HOME:-}" ]]; then
  echo "JAVA_HOME is not set" >&2
  exit 1
fi

if [[ -z "${ANDROID_HOME:-}" && -z "${ANDROID_SDK_ROOT:-}" ]]; then
  if sdk_dir="$(read_local_property "sdk.dir")"; then
    export ANDROID_HOME="${sdk_dir}"
    export ANDROID_SDK_ROOT="${sdk_dir}"
  else
    echo "ANDROID_HOME or ANDROID_SDK_ROOT is not set, and sdk.dir was not found in local.properties" >&2
    exit 1
  fi
fi

if [[ -z "${ANDROID_NDK_HOME:-}" && -z "${ANDROID_NDK_ROOT:-}" ]]; then
  if ndk_dir="$(read_local_property "ndk.dir")"; then
    export ANDROID_NDK_HOME="${ndk_dir}"
    export ANDROID_NDK_ROOT="${ndk_dir}"
  else
    echo "ANDROID_NDK_HOME or ANDROID_NDK_ROOT is not set, and ndk.dir was not found in local.properties" >&2
    exit 1
  fi
fi

if [[ -x "${SCRIPT_DIR}/gradlew" ]]; then
  GRADLE_CMD=("${SCRIPT_DIR}/gradlew")
elif command -v gradle >/dev/null 2>&1; then
  GRADLE_CMD=("gradle")
else
  echo "Gradle not found. Install gradle or add ./gradlew to android_example." >&2
  exit 1
fi

echo "Building Android APK with task: ${TASK}"
echo "ANDROID_HOME=${ANDROID_HOME:-${ANDROID_SDK_ROOT:-}}"
echo "ANDROID_NDK_HOME=${ANDROID_NDK_HOME:-${ANDROID_NDK_ROOT:-}}"
"${GRADLE_CMD[@]}" -p "${SCRIPT_DIR}" "${TASK}"

if [[ -f "${APK_PATH}" ]]; then
  echo "APK built successfully:"
  echo "${APK_PATH}"
else
  echo "Build finished, but APK was not found at:"
  echo "${APK_PATH}"
  exit 1
fi
