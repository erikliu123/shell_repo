#!/bin/bash
# Huaicheng Li <huaicheng@cs.uchicago.edu>
# Run FEMU as a black-box SSD (FTL managed by the device)

# image directory
IMGDIR=$HOME/liujiahong/linux/arch/x86_64/boot
# Virtual machine disk image
KERNEL_IMG=$IMGDIR/bzImage

if [[ ! -e "$KERNEL_IMG" ]]; then
	echo ""
	echo "VM disk image couldn't be found ..."
	echo "Please prepare a usable VM image and place it as $KERNEL_IMG"
	echo "Once VM disk image is ready, please rerun this script again"
	echo ""
	exit
fi

sudo ~/liujiahong/simulator/FEMU/build-femu/x86_64-softmmu/qemu-system-x86_64 \
    -name "FEMU-BBSSD-VM" \
    -enable-kvm \
    -cpu host \
    -smp 4 \
    -m 4G \
    -kernel $KERNEL_IMG \
    -hda ~/liujiahong/myinitrd4M.img \
    -device virtio-scsi-pci,id=scsi0 -drive file=/dev/sda,if=none,format=raw,discard=unmap,aio=native,cache=none,id=someid -device scsi-hd,drive=someid,bus=scsi0.0 \
    -nographic   \
    -append "console=ttyS0 init=/init root=/dev/sda rw" \
    -device femu,devsz_mb=4096,femu_mode=1 \
    -net nic -net tap,ifname=tap3,script=no,downscript=no \
    -nographic \
    -qmp unix:./qmp-sock,server,nowait 2>&1 | tee log
