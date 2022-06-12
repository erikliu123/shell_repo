#include <iostream>
//gcc -fPIC test.c -o example.so -shared  -I/usr/include/python3.6
extern "C"{ // 重要，因为使用g++编译时函数名会改变，比方print_msg(const char*)
            // 会编译成函数名 print_msg_char，这会导致python调用这个函数的时候
            // 找不到对应的函数名，这有加了 extern "C"，才会以C语言的方式进行
            // 编译，这样不会改变函数名字
        void print_msg(const char* s)
        {
            std::cout<<1122334455667788<<std::endl;
            std::cout<<s<<1234<<std::endl;
        }

        int add_Integer(int a,int b)
        {
                return a+b;
        }
}

/*
#首先生成example.so文件，再运行python程序
#gcc -fPIC test.c -o example.so -shared  -I/usr/include/python3.6
from ctypes import *
import os

sotest = cdll.LoadLibrary(os.getcwd()+ "/example.so")
sotest.print_msg("hello,my shared object used by python!")

*/