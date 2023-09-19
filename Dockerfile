FROM ubuntu:bionic

RUN apt-get update && \
    apt-get install -y \
        git \
        cmake \
        make \
        gcc \
        g++ \
        libz-dev \
        xz-utils

ADD https://github.com/libretro/libretro-toolchains/raw/master/wiiu.tar.xz /opt/wiiu.tar.xz
RUN tar xf /opt/wiiu.tar.xz -C /opt; rm /opt/wiiu.tar.xz

ENV DEVKITPRO=/opt/devkitpro
ENV DEVKITPPC=/opt/devkitpro/devkitPPC

WORKDIR project