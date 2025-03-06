FROM ubuntu:24.04

RUN apt-get update \
    && \
    apt-get install -y \
        vim git make g++ cmake libtool python3 \
        libboost-all-dev libssl-dev libgmp-dev vim

WORKDIR /home/

# --------------------- clone, build relic ----------------------
RUN git clone https://github.com/relic-toolkit/relic 
WORKDIR /home/relic
RUN git checkout 0.7.0

RUN mkdir build && cd build && cmake .. && make -j && make install

# # --------------------- clone, build cryptotools ----------------------
WORKDIR /home/
RUN git clone --recurse-submodules https://github.com/ladnir/cryptoTools/ 
WORKDIR /home/cryptoTools
#RUN git checkout b2cdd30
# build according to instructions on github
RUN python3 build.py --setup --boost --relic
# RUN python3 build.py -D ENABLE_RELIC=ON
RUN python3 build.py --install
 
# # # --------------------- clone, build coproto ----------------------
WORKDIR /home/
RUN git clone https://github.com/Visa-Research/coproto
 
WORKDIR /home/coproto
RUN python3 build.py
RUN python3 build.py --install
# # --------------------- clone, build libOTe ----------------------
# 
WORKDIR /home/
RUN git clone https://github.com/osu-crypto/libOTe  
WORKDIR /home/libOTe
#RUN git checkout v2.2.0 

RUN python3 build.py --boost -DENABLE_SIMPLESTOT_ASM=ON -DENABLE_MR_KYBER=ON \ 
-DENABLE_IKNP=ON -DENABLE_SOFTSPOKEN_OT=ON -DENABLE_SILENTOT=ON -DENABLE_PIC=ON -DLIBOTE_STD_VER=20 --relic  
# # RUN python3 build.py --all --boost --sodium
RUN python3 build.py --install
# create KyberOT directory if it does not exist
RUN if [ ! -d /usr/local/include/KyberOT/ ]; then mkdir -p /usr/local/include/KyberOT/; fi
# copy header files 
RUN cp thirdparty/KyberOT/KyberOT.h /usr/local/include/KyberOT/&& cp thirdparty/KyberOT/params.h /usr/local/include/KyberOT/
 
# # --------------------- clone, build leap ----------------------
WORKDIR /pwd
RUN mkdir build 
WORKDIR /pwd/build
SHELL ["/bin/bash", "-c"]
RUN cmake .. && make
