PORT=4120
IP=192.168.123.83  #`ifconfig ens3 | grep inet\ | awk '{ print $2 }'`
subsytem=nqn.2016-06.io.spdk:cnode2
remote_dir=/mnt/nfs
mountdir=/mnt/nfs

sudo modprobe nvme_tcp
HOSTNQN=`sudo nvme gen-hostnqn`
echo "IP: $IP, need change after VM restarted!"
sudo nvme connect -t tcp -a $IP -s $PORT -n  "$subsytem" --hostnqn=$HOSTNQN

echo "list nvme devices..."
sudo nvme list
if [ $# -eq 0 ]; then
    sudo mount -t nfs $IP:$remote_dir $mountdir
else
    echo "skip nfs mount period"
fi