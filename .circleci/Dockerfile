FROM logicp/kserver:1.2.0
# Initial update of package lists
# ENV TZ="America/Toronto"
# RUN rm -rf /etc/pacman.d/gnupg
#RUN pacman-key --init
#RUN pacman-key --populate
#RUN pacman-key --refresh-keys
# RUN pacman -Sy archlinux-keyring --noconfirm
# RUN pacman -Syyu --noconfirm
#RUN pacman -Syu --noconfirm
RUN pacman -Sy --noconfirm base-devel gcc cmake git libpqxx postgresql postgresql-libs flatbuffers libsodium zeromq fmt
RUN git clone https://github.com/jtv/libpqxx && cd libpqxx && cmake . && make && make install
