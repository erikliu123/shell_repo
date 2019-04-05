
####################
#
#   sed操作
#
#   Sed is a non-interactive line editor. It receives text input, whether from stdin or from a file, performs certain operations on specified lines of the input, 
#   one line at a time, then outputs the result to stdout or to a file. Within a shell script, sed is usually one of several tool 
####################

#~/py/project下运行
#查看文件book.txt的第1行
 sed -n -e '1p'  sed.txt 

#查看文件book.txt的倒数第二、三行
tail -n 2 book.txt|sed -n -e "1p"
tail -n 3 book.txt|sed -n -e "1p"

sed -n 's/worker/**&**/pg' sed.txt 
#删除前导空格
sed 's/^[ ]*//' sed.txt

#删除所有空格
sed 's/[[:space:]]//g'  tmp.txt >aa.txt
sed 's/\ //g'

#删除每行开头的空格
sed 's/^ *//' tmp.txt 
sed 's/^[ ]*//' tmp.txt
sed 's/^[[:space:]]*//' tmp.txt

#在每行行首添加双引号"
sed 's/^/"&/g' tmp.txt
#在每行行尾添加双引号和逗号。有可能存在问题，应该是 sed 's/$/&",/g' tmp.txt ???
sed 's/$/",&/g' tmp.txt

#删除每行开头的空格键和TAB键,通常\s
sed 's/^[\s]*//g'


匹配ABC 或 123:
sed -n 's/\(ABC\|123\)/p'
awk '/ABC/||/123/ { print $0 }'
grep -E '(ABC|123)' 或 egrep 'ABC|123'

sed -n 's/\(^#\|^\s*$\|^\s*#\)//p'
sed  's/\(^#.*\|^\s*$\|^\s*#.*\)//gp' test.sh  ##TODO:删除了这些空格注释，但是该行没有被删除！
sed  -n '/\(^#.*\|^\s*$\|^\s*#.*\)/d' test.sh  ##使用d而不是s，解决问题


##Andvanced：注释格式从// 变成/**/
sed -i 's#^//\(.*\)#/\*\1\*/#' filename;
#Andvanced：删去文本中所有的换行 
sed ':a;N;$!ba;s/\n//g' sed.txt 


####################
#
#
#   grep操作
#
#
####################

#正则表达式背景知识
#\s
#匹配任何不可见字符，包括空格、制表符、换页符等等。等价于[ \f\n\r\t\v]。
#\S
#匹配任何可见字符。等价于[^ \f\n\r\t\v]。
#Andvanced
#匹配无意义的行：   ^\s*$


#1.删除文本中空行组成的行及#号注释的行 
grep -Eiv "^#|^$" test.sh
#2.在上个命令的基础下，删除了某行全是[\f\n\r\t\v]的行。  核心^\s*$
grep -Eiv "^#|^\s*$" test.sh
#3.继续优化，删除了[\s...#]的注释行。 核心^\s*#$
grep -Eiv "^#|^\s*$|^\s*#" test.sh ##和sed进行比对

##啥意思？？
awk '/^ +/{p++}/^?/{t=s=0;v++;k[v]=p;p=0;next}!t&&!/^[0-9]/{s++;a[s]=a[s]?a[s]"\t"$0:$0;}/^[0-9]+/{m[++q]=$0;if($0>n) n=$0}
END{for(i=1;i<=n;i++) {print a[i];if(i<=v) {t=t?t"\t"m[i]:m[i];d=d?d"\t?":"\n?"}}print t,d}'  filename


