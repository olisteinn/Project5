#!/bin/bash
set -e

cd "$(dirname "$0")"

mkdir -p userspace_app/deb/usr/local/bin userspace_app/deb/usr/src/project5-kernel
cp kernel_module/* userspace_app/deb/usr/src/project5-kernel/
chmod 755 userspace_app/deb/DEBIAN/postinst

zig c++ -target arm-linux-gnueabihf -mcpu=cortex_a53 -fno-sanitize=undefined \
    userspace_app/build/src/main.cpp \
    userspace_app/build/src/drive.cpp \
    userspace_app/build/src/P_controller.cpp \
    userspace_app/build/src/PI_controller.cpp \
    userspace_app/build/src/setpoint_generator.cpp \
    -Iuserspace_app/build/include \
    -Luserspace_app/lib -lgpiod \
    -o userspace_app/deb/usr/local/bin/project5

dpkg-deb --build userspace_app/deb userspace_app/project5.deb
echo "Built userspace_app/project5.deb successfully!"
