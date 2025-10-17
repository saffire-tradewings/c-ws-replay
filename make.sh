#!/usr/bin/env bash
set -euo pipefail

BUILD_DIR="build"
GENERATOR="${GENERATOR:-Unix Makefiles}" # or Ninja
PREFIX_DEFAULT="/usr/local"
PREFIX="${CMAKE_INSTALL_PREFIX:-$PREFIX_DEFAULT}"

print_usage() {
	cat <<'EOF'
Usage:
  ./make.sh [CMAKE_BUILD_OPTS...] [-- [TOOL_OPTS...]]

Common commands:
  ./make.sh --help                 Show this help
  ./make.sh --list                 List all available build targets
  ./make.sh                        Build default target
  ./make.sh --target install       Install everything to the configured prefix
  ./make.sh --target install_static    Build+install static library (one-shot)
  ./make.sh --target install_shared    Build+install shared library (one-shot)
  ./make.sh --target clean         Clean generated objects
  ./make.sh --target realclean     Remove entire build directory

Configuration:
  Environment variables:
    CMAKE_INSTALL_PREFIX=/path     Configure install prefix (default: /usr/local)
    GEN=Ninja                      Choose generator (default: Unix Makefiles)
  Examples:
    CMAKE_INSTALL_PREFIX=$HOME/.local ./make.sh --target install_static
    GEN=Ninja ./make.sh -- -j8
    DESTDIR=$PWD/stage ./make.sh --target install_shared

Notes:
  - The first run bootstraps the build directory automatically.
  - -- separates CMake build options from underlying tool options (make/ninja).
  - Use sudo for system-wide installs (e.g., /usr/local).
EOF
}

# Early help handling (no bootstrap required)
if [[ "${1:-}" == "--help" || "${1:-}" == "help" || "${1:-}" == "-h" ]]; then
	print_usage
	exit 0
fi

# Early list handling: if build dir not configured yet, configure a minimal cache first
if [[ "${1:-}" == "--list" ]]; then
	if [[ ! -d "${BUILD_DIR}" || ! -f "${BUILD_DIR}/CMakeCache.txt" ]]; then
		echo "[bootstrap] configuring ${BUILD_DIR} (prefix=${PREFIX}, generator=${GENERATOR})"
		cmake -S . -B "${BUILD_DIR}" -G "${GENERATOR}" -DCMAKE_INSTALL_PREFIX="${PREFIX}"
	fi
	echo "[targets]"
	cmake --build "${BUILD_DIR}" --target help | sed -n '1,200p'
	exit 0
fi

# Bootstrap configure if needed
if [[ ! -d "${BUILD_DIR}" || ! -f "${BUILD_DIR}/CMakeCache.txt" ]]; then
	echo "[bootstrap] configuring ${BUILD_DIR} (prefix=${PREFIX}, generator=${GENERATOR})"
	cmake -S . -B "${BUILD_DIR}" -G "${GENERATOR}" -DCMAKE_INSTALL_PREFIX="${PREFIX}"
fi

# Split args into CMake build opts and tool opts
cmake_args=()
tool_args=()
seen_ddash="false"
for a in "$@"; do
	if [[ "${a}" == "--" ]]; then
		seen_ddash="true"
		continue
	fi
	if [[ "${seen_ddash}" == "true" ]]; then
		tool_args+=("${a}")
	else
		cmake_args+=("${a}")
	fi
done

echo "[bootstrap] cmake --build ${BUILD_DIR} ${cmake_args[*]} -- ${tool_args[*]}"
cmake --build "${BUILD_DIR}" "${cmake_args[@]}" -- "${tool_args[@]}"
