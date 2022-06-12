
depth=128
output_file_prex=tmp_
result_file_prex=res_
output_dir=tmp
dev=`sudo nvme list | grep SPDK | awk '{print $1}'` #/dev/nvme1n1 #TEST device
bssize=4096
optype=randwrite
option=1
test_list="4k 16k 64k 256k 1m 2m" #"4k 16k" #

function full_write(){
    total_gb=`sudo fdisk -l /dev/nvme1n1 | grep Disk | awk '{print $3}'`
    count=`expr $total_gb \* 1024 / 2`
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
        bw=`sed -n 's/.*WRITE:\s\+bw=[0-9]\+MiB\/s\s\+(\([0-9]\+\)MB\/s).*/\1/gp' $1` #READ->WRITE
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


function test_psync(){
    echo -e "Test parameters:\n"
    echo -e "IO depth:$depth\nIO size:$1"
    echo -e "IO engine:aio\nIO type: $optype"
    output_file=$output_file_prex$bssize"_psync.txt"
    size=2G
    sudo fio -rw=$optype -ioengine=psync -direct=1 -thread -numjobs=1 -bs=$1 -iodepth=$depth -filename=$dev -size=$size -name=job1 > $output_file
    process_result $output_file
}

function test_spdk(){
    #ioengine=spdk
    echo -e "Test parameters:\n"
    echo -e "IO depth:$depth\nIO size:$1"
    echo -e "IO engine:spdk\nIO type: $optype"
    
    output_file=$output_file_prex$bssize"_spdk.txt"
    size=2G
    sudo LD_PRELOAD=/home/femu/spdk/build/fio/spdk_nvme fio -ioengine=spdk -norandommap=1 -direct=1 -thread -numjobs=1 -bs=$1 -iodepth=$depth -size=$size  -name=job1 '--filename=trtype=PCIe traddr=0000.00.05.0 ns=1' > $output_file
    process_result $output_file
}


function help(){
    echo "$0 [libaio=1 or spdk=2 or psync=3] [io size]"
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
if [ ! -d $output_dir ]; then
    mkdir $output_dir
fi
cd $output_dir
for each in ${arr[*]}
do
    bssize=$each
    if [ $1 -eq 1 ]; then
        test_libaio $bssize
    elif [ $1 -eq 2 ]; then
        test_spdk $bssize
    elif [ $1 -eq 3 ]; then
        test_psync $bssize
    else
        echo "invalid option!!"
        exit
    fi
done

