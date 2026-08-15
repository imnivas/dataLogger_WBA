#!/usr/bin/env bash
# Headless STM32CubeIDE build: ./scripts/build.sh <build|clean> [--release|--debug]
#
# Uses STM32CubeIDE's Eclipse/CDT headless builder to compile (or clean)
# the STM32CubeIDE/ project from the command line, without opening the
# GUI. This imports the project into a disposable workspace under
# .stm32cubeide-workspace/ so it never touches your interactive IDE
# workspace/.metadata. It only compiles the sources on disk as-is —
# it does not regenerate code from the .ioc file. Pin/peripheral
# reassignment via CubeMX must still be done in STM32CubeIDE itself.
set -euo pipefail

usage() {
  echo "Usage: $0 <build|clean> [--release|--debug]" >&2
}

ACTION=""
CONFIG="Release"
for arg in "$@"; do
  case "$arg" in
    build|clean) ACTION="$arg" ;;
    --debug) CONFIG="Debug" ;;
    --release) CONFIG="Release" ;;
    *)
      usage
      exit 1
      ;;
  esac
done

if [[ -z "$ACTION" ]]; then
  usage
  exit 1
fi

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
WORKSPACE_DIR="$PROJECT_DIR/.stm32cubeide-workspace"
PROJECT_NAME="BLE_p2pServer_ota"

find_cubeide_bin() {
  if [[ -n "${STM32CUBEIDE_APP:-}" ]]; then
    echo "$STM32CUBEIDE_APP/Contents/MacOs/stm32cubeide"
    return
  fi
  local candidate="/Applications/STM32CubeIDE.app/Contents/MacOs/stm32cubeide"
  if [[ -x "$candidate" ]]; then
    echo "$candidate"
    return
  fi
  local found
  found="$(/usr/bin/find /Applications -maxdepth 1 -iname 'STM32CubeIDE*.app' -print -quit 2>/dev/null || true)"
  if [[ -n "$found" ]]; then
    echo "$found/Contents/MacOs/stm32cubeide"
    return
  fi
  echo ""
}

CUBEIDE_BIN="$(find_cubeide_bin)"
if [[ -z "$CUBEIDE_BIN" || ! -x "$CUBEIDE_BIN" ]]; then
  echo "error: could not find STM32CubeIDE. Set STM32CUBEIDE_APP=/path/to/STM32CubeIDE.app or install it under /Applications." >&2
  exit 1
fi

mkdir -p "$WORKSPACE_DIR"

ELF="$PROJECT_DIR/STM32CubeIDE/$CONFIG/$PROJECT_NAME.elf"
BIN="$PROJECT_DIR/STM32CubeIDE/$CONFIG/$PROJECT_NAME.bin"

if [[ "$ACTION" == "build" ]]; then
  BEFORE_MTIME="$(stat -f %m "$ELF" 2>/dev/null || echo 0)"

  echo "Building $PROJECT_NAME/$CONFIG with $CUBEIDE_BIN ..."
  LOG_FILE="$(mktemp)"
  "$CUBEIDE_BIN" -nosplash \
    -application org.eclipse.cdt.managedbuilder.core.headlessbuild \
    -data "$WORKSPACE_DIR" \
    -import "$PROJECT_DIR/STM32CubeIDE" \
    -cleanBuild "$PROJECT_NAME/$CONFIG" \
    2>&1 | tee "$LOG_FILE" || true
  BUILD_STATUS=${PIPESTATUS[0]}

  AFTER_MTIME="$(stat -f %m "$ELF" 2>/dev/null || echo 0)"

  if [[ $BUILD_STATUS -ne 0 ]] || [[ ! -f "$ELF" ]] || [[ "$AFTER_MTIME" == "$BEFORE_MTIME" ]] || grep -qiE '^[0-9]+ errors?,' "$LOG_FILE"; then
    echo "error: build failed." >&2
    rm -f "$LOG_FILE"
    exit 1
  fi
  rm -f "$LOG_FILE"

  echo "Build succeeded:"
  echo "  $ELF"
  [[ -f "$BIN" ]] && echo "  $BIN"
else
  # The headless builder only supports -build/-cleanBuild, no standalone
  # clean-only flag, so drive the generated makefile's own `clean` target
  # instead (same one STM32CubeIDE's GUI "Clean" action invokes).
  MAKEFILE_DIR="$PROJECT_DIR/STM32CubeIDE/$CONFIG"
  if [[ ! -f "$MAKEFILE_DIR/makefile" ]]; then
    echo "error: no makefile found at $MAKEFILE_DIR (nothing to clean)." >&2
    exit 1
  fi

  echo "Cleaning $PROJECT_NAME/$CONFIG ..."
  if ! (cd "$MAKEFILE_DIR" && make clean); then
    echo "error: clean failed." >&2
    exit 1
  fi

  echo "Clean succeeded for $CONFIG."
fi
