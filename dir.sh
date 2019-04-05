
peter@potter-PC:~/Desktop/x86/ics2018$ readlink -e nemu
/home/peter/Desktop/x86/ics2018/nemu

peter@potter-PC:~/Desktop/x86/ics2018$ mktemp -q
/tmp/tmp.e6oql66XoS

#创造1B 的随机值
head -c 1 /dev/urandom | hexdump -v -e '"%02x"'
#输出81
ubuntu@VM-0-8-ubuntu:~$ head -20 1 /dev/urandom | hexdump -v -e '"%02x"'
9dc3116c115eb8d14330126afaf846c25ca56570ubuntu

#测试
define git_commit
	-@git add .. -A --ignore-errors
	-@while (test -e .git/index.lock); do sleep 0.1; done
	-@(echo "> $(1)" && echo $(STUID) && id -un && uname -a && uptime && (head -c 20 /dev/urandom | hexdump -v -e '"%02x"') && echo) | git commit -F - $(GITFLAGS)
	-@sync
endef
@while (test -e .git/index.lock); do sleep 0.1; done

初始化gitosis并添加管理员
sudo -H -u git gitosis-init < /tmp/id_rsa.pub