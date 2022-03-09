#https://qiedd.com/669.html
id="0000:17:00.0"
vendor=$(cat /sys/bus/pci/devices/$id/vendor)
device=$(cat /sys/bus/pci/devices/$id/device)
echo $id > /sys/bus/pci/devices/$id/driver/unbind
#echo 1 > /sys/bus/pci/devices/$id/remove  #从nvidia驱动先卸载下来
#echo 1 > /sys/bus/pci/rescan   #重新扫描pci设备
echo $vendor $device > /sys/bus/pci/drivers/vfio-pci/new_id


# qemu-system-x86_64: -device vfio-pci,host=17:00.0,addr=09.0,multifunction=on: 
#vfio 0000:17:00.0: failed to open /dev/vfio/40: No such file or directory

#exit
# GPU's audio device
id="0000:17:00.1"
vendor=$(cat /sys/bus/pci/devices/$id/vendor)
device=$(cat /sys/bus/pci/devices/$id/device)
# unbind snd_hda_intel first
echo $id > /sys/bus/pci/devices/$id/driver/unbind
echo $vendor $device > /sys/bus/pci/drivers/vfio-pci/new_id
