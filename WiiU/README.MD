
### Build Instructions

Please just use Docker.

If you need help cloning, this is reasonable if you're only looking to make a one-off build and don't need the full history:
```
git clone --depth=1 --recurse-submodules --shallow-submodules https://github.com/vaguerant/ppsspp
```
Then you can build with Docker:
```
docker build . -t ppsspp-builder
docker run -it --rm -v ${PWD}:/project ppsspp-builder sh -c "make -C ext/wiiu/elf2rpl; mkdir build; cd build; cmake .. -DCMAKE_TOOLCHAIN_FILE=../cmake/Toolchains/wiiu.cmake; touch ../CMakeLists.txt; make -j4"
```

if you need console output over network, set the PC_DEVELOPMENT_IP_ADDRESS define in ext/wiiu/logger.c and you can use the ext/wiiu/net_listen.sh script to listen to the log.

the run.sh script be uused to both send the compiled rpx to the HBL and listen to console output.