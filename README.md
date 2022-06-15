# Introduction

Access hardware TPM from QEMU.

# Table of Contents

- **[Prerequisites](#prerequisites)**
- **[Hardware Setup](#hardware-setup)**
- **[Software Setup](#software-setup)**
- **[Launch](#launch)**

# Prerequisites

<!--
- Raspberry Pi 4 Model B [[1]](#1)
-->
- Host machine with Ubuntu 20.04.2 LTS installed
- Iridium 9670 TPM 2.0 board [[2]](#2)

# Hardware Setup

# Software Setup

Install QEMU:
```
$ sudo apt update
$ sudo apt-get install qemu-system
```

Optionally, install QEMU from source (modified from [[3]](#3) v7.0.0):
```
$ sudo apt-get install git libglib2.0-dev libfdt-dev libpixman-1-dev zlib1g-dev ninja-build make gcc
$ git clone https://github.com/wxleong/qemu.git ~/qemu
$ cd ~/qemu
$ git checkout develop-usb-tpm
$ mkdir build
$ cd build
$ ../configure
$ make -j$(nproc)
```

Download kernel binary and dtb:
```
$ git clone https://github.com/dhruvvyas90/qemu-rpi-kernel ~/qemu-rpi-kernel
$ cd ~/qemu-rpi-kernel
$ git checkout 1938fb5ac06188c12fc88b266cf6912e3fea6f68
```

Download RPi OS image:
```
$ sudo apt install curl
$ curl https://downloads.raspberrypi.org/raspios_armhf/images/raspios_armhf-2022-04-07/2022-04-04-raspios-bullseye-armhf.img.xz --output ~/2022-04-04-raspios-bullseye-armhf.img.xz
$ cd ~
$ unxz 2022-04-04-raspios-bullseye-armhf.img.xz
```

Restore the default 'pi' user:
```
$ cd ~
$ sudo modprobe loop
$ LOOP=`sudo losetup -f`
$ sudo losetup -P $LOOP 2022-04-04-raspios-bullseye-armhf.img
$ sudo mount ${LOOP}p2 /mnt/

$ sudo su -c "echo 'pi:x:1000:1000:,,,:/home/pi:/bin/bash' >> /mnt/etc/passwd"
$ sudo su -c "echo 'pi:\$y\$j9T\$esBSS1gyTG7u.pAPq6nvJ1\$xj6oJZIPpaaVcT0F1WF7tWdqcrE4sULZwmyWQPtSbt0:19158:0:99999:7:::' >> /mnt/etc/shadow"

# Remove duplicated entries "pi..."
$ sudo vi /mnt/etc/passwd
$ sudo vi /mnt/etc/shadow

$ sudo umount ${LOOP}p2
$ sudo losetup -d ${LOOP}
```

# Launch

Directly via QEMU command:
```
$ ~/qemu/build/qemu-system-arm \
  -M versatilepb \
  -cpu arm1176 \
  -m 256 \
  -drive "file=${HOME}/2022-04-04-raspios-bullseye-armhf.img,if=none,index=0,media=disk,format=raw,id=disk0" \
  -device "virtio-blk-pci,drive=disk0,disable-modern=on,disable-legacy=off" \
  -dtb ~/qemu-rpi-kernel/versatile-pb-bullseye-5.10.63.dtb \
  -kernel ~/qemu-rpi-kernel/kernel-qemu-5.10.63-bullseye \
  -append 'root=/dev/vda2 panic=1' \
  -net "user,hostfwd=tcp::5555-:22" \
  -net nic \
  -no-reboot \
  -nographic
```
<!--
OR:
$ qemu-system-arm \
  -M versatilepb \
  -cpu arm1176 \
  -m 256 \
  -hda ~/2022-04-04-raspios-bullseye-armhf.img \
  -dtb ~/qemu-rpi-kernel/versatile-pb-bullseye-5.10.63.dtb \
  -kernel ~/qemu-rpi-kernel/kernel-qemu-5.10.63-bullseye \
  -append "root=/dev/sda2 rootfstype=ext4 rw" \
  -net "user,hostfwd=tcp::5022-:22" \
  -net nic \
  -no-reboot \
  -nographic
  //-serial stdio \ <-- Ctrl+C will terminate the QEMU
-->

<!--
Via hypervisor agnostic libvirt:
```
$ virt-install \
  --name pi \
  --arch armv6l \
  --machine versatilepb \
  --cpu arm1176 \
  --vcpus 1 \
  --memory 256 \
  --import \
  --disk ~/2019-09-26-raspbian-buster-lite.img,format=raw,bus=virtio \
  --network user,model=virtio \
  --video vga \
  --graphics spice \
  --rng device=/dev/urandom,model=virtio \
  --boot 'dtb=${HOME}/qemu-rpi-kernel/versatile-pb-bullseye-5.10.63.dtb, \
          kernel=${HOME}/qemu-rpi-kernel/kernel-qemu-5.10.63-bullseye, \
          kernel_args=root=/dev/vda2 panic=1' \
  --events on_reboot=destroy
```
-->

Enable SSH server on RPi OS:
```
$ sudo raspi-config
    > 3 Interface Options
      > I2 SSH
        > <Yes>
```

SSH to RPi OS from host:
```
$ ssh -p 5555 pi@localhost
```
<!--
$ ssh -vvv -p 5555 pi@localhost <-- to set verbose
-->

# References

<a id="1">[1] https://github.com/wxleong/tpm2-rpi4/</a> <br>
<a id="2">[2] https://www.infineon.com/cms/en/product/evaluation-boards/iridium9670-tpm2.0-linux/</a> <br>
<a id="3">[3] https://github.com/qemu/qemu.git</a> <br>
<a id="4">[4] https://github.com/dhruvvyas90/qemu-rpi-kernel/</a> <br>

# License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.
