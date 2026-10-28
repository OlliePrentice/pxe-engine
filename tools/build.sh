#!/usr/bin/env bash
# Configure (first time only) and build pxe with the CMakePresets.json presets,
# then optionally run the game or the minimal example. macOS and Linux.
set -euo pipefail

usage() {
  cat <<'EOF'
Usage: tools/build.sh [debug|release] [game|example] [-- args...]

Builds the engine, the game and the minimal example with the CMake preset
(default: release; build dir build/<config>), configuring it first if the
build dir isn't configured yet, and prints the paths of the built binaries.

  debug | release   Build configuration (default: release).
  game | example    After a successful build, run the game (pxe) or the
                    minimal example (pxe_minimal) and exit with its exit code.
  -- args...        Arguments passed to the binary run (needs game or example).
  -h, --help        Print this help.

The binary runs in your current directory, so relative paths such as
--capture shot.png land where you are.

Examples:
  tools/build.sh
  tools/build.sh debug
  tools/build.sh release game -- --capture shot.png --scene hills
  tools/build.sh example -- --capture out.png
EOF
}

config=""
target=""
run_args=()
have_run_args=0

while (($# > 0)); do
  case "$1" in
    -h | --help)
      usage
      exit 0
      ;;
    debug | release)
      if [[ -n "$config" ]]; then
        echo "build.sh: configuration given twice: $config and $1" >&2
        usage >&2
        exit 2
      fi
      config="$1"
      ;;
    game | example)
      if [[ -n "$target" ]]; then
        echo "build.sh: target given twice: $target and $1" >&2
        usage >&2
        exit 2
      fi
      target="$1"
      ;;
    --)
      shift
      run_args=("$@")
      have_run_args=1
      break
      ;;
    *)
      echo "build.sh: unknown argument: $1" >&2
      usage >&2
      exit 2
      ;;
  esac
  shift
done

if [[ -z "$config" ]]; then
  config="release"
fi
if ((have_run_args)) && [[ -z "$target" ]]; then
  echo "build.sh: arguments after -- need game or example" >&2
  usage >&2
  exit 2
fi

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir="$repo_root/build/$config"

# cmake --preset reads CMakePresets.json from the working directory, so run
# CMake from the repo root in subshells; the caller's directory is unchanged.
if [[ ! -f "$build_dir/CMakeCache.txt" ]]; then
  (cd "$repo_root" && cmake --preset "$config")
fi
(cd "$repo_root" && cmake --build --preset "$config")

# Single-config generators put a binary in the target's build dir; multi-config
# ones (Xcode, Ninja Multi-Config) in a per-configuration subdirectory.
find_binary() {
  local dir="$1" name="$2" config_dir
  case "$config" in
    debug) config_dir="Debug" ;;
    release) config_dir="Release" ;;
  esac
  if [[ -x "$dir/$name" ]]; then
    echo "$dir/$name"
  elif [[ -x "$dir/$config_dir/$name" ]]; then
    echo "$dir/$config_dir/$name"
  else
    echo "build.sh: built binary $name not found in $dir" >&2
    return 1
  fi
}

game_bin="$(find_binary "$build_dir/game" pxe)"
example_bin="$(find_binary "$build_dir/examples/minimal" pxe_minimal)"

echo "game:    $game_bin"
echo "example: $example_bin"

case "$target" in
  game) bin="$game_bin" ;;
  example) bin="$example_bin" ;;
  *) exit 0 ;;
esac

# Bash 3.2 (macOS) treats an empty array as unset under set -u, hence the guard.
exec "$bin" ${run_args[@]+"${run_args[@]}"}
