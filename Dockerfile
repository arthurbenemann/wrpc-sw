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

WORKDIR /riscv

# Download extract, and export path to riscv-11.2-small.tgz file
RUN curl -L -o riscv-11.2-small.tgz https://ohwr.org/project/wrpc-sw/wikis/uploads/9f9224d2249848ed3e854636de9c08dc/riscv-11.2-small.tgz
RUN tar -xvzf riscv-11.2-small.tgz && rm riscv-11.2-small.tgz
ENV CROSS_COMPILE="/riscv/riscv-11.2-small/bin/riscv32-elf-"

# change to /wr folder, and avoid the checks that fail because of the new flags added to git because of cve-2022-24765
WORKDIR /wr
RUN git config --global --add safe.directory /wr

# Optionally, run make in the current directory (you can override this when running the container)
CMD [ "bash"]

# To make the codebase with a pre-built image use: docker run -it --rm -v ./:/wr arthurbdocker/wrpc-sw-build make -j`nproc`
