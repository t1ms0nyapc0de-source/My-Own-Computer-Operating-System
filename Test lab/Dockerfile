FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y \
    build-essential \
    gcc-multilib \
    nasm \
    grub-pc-bin \
    xorriso \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /workspace
