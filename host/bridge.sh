#!/bin/sh

#sudo apt-get install bridge-utils
#sudo apt-get install uml-utilities
#choose a network interface to connect with virtual bridge
if [ $# -ne 1 ]; then
	echo "input a network interface"
	exit 0
fi
sudo ifconfig "$1" down        # 关闭网卡
sudo brctl addbr br0     # 添加网桥br0
sudo brctl addif br0 "$1"    # 将$1网口的配置复制一份到br0
sudo brctl stp br0 off     # 关闭网桥
sudo brctl setfd br0 1     # 设置网桥的fd（标识符）
sudo brctl sethello br0 1    # 设置hello协议的时延
sudo ifconfig br0 0.0.0.0 promisc up # 开启网桥br0
sudo ifconfig "$1" 0.0.0.0 promisc up # 开启$1网卡
sudo dhclient br0      # 向路由器获取IP地址
sudo brctl show br0      # 打印网桥信息
sudo brctl showstp br0     # 打印网桥stp信息
sudo tunctl -t tap0 -u root    # 新建一个tap0网卡
sudo brctl addif br0 tap0    # 将tap0网卡加入到br0上
sudo ifconfig tap0 0.0.0.0 promisc up # 开启tap0网卡
brctl showstp br0      # 打印网桥stp信息
# sudo systemctl restart zerotier-one.service # 重启zerotier

