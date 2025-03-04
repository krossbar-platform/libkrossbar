FROM ubuntu:24.10

# Configure apt and install packages
RUN apt-get update
RUN apt-get -y upgrade
RUN apt-get -y install \
    build-essential \
    cmake \
    ninja-build \
    gdb \
    pkg-config \
    make \
    ccache \
    valgrind \
    cppcheck \
    ca-certificates \
    locales \
    ssh \
    liburing-dev \
    libgtest-dev \
    liblog4c-dev

# Cleanup
RUN apt-get autoremove -y
RUN apt-get clean -y

# Set up locales
RUN locale-gen en_US.UTF-8
ENV LANG=en_US.UTF-8
ENV LANGUAGE=en_US:en
ENV LC_ALL=en_US.UTF-8

# Create a non-root user with sudo access
ARG USERNAME=vscode
ARG USER_UID=1001
ARG USER_GID=$USER_UID

RUN groupadd --gid $USER_GID $USERNAME
RUN useradd --uid $USER_UID --gid $USER_GID -m $USERNAME
RUN apt-get install -y sudo
RUN echo $USERNAME ALL=\(root\) NOPASSWD:ALL > /etc/sudoers.d/$USERNAME
RUN chmod 0440 /etc/sudoers.d/$USERNAME

# Set the default user
USER $USERNAME
WORKDIR /home/$USERNAME

CMD ["/bin/bash"]
