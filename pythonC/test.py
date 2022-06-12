from random import random
from time import time
from fast_tanh import fast_tanh #从fast_tanh*.so找到fast_tanh函数(首先调用setup.py生成动态库)
from fast_tanh import spdk_log #从fast_tanh*.so找到spdk_log函数
import ctypes

data = [random() for i in range(1000000)]    # 生成随机数据

start_time = time()              # 计算并统计时间
result = list(map(fast_tanh, data))
end_time = time()
print(end_time - start_time)     # 输出运行时间
str=b"liujiahong"
spdk_log(((str)))
#spdk_log(ctypes.c_char_p(id(str)))
