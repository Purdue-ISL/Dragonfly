apt-get update  && apt-get install build-essential software-properties-common -y  && add-apt-repository ppa:ubuntu-toolchain-r/test && apt-get update  && apt-get install gcc-9 g++-9 -y  && update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-9 60 --slave /usr/bin/g++ g++ /usr/bin/g++-9
add-apt-repository ppa:jonathonf/ffmpeg-4
apt-get install -y  ffmpeg  libgflags-dev libgoogle-glog-dev libboost-all-dev libavcodec-dev libavformat-dev libswscale-dev libdouble-conversion-dev libfmt-dev libevent-dev libssl-dev cmake  mahimahi
cd ~/Dragonfly/third-party-lib/fmt && mkdir build && cd build && cmake .. && make -j2 && make install
cd ~/Dragonfly/third-party-lib/folly-2021.03.15.00 && mkdir _build && cd _build && cmake .. && make -j2 && make install
cd ~/Dragonfly/system && mkdir build && make -f Makefile_ubuntu

