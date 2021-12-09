depth=128
output_file_prex=bdev_
result_file_prex=bdev_result_
SPDK_dev=Nvme0n1 #TEST device
bssize=4096
optype=read
option=1
test_list="4k 16k 64k 256k 1m 2m"
SPDK_DIR=$HOME/spdk

function full_write(){
    count=2048
    dev=/dev/nvme0n1
    echo "write device $dev, per block size = 2M, count=$count"
    sudo dd if=/dev/urandom of=$dev bs=2M count=$count
}

function process_result(){
    # judge whether file exist
    if [[ $# -eq 1 && -a $1 ]]; then

        # lat (usec): min=404,
        # READ: bw=263MiB/s (276MB/s),
        echo "process result..."
        now=`date +%m%d_%H_%M_%S`
        latency=`sed -n 's/\s\+lat\s\+([um]sec):\s\+min=\([0-9]\+\),.*/\1/gp' $1`
        bw=`sed -n 's/.*READ:\s\+bw=[0-9]\+MiB\/s\s\+(\([0-9]\+\)MB\/s).*/\1/gp' $1`
        result_file=$result_file_prex$optype".txt"
        echo "$now $option $bssize $bw $latency" >> $result_file
        echo "the result has store in $result_file"
    else
        echo "invalid parameter or file is not existed."
    fi
}

function test_libaio(){
    echo -e "Test parameters:\n"
    echo -e "IO depth:$depth\nIO size:$1"
    echo -e "IO engine:aio\nIO type: $optype"
    output_file=$output_file_prex$bssize"_libaio.txt"
    size=2G
    sudo fio -rw=$optype -ioengine=libaio -direct=1 -thread -numjobs=1 -bs=$1 -iodepth=$depth -filename=$dev -size=$size -name=job1 > $output_file
    process_result $output_file
}


#测试前要判断有没有SPDK_dev设备
function test_spdk_bdev(){
    #ioengine=spdk
    echo -e "Test parameters:\n"
    echo -e "IO depth:$depth\nIO size:$1"
    echo -e "IO engine:spdk\nIO type: $optype"
    output_file=$output_file_prex$bssize"_bdev.txt"
    size=4G
    sudo LD_PRELOAD=$SPDK_DIR/build/fio/spdk_bdev fio  -rw=$optype -ioengine=spdk_bdev -norandommap=1 -direct=1 -thread -numjobs=1 -bs=$1 -iodepth=$depth -filename=$SPDK_dev -size=$size -name=test -spdk_json_conf=$SPDK_DIR/examples/bdev/fio_plugin/local_bdev.json > $output_file
    process_result $output_file
}


function help(){
    echo "$0 [bdev=2 or ext4=1] [io size]"
    echo "$0 write"
}

if [ $# -eq 0 ]; then
    help
elif [ $# -eq 1 ]; then

    if [[ $1 == "write" ]]; then
        full_write
    else
        echo "error parameter"
        help
    fi
fi

if [ $# -lt 2 ]; then
    exit
fi

option=$1
bssize=$2

str=${test_list//|/ } #将读入的line以空格拆分，保存到数组
	#echo str 长度: ${#str[@]}
arr=($str)
echo arr 长度：${#arr[@]}
for each in ${arr[*]}
do
bssize=$each
if [ $1 -eq 1 ]; then
    test_libaio $bssize
elif [ $1 -eq 2 ]; then
    test_spdk_bdev $bssize
else
    echo "invalid option!!"
    exit
fi
done

