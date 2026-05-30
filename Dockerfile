# odb2kicad build and test environment
FROM ubuntu:22.04

# Avoid interactive prompts
ENV DEBIAN_FRONTEND=noninteractive

# Install build tools and Python for testing
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    python3 \
    python3-pip \
    git \
    && rm -rf /var/lib/apt/lists/*

# Set working directory
WORKDIR /app

# Copy source code (exclude build directories)
COPY odb2kicad/src/ /app/odb2kicad/src/
COPY odb2kicad/CMakeLists.txt /app/odb2kicad/
COPY odb2kicad/test/ /app/odb2kicad/test/
COPY samples/ /app/samples/
COPY test/ /app/test/

# Build native CLI (fresh build directory)
RUN cmake -S odb2kicad -B odb2kicad/build -DCMAKE_BUILD_TYPE=Release \
    && cmake --build odb2kicad/build -j$(nproc)

# Make the binary accessible
ENV PATH="/app/odb2kicad/build:${PATH}"

# Default command: run tests
CMD ["bash", "-c", "odb2kicad --help && echo '--- Build successful ---'"]
