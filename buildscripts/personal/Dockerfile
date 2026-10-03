# Personal development image for the fork: Ubuntu 22.04 like the Linux CI
# runner, set up by the CI's own script, plus Qt 6.10.2 and ccache.
FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive TZ=Etc/UTC

RUN apt-get update \
    && apt-get install -y --no-install-recommends sudo ca-certificates python3-pip ccache git \
    && rm -rf /var/lib/apt/lists/*

RUN python3 -m pip install --no-cache-dir "cmake>=3.24,<4" "ninja>=1.11" "aqtinstall>=3.2"

COPY setup.sh /tmp/setup.sh
RUN bash /tmp/setup.sh \
    && mkdir -p /opt/build_tools \
    && cp /root/build_tools/environment.sh /opt/build_tools/environment.sh \
    && chmod -R a+rX /opt/build_tools

RUN aqt install-qt linux desktop 6.10.2 linux_gcc_64 \
        -m qt5compat qtnetworkauth qtshadertools qtwebsockets \
        -O /opt/Qt

ENV PATH=/opt/Qt/6.10.2/gcc_64/bin:${PATH} \
    CMAKE_PREFIX_PATH=/opt/Qt/6.10.2/gcc_64 \
    QT_QPA_PLATFORM=minimal
