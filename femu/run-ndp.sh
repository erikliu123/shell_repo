#!/bin/bash
# Huaicheng Li <huaicheng@cs.uchicago.edu>
# Run FEMU as a black-box SSD (FTL managed by the device)

# image directory
IMGDIR=$HOME/images
# Virtual machine disk image
OSIMGF=$IMGDIR/ljh-u20s.qcow2

if [[ ! -e "$OSIMGF" ]]; then
	echo ""
	echo "VM disk image couldn't be found ..."
	echo "Please prepare a usable VM image and place it as $OSIMGF"
	echo "Once VM disk image is ready, please rerun this script again"
	echo ""
	exit
fi

sudo x86_64-softmmu/qemu-system-x86_64 \
    -name "FEMU-BBSSD-VM" \
    -enable-kvm \
    -cpu host \
    -smp 8 \
    -m 16G \
    -device virtio-scsi-pci,id=scsi0 \
    -device scsi-hd,drive=hd0 \
    -drive file=$OSIMGF,if=none,aio=native,cache=none,format=qcow2,id=hd0 \
    -device femu,devsz_mb=4096,femu_mode=1 \
    -device femu,devsz_mb=4096,femu_mode=1 \
    -net tap,ifname=tap0,script=no,downscript=no \
    -net nic,macaddr=`dd if=/dev/urandom count=1 2>/dev/null | md5sum | sed 's/^\(.\)\(..\)\(..\)\(..\)\(..\)\(..\).*$/\14:\2:\3:\4:\5:\6/g'` \
    -nographic \
    -device vfio-pci,host=17:00.0,addr=10.0,multifunction=on \
    -device vfio-pci,host=17:00.1,addr=11.0,multifunction=on \
    -device vhost-vsock-pci,guest-cid=10 \
    -qmp unix:./qmp-sock,server,nowait 2>&1 | tee log

exit

# 旧选项
    -net user,hostfwd=tcp::8080-:22 \
    -net nic,model=virtio \
-device vfio-pci,host=17:00.0,addr=09.0,multifunction=on \
-device vfio-pci,host=17:00.1,addr=10.0,multifunction=on \

