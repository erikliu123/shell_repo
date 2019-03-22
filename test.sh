#应用背景
#服务器爬书时，有一些垃圾PDF（文件太小无法打开），而且很多，于是写了一个shell脚本，记录下这些文件的名字
#后期也可以rm -i inode号码进行删除


#发现有几个探讨的问题
#1.文件大小由小到大依次排列，但是这个排列是以字符串字典序列排的 ls -li | sort -k6r  
#2. read line读的东西不能直接转化成字符串数组，需要使用${string//|/}这个有用的东西
#3. 

#TODO:
#删除的文件需要.pdf后缀才删除，一开始可以grep line的信息
#remove_books一开始可以加一个日期

book="book.txt"
remove_books="dummy-books.log"
she=`ls -1`
count=0

pre=`pwd`
if [ ! -d  $book ]
then
	touch $book;
	if [ $? -ne 0 ];then
		echo "can't touch $book" 
		exit -1
	fi
fi

cd ..
parent=`pwd`
if [ ! -d  $remove_books ]
then
        touch $remove_books;
        if [ $? -ne 0 ];then
                echo "can't touch $remove_books"
		cd $pre 
                exit -1
        fi
fi
cd $pre

remove_books=$parent"/"$remove_books #不要使用“+”  $parent+"/"+$remove_books错误
ls -li > $book;
#echo $she

cat $book | while read line
do
echo $line | awk '{print $6}' | while read temp
do
if [ $temp -le 565 ]
then
	NUM=1
	str=${line//|/ } #将读入的line以空格拆分，保存到数组
	#echo str 长度: ${#str[@]}
	#for each in ${str[*]}
    	#do
    	#	echo $each
   	#done
    arr=($str)
    echo arr 长度：${#arr[@]}
   	for each in ${arr[*]}
    	do
		if [ $NUM -ge 10 ]; then  #第10列开始才是文件的名字，因为文件名可以有空格，故需要多次读入
			
    			echo -n "$each "
			echo -n "$each " >> $remove_books
		fi
		NUM=$(( $NUM + 1 ))
		
    	done
	echo ""
	echo "" >>$remove_books #文件换行
        
	#line 本身不是元组，所以赋值也不会按照元组的方式赋值？
	#测试为什么line直接赋值不行？
	arrayall=$line
	echo " length = ${#arrayall[@]}"  #一直显示1
	#echo  $line | awk '{print $10}' 
	#TODO:remove  inode
	#rm -i ``echo $line | awk '{print $1}'`
fi
done
#echo $size
done
exit 0


#CAN IGNORE!  useless
for i in $she;
do
	count=`expr $count + 1 `
if [ $count -eq 100 ]
then
	echo "finish"
	exit -1
fi
echo $i
size= `echo $i | awk '{print $7}'`
echo $size;
done   
