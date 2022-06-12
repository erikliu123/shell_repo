
#modified:   lib/nvmf/ctrlr.c
#modified:   lib/nvmf/ctrlr_bdev.c
#modified:   lib/nvmf/nvmf_internal.h
#       lib/nvmf/ndp.h

HOST_MONITOR_DIR=$HOME/liujiahong/spdk
TARGET_DIR=/home/femu/spdk
TARGET_SSH=femu\@localhost
PORT=8080

#file_list="lib/nvmf/ctrlr.c lib/nvmf/ctrlr_bdev.c lib/nvmf/nvmf_internal.h lib/nvmf/ndp.h include/spdk/nvme_spec.h"
file_list="lib/nvmf/ctrlr_bdev.c"

str=${file_list//|/ } #将读入的line以空格拆分，保存到数组
arr=($str)
echo arr 长度：${#arr[@]}
for each in ${arr[*]}
do
    scp -P $PORT $HOST_MONITOR_DIR/$each $TARGET_SSH:$TARGET_DIR/$each
done
