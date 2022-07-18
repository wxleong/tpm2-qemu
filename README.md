# Introduction

Access hardware TPM from QEMU.

# Table of Contents

- **[Prerequisites](#prerequisites)**
- **[Hardware Setup](#hardware-setup)**
- **[Software Setup](#software-setup)**
- **[Launch QEMU](#launch-qemu)**

# Prerequisites

- Host machine with Ubuntu 20.04.2 LTS installed
- Iridium 9670 TPM 2.0 board and USB-FTDI-SPI converter [[1]](#1)

# Hardware Setup

Complete the setup [[1]](#1)

# Software Setup

Install dependencies:
```
$ sudo apt update
$ sudo apt-get install qemu-system ovmf libguestfs-tools libfuse-dev
```

Download Debian installer ISO:
```
$ curl https://cdimage.debian.org/cdimage/release/current/amd64/iso-cd/debian-11.3.0-amd64-netinst.iso --output ~/debian-11.3.0-amd64-netinst.iso
```

Create a Debian base image by completing the Debian OS installation through a remote desktop viewer (you may use Vinagre from Ubuntu Software):
```
$ ~/qemu/build/qemu-img create -f qcow2 ~/debian-x86_64.qcow 10G
$ ~/qemu/build/qemu-system-x86_64 \
  -m 4096 \
  -hda ~/debian-x86_64.qcow \
  -cdrom ~/Downloads/debian-11.3.0-amd64-netinst.iso \
  -boot d \
  -smp $(nproc)
```

Extract the kernel and initrd from the base image:
```
$ sudo chmod 644 /boot/vmlinuz*
$ virt-ls -a ~/debian-x86_64.qcow /boot/
$ virt-copy-out -a ~/debian-x86_64.qcow /boot/vmlinuz-5.10.0-15-amd64 /boot/initrd.img-5.10.0-15-amd64 ~/
```

Start a character device in user space:
```
$ git clone https://github.com/wxleong/tpm2-qemu ~/tpm2-qemu
$ cd ~/tpm2-qemu/cuse
$ gcc -Wall -Wextra cuse.c `pkg-config --cflags --libs fuse` -o cuse
```

# Launch QEMU

Plug in the USB-FTDI-SPI-TPM and start the tpm2-server:
```
$ cd ~/build/tpm2_server
$ sudo ./ntpm -d -f 10000000
```

Start a `/dev/usbtpm0` character device in user space:
```
$ sudo ~/tpm2-qemu/cuse/cuse -f --name=usbtpm0
$ sudo chmod a+rw /dev/usbtpm0
```

Start the QEMU:
```
$ touch /tmp/usbtpm0-cancel
$ qemu-system-x86_64 \
  -m 4096 \
  -bios /usr/share/ovmf/OVMF.fd \
  -kernel ~/Downloads/vmlinuz-5.10.0-15-amd64 \
  -append "root=/dev/vda1 console=ttyS0" \
  -initrd ~/Downloads/initrd.img-5.10.0-15-amd64 \
  -drive if=virtio,file=debian-x86_64.qcow,format=qcow2,id=hd \
  -smp $(nproc) \
  -net "user,hostfwd=tcp::5555-:22" \
  -net nic \
  -nographic \
  -monitor none \
  -serial stdio \
  -tpmdev passthrough,id=usbtpm0,path=/dev/usbtpm0,cancel-path=/tmp/usbtpm0-cancel \
  -device tpm-tis,tpmdev=usbtpm0
```

SSH to RPi OS from host:
```
$ ssh -p 5555 pi@localhost
```

# References

<a id="1">[1] https://github.com/wxleong/tpm2-usb</a> <br>
<a id="2">[2] https://github.com/qemu/qemu.git</a> <br>

# License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.
