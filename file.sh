

HOST_MONITOR_DIR=$HOME/liujiahong/linux/drivers/nvme/host/
TARGET_DIR=/home/femu/linux/drivers/nvme/host/
TARGET_SSH=femu\@localhost
PORT=8080

file_list="tcp.c"
#file_list="fabrics.h" #"nvme.h tcp.c Makefile1"

str=${file_list//|/ } #将读入的line以空格拆分，保存到数组
arr=($str)
echo arr 长度：${#arr[@]}
for each in ${arr[*]}
do
    scp -P $PORT $HOST_MONITOR_DIR/$each $TARGET_SSH:$TARGET_DIR/$each
done
