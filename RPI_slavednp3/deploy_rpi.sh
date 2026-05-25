#!/bin/bash
set -e

RPI_USER=ufu
RPI_IP=192.168.100.163
RPI_DIR=/home/ufu

BIN=build-rpi/slavednp3
LIB=$(find build-rpi -name "libopendnp3.so*" | head -n 1)

echo "==> Build"
cmake --build build-rpi

echo "==> Deploy bin"
scp -p "$BIN" ${RPI_USER}@${RPI_IP}:${RPI_DIR}/

if [ -n "$LIB" ]; then
  echo "==> Deploy lib"
  scp -p "$LIB" ${RPI_USER}@${RPI_IP}:${RPI_DIR}/
fi

echo "==> Run"
ssh ${RPI_USER}@${RPI_IP} "${RPI_DIR}/slavednp3"
