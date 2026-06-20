FROM ubuntu:24.04


# https://developer.arm.com/-/media/Files/downloads/gnu/15.2.rel1/binrel/arm-gnu-toolchain-15.2.rel1-x86_64-arm-none-eabi.tar.xz

# Select the Arm GNU Toolchain version.
ARG ARM_TOOLCHAIN_VERSION=15.2.rel1
# Define the Arm GNU Toolchain archive filename.
ARG ARM_TOOLCHAIN_FILE=arm-gnu-toolchain-${ARM_TOOLCHAIN_VERSION}-x86_64-arm-none-eabi.tar.xz
# Define the Arm GNU Toolchain download URL.
ARG ARM_TOOLCHAIN_URL=https://developer.arm.com/-/media/Files/downloads/gnu/${ARM_TOOLCHAIN_VERSION}/binrel/${ARM_TOOLCHAIN_FILE}

# Avoid interactive prompts during package installation
ENV DEBIAN_FRONTEND=noninteractive
# Stable location for the ARM GNU toolchain
ENV ARM_GNU_TOOLCHAIN=/opt/arm-gnu-toolchain
# Ensure the ARM GNU toolchain is in the PATH
ENV PATH="${ARM_GNU_TOOLCHAIN}/bin:${PATH}"

#-----------------------------------------------------------------------------
# Install dependencies
#-----------------------------------------------------------------------------
RUN apt-get update && apt-get install -y \
    ca-certificates \
    wget \
    curl \
    gpg \
    xz-utils \
    tar \
    git \
    ninja-build \
    build-essential \
    clang \
    clangd \
    clang-tidy \
    gdb-multiarch \
    openocd \
    python3 \
    python3-pip \
    && rm -rf /var/lib/apt/lists/*

# -----------------------------------------------------------------------------
# Kitware repository (latest CMake)
# -----------------------------------------------------------------------------

# Download and add the Kitware GPG key, then add the Kitware repository to the sources list
RUN wget -O - https://apt.kitware.com/keys/kitware-archive-latest.asc \
    | gpg --dearmor \
    > /usr/share/keyrings/kitware-archive-keyring.gpg


# Add Kitware repository.
RUN echo "deb [signed-by=/usr/share/keyrings/kitware-archive-keyring.gpg] https://apt.kitware.com/ubuntu/ noble main" \
    > /etc/apt/sources.list.d/kitware.list

# Update package lists and install CMake from the Kitware repository
RUN apt-get update && apt-get install -y cmake

#-----------------------------------------------------------------------------
# Install the ARM GNU toolchain
#-----------------------------------------------------------------------------

# Download and extract the ARM GNU toolchain
RUN wget -O /tmp/${ARM_TOOLCHAIN_FILE} ${ARM_TOOLCHAIN_URL} 

# Extract the toolchain to the specified location
RUN tar -xf /tmp/${ARM_TOOLCHAIN_FILE} -C /opt/ 

# Create the symbolic link to the extracted toolchain
RUN ln -s /opt/arm-gnu-toolchain-${ARM_TOOLCHAIN_VERSION}-x86_64-arm-none-eabi ${ARM_GNU_TOOLCHAIN}

# Remove temporary downloaded file
RUN rm /tmp/${ARM_TOOLCHAIN_FILE}

##-----------------------------------------------------------------------------
## Verify installation
##-----------------------------------------------------------------------------
RUN arm-none-eabi-gcc --version
RUN cmake --version
RUN ninja --version

WORKDIR /workspace

CMD ["bash"]

