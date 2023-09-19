# Dragonfly [![doi](https://badgen.net/badge/DOI/10.1145%2F3603269.3604876/green)](https://doi.org/10.1145/3603269.3604876)

<p align="center">
<img src="_logo/Dfly_Logo_v1.svg" width="100%"> 
</p>



This accompanies the paper *"Dragonfly: Higher Perceptual Quality For Continuous 360° Video Playback"*. Ehab Ghabashneh, Chandan Bothra, Ramesh Govindan, Antonio Ortega, and Sanjay Rao. In Proceedings of the ACM Special Interest Group on Data Communication, SIGCOMM ’23, New York, NY, USA. If you use this artifact, please cite: 

```
@inproceedings{Ghabashneh_Dragonfly_2023,
  author    = {Ghabashneh, Ehab and Bothra, Chandan and Govindan, Ramesh and Ortega, Antonio and Rao, Sanjay},
  title     = {Dragonfly: Higher Perceptual Quality For Continuous 360° Video Playback},
  year      = {2023},
  url       = {https://doi.org/10.1145/3603269.3604876},
  doi       = {10.1145/3603269.3604876},
  booktitle = {Proceedings of the ACM Special Interest Group on Data Communication},
  series    = {SIGCOMM '23}
}

```
## Prerequisite

- Ubuntu 16.04 or Ubunutu 18.04 machine
- gcc-9 
- g++-9
- cmake
- ffmpeg-4
- libgflags-dev 
- libgoogle-glog-dev 
- libboost-all-dev 
- libavcodec-dev
- libavformat-dev 
- libswscale-dev 
- libdouble-conversion-dev 
- libfmt-dev 
- libevent-dev 
- libssl-dev 
- mahimahi
- fmt
- folly

### Install Prerequisites
Please refer to our bash.sh script, or follow the commands below:
```
apt-get install gcc-9 g++-9 -y  && update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-9 60 --slave /usr/bin/g++ g++ /usr/bin/g++-9 
add-apt-repository ppa:jonathonf/ffmpeg-4
apt-get install -y ffmpeg  libgflags-dev libgoogle-glog-dev libboost-all-dev libavcodec-dev libavformat-dev libswscale-dev libdouble-conversion-dev libfmt-dev libevent-dev libssl-dev cmake mahimahi
```

#### fmt & folly libraries, please use the versions in the third-party-lib dir
Install fmt --> `cd ~/Dragonfly/third-party-lib/fmt && mkdir build && cd build && cmake .. && make -j2 && make install`

Install folly --> `cd ~/Dragonfly/third-party-lib/folly-2021.03.15.00 && mkdir _build && cd _build && cmake .. && make -j2 && make install`

### Build Dragonfly
Dragonfly source code resides in the system directory. To build Dragonfly,
```
cd system && mkdir build
make -f Makefile_ubuntu
```


