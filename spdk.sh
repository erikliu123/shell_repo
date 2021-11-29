TMPDIR=tmp
TEST_FILE=silly_test
WRITE_COUNT=2000

## fio parameters
TEST_OP=randread #randread
BSSIZE=64k
READ_SIZE=8M
IO_DEPTH=2
IO_ENGINE=spdk #libaio #psync

if [ ! -d $TMPDIR ];then
	mkdir $TMPDIR
else
	sudo rm -rf $TMPDIR
	mkdir $TMPDIR
fi

dd if=/dev/zero of=/tmp/test-fs.ext4 bs=1024 count=102400
mkfs.ext4 -b 4096 /tmp/test-fs.ext4

if [ $? -ne 0 ];then
	echo "fail to create file system..." && exit
fi

dd if=/dev/zero of=$TMPDIR/$TEST_FILE bs=4096 count=20000
umount $TMPDIR
echo "The image has been made successfully\n\n"

echo "FIO test for kernel EXT4"
sudo mount /tmp/test-fs.ext4 $TMPDIR
cd $TMPDIR
fio -filename=$TEST_FILE -direct=1 -iodepth $IO_DEPTH -thread -rw=$TEST_OP -ioengine=$IO_ENGINE -bs=$BSSIZE -size=$READ_SIZE  -numjobs=1  -group_reporting -name=mytest
echo  "\n\n"

cd ..
umount $TMPDIR
