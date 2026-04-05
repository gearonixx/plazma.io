FROM ghcr.io/userver-framework/ubuntu-22.04-userver-base:latest


RUN apt-get update && apt-get install -y sudo curl gnupg lsb-release libclang-dev clangd

RUN curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh -s -- -y
ENV PATH="/root/.cargo/bin:${PATH}"
RUN rustup default stable

RUN curl -fsSL https://claude.ai/install.sh | bash
ENV PATH="/root/.claude/bin:${PATH}"

RUN apt-get update && apt-get install -y libuv1-dev

RUN git clone https://github.com/scylladb/cpp-rs-driver.git /opt/cpp-rs-driver \
  && cd /opt/cpp-rs-driver \
  && mkdir build && cd build \
  && cmake -DCASS_BUILD_STATIC=OFF -DCASS_BUILD_INTEGRATION_TESTS=OFF -DCMAKE_BUILD_TYPE=Release .. \
  && make -j$(nproc) \
  && make install \
  && ldconfig \
  && cd / && rm -rf /opt/cpp-rs-driver

RUN git clone https://github.com/gearonixx/userver.git /opt/userver \
  && cd /opt/userver \
  && git checkout scylladb-driver \
  && BUILD_OPTIONS=" \
    -DUSERVER_BUILD_ALL_COMPONENTS=1 \
    -DUSERVER_FEATURE_YDB=0 \
    -DUSERVER_FEATURE_GRPC_CHANNELZ=0 \
    -DUSERVER_FEATURE_MONGODB=ON \
    -DUSERVER_FEATURE_MYSQL=OFF \
    -DUSERVER_FEATURE_POSTGRESQL=OFF \
    -DUSERVER_FEATURE_SCYLLADB=ON \
    -DUSERVER_FEATURE_STACKTRACE=0 \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
    -DUSERVER_FEATURE_EASY=OFF \
    -DUSERVER_FEATURE_GRPC=OFF \
    -DUSERVER_FEATURE_OTLP=OFF \
    -DUSERVER_FEATURE_GRPC_REFLECTION=OFF" \
  ./scripts/build_and_install_all.sh \
  && cd / && rm -rf /opt/userver

EXPOSE 8080-8100