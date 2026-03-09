FROM gcc:latest AS builder

WORKDIR /app

# Build libjwt 3.2.3 from source
RUN apt-get update && apt-get install -y libcurl4-openssl-dev libjansson-dev libssl-dev cmake && \
    rm -rf /var/lib/apt/lists/*

RUN curl -L https://github.com/benmcollins/libjwt/archive/refs/tags/v3.2.3.tar.gz | tar xz && \
    cd libjwt-3.2.3 && \
    cmake -B build -DCMAKE_INSTALL_PREFIX=/usr/local -DBUILD_SHARED_LIBS=ON && \
    cmake --build build && \
    cmake --install build && \
    ldconfig

COPY lib/ lib/
COPY *.c *.h ./

RUN mkdir -p build logs && touch logs/wikigen_auth.log && \
    for f in *.c; do gcc -Wall -Wextra -Wuninitialized -Wno-unused-parameter -Wno-unused-variable -c "$f" -I/usr/local/include -o "build/$(basename "$f" .c).o"; done && \
    gcc build/*.o -o build/wikigen_auth -lcurl -ljwt -ljansson -lssl -lcrypto -L/usr/local/lib -L/app/lib -llibsql

FROM debian:trixie-slim

WORKDIR /app

RUN apt-get update && apt-get install -y libcurl4 libjansson4 libssl3 && \
    rm -rf /var/lib/apt/lists/* && \
    mkdir -p logs && touch logs/wikigen_auth.log

COPY --from=builder /usr/local/lib/libjwt* /usr/local/lib/
COPY --from=builder /app/build/wikigen_auth build/
COPY --from=builder /app/lib/ lib/
COPY .env code.html JWKS.json ./

RUN ldconfig

ENV LD_LIBRARY_PATH=/app/lib:/usr/local/lib

EXPOSE 8080

CMD ["./build/wikigen_auth"]
