FROM ubuntu:20.04

LABEL org.opencontainers.version="v1.0.0"
LABEL org.opencontainers.image.authors="Sirapop Theeranantachai <stheera@g.ucla.edu>"
LABEL org.opencontainers.image.url="https://github.com/markverick/ns3-ospf"
LABEL org.opencontainers.image.source="https://github.com/markverick/ns3-ospf"
LABEL org.opencontainers.image.vendor="Sirapop Theeranantachai"
LABEL org.opencontainers.image.licenses="GPL-2.0"
LABEL org.opencontainers.image.title="ns-3 docker container"
LABEL org.opencontainers.image.description="ns-3 docker container for github action pipelines"

WORKDIR /usr/local/app

RUN apt-get update && apt-get install -y --no-install-recommends \
    apt-transport-https \
    ca-certificates \
    g++ \
    git \
    python3 \
    && rm -rf /var/lib/apt/lists/*

RUN update-ca-certificates && \
    git clone https://github.com/nsnam/ns-3-dev-git.git ns-3 && \
    cd ns-3 && \
    git checkout ns-3.35

VOLUME ["./ns-3/contrib"]

# COPY ospf ./ns-3/contrib/ospf

RUN cd ns-3 && \
    ./waf configure --enable-examples --enable-tests && \
    ./waf build

COPY ./entrypoint.sh ./entrypoint.sh
ENTRYPOINT ["./entrypoint.sh"]
