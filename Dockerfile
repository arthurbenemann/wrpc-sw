FROM ubuntu:noble

# Install necessary dependencies: curl, tar, build-essential (for make), and gcc
RUN apt-get update && apt-get install -y \
    curl \
    tar \
    build-essential \
    git \
    gcc \
    libreadline-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app    

# Download extract, and export path to riscv-11.2-small.tgz file
RUN curl -L -o riscv-11.2-small.tgz https://ohwr.org/project/wrpc-sw/wikis/uploads/9f9224d2249848ed3e854636de9c08dc/riscv-11.2-small.tgz
RUN tar -xvzf riscv-11.2-small.tgz
ENV CROSS_COMPILE="/app/riscv-11.2-small/bin/riscv32-elf-"

# Copy repository into the container
COPY . /app

# Initialize and update the Git submodules
RUN git submodule update --init --recursive

# Select a default build config and make it
RUN make spec_defconfig
RUN make

# Optionally, run make in the current directory (you can override this when running the container)
CMD [ "bash"]