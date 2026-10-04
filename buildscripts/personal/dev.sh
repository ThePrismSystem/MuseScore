#!/usr/bin/env bash
# Personal development helper for the fork. Builds and tests run inside an
# Ubuntu 22.04 container that mirrors the Linux CI runner, so the host needs
# nothing but Docker.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
IMAGE="musescore-fork-dev:4.7"
BUILD_DIR="build.debug"
CCACHE_HOST_DIR="${HOME}/.cache/musescore-fork-ccache"

in_container() {
    mkdir -p "${CCACHE_HOST_DIR}"
    docker run --rm \
        -u "$(id -u):$(id -g)" \
        -e HOME=/tmp \
        -e CCACHE_DIR=/ccache \
        -e QT_QPA_PLATFORM=minimal:enable_fonts \
        -v "${ROOT}:/src" \
        -v "${CCACHE_HOST_DIR}:/ccache" \
        -w /src \
        "${IMAGE}" \
        bash -c "source /opt/build_tools/environment.sh >/dev/null && $1"
}

case "${1:-}" in
image)
    docker build -t "${IMAGE}" -f "${ROOT}/buildscripts/personal/Dockerfile" "${ROOT}/buildscripts/ci/linux"
    ;;
configure)
    in_container "cmake -S . -B ${BUILD_DIR} -GNinja \
        -DCMAKE_BUILD_TYPE=Debug \
        -DCMAKE_C_COMPILER_LAUNCHER=ccache \
        -DCMAKE_CXX_COMPILER_LAUNCHER=ccache \
        -DMUSE_APP_BUILD_MODE=dev \
        -DMUE_DOWNLOAD_SOUNDFONT=OFF \
        -DMUSE_ENABLE_UNIT_TESTS=ON \
        -DMUSE_COMPILE_USE_UNITY=ON"
    ;;
build)
    shift
    in_container "cmake --build ${BUILD_DIR} --target $*"
    ;;
test)
    shift
    in_container "cd ${BUILD_DIR} && ctest --output-on-failure -R '$1'"
    ;;
style)
    shift
    in_container "cmake -P buildscripts/ci/checkcodestyle/download_tools.cmake >/dev/null \
        && for f in $*; do cmake -P muse/tools/codestyle/format_file.cmake \"\$f\"; done"
    ;;
*)
    echo "usage: dev.sh image | configure | build <targets...> | test <ctest-regex> | style <files...>" >&2
    exit 1
    ;;
esac
