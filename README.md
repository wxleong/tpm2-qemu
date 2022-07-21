# Introduction

Access hardware TPM from QEMU.

# Table of Contents

- **[Prerequisites](#prerequisites)**
- **[Software Setup](#software-setup)**
- **[Launch QEMU](#launch-qemu)**

# Prerequisites

- Host machine with Ubuntu 20.04.2 LTS installed
- LetsTrust-TPM2Go [[1]](#1)

# Software Setup

Install dependencies:
```
$ sudo apt update
$ sudo apt-get install qemu-system ovmf libguestfs-tools libfuse-dev

# tpm2-tss dependencies
$ sudo apt -y install \
  autoconf-archive \
  libcmocka0 \
  libcmocka-dev \
  procps \
  iproute2 \
  build-essential \
  git \
  pkg-config \
  gcc \
  libtool \
  automake \
  libssl-dev \
  uthash-dev \
  autoconf \
  doxygen \
  libjson-c-dev \
  libini-config-dev \
  libcurl4-openssl-dev

# tpm2-tools dependencies
$ sudo apt -y install \
  libcurl4-gnutls-dev \
  python-yaml \
  uuid-dev \
  pandoc
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

Install a modified version of tpm2-tss:
```
$ git clone --branch letstrust2go https://github.com/tobyp/tpm2-tss ~/tpm2-tss
$ cd ~/tpm2-tss
$ git checkout 9b95f9676b41e86a91a869ce3524f7b8fb7710b0

# Apply patches
$ git clone --branch develop-letstrust2go https://github.com/wxleong/tpm2-qemu ~/tpm2-qemu
$ cd ~/tpm2-tss
$ wget https://github.com/PeterHuewe/tpm2-tss/commit/bbbe7ab6f184be1e078ef501b8c14e3fd9d4620c.patch -O workaround-waitstates.patch
$ git am < workaround-waitstates.patch
$ git am < ~/tpm2-qemu/patch/0001-fix-tcti-spi-usb-to-stabilize-the-usb-connection.patch

# Build tpm2-tss
$ ./bootstrap
$ ./configure
$ make -j$(nproc)
$ sudo make install
$ sudo ldconfig
```

Install tpm2-tools:
```
$ git clone https://github.com/tpm2-software/tpm2-tools ~/tpm2-tools
$ cd ~/tpm2-tools
$ git checkout 5.2
$ ./bootstrap
$ ./configure
$ make -j$(nproc)
$ sudo make install
$ sudo ldconfig
```

Set udev rules for LetsTrust-TPM2Go by creating a file `/etc/udev/rules.d/60-tpm2go.rules`:
```
ATTRS{idVendor}=="04b4", ATTRS{idProduct}=="0004", TAG+="uaccess"
```

Activate the udev rules:
```
$ sudo udevadm control --reload
```

Plug in the LetsTrust-TPM2Go and you should see:
```
$ dmesg
[ 1019.115823] usb 3-2: new full-speed USB device number 5 using xhci_hcd
[ 1019.480333] usb 3-2: New USB device found, idVendor=04b4, idProduct=0004, bcdDevice= 0.00
[ 1019.480360] usb 3-2: New USB device strings: Mfr=1, Product=2, SerialNumber=3
[ 1019.480382] usb 3-2: Product: LetsTrust-TPM2Go
[ 1019.480405] usb 3-2: Manufacturer: www.pi3g.com
[ 1019.480426] usb 3-2: SerialNumber: 000010

$ sudo udevadm info -e | grep LetsTrust
E: ID_MODEL=LetsTrust-TPM2Go
E: ID_MODEL_ENC=LetsTrust-TPM2Go
E: ID_SERIAL=www.pi3g.com_LetsTrust-TPM2Go_000010
```

Use tcti-spi-usb to communicate with LetsTrust-TPM2Go:
```
$ tpm2_startup -Tspi-usb -c
$ tpm2_getrandom -Tspi-usb 8 --hex
```

Expose the TPM on a socket to exchange raw command:
```
$ socat TCP-LISTEN:9883,reuseaddr,fork EXEC:"tpm2_send -Tspi-usb"
```

Test the socket by sending TPM2_Startup(TPM_SU_CLEAR):
```
$ echo -e '\x80\x01\x00\x00\x00\x0C\x00\x00\x01\x44\x00\x00' | nc localhost 9883 -q 1 | xxd -p
80010000000a00000100
```

Build a midware to forward the socket to a character device:
```
$ cd ~/tpm2-qemu/cuse
$ gcc -Wall -Wextra cuse.c `pkg-config --cflags --libs fuse` -o cuse
```

# Launch QEMU

Expose the TPM on a socket if you not already done so:
```
$ socat TCP-LISTEN:9883,reuseaddr,fork EXEC:"tpm2_send -Tspi-usb"
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

<a id="1">[1] https://www.letstrust.de/archives/36-Chip-crisis-Update-to-More-Hardware-LetsTrust-TPM2Go.html</a> <br>
<a id="2">[2] https://github.com/qemu/qemu.git</a> <br>
<a id="3">[3] https://gist.github.com/tobyp/aed5598188088f4abbeb737b408e5287</a> <br>
<a id="4">[4] https://github.com/PeterHuewe/tpm2-tss/commit/bbbe7ab6f184be1e078ef501b8c14e3fd9d4620c</a> <br>

# License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.