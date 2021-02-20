FROM logicp/kserver:1.0.0
# Initial update of package lists
RUN pacman -S --noconfirm libsodium zeromq
RUN git clone https://github.com/stronglogicsolutions/kserver
WORKDIR kserver
RUN git submodule sync
RUN git submodule update --init --recursive
RUN BUILD_TYPE=Debug ./build.sh
