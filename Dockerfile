FROM ubuntu:20.04
WORKDIR /usr/local/app

RUN git clone https://gitlab.com/nsnam/ns-3-dev.git ns-3 && \
    cd ns-3 && \
    git checkout ns-3.35

COPY ospf ./contrib/ospf

RUN apt-get update && apt-get install -y --no-install-recommends \
    g++ \
    git \
    python3 \
    && rm -rf /var/lib/apt/lists/*

RUN cd ns-3 && \
    ./waf configure --enable-examples --enable-tests
    ./waf build


ENTRYPOINT ["/entrypoint.sh"]
