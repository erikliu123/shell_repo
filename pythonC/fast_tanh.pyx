# distutils: language = c++
# cython: language_level = 3

cdef extern from "mytanh.cpp":
    double mytanh(double x)

cdef extern from "mytanh.cpp":
    void print_msg(const char *s)

def fast_tanh(double x):
    return mytanh(x)

def spdk_log(s):
    return print_msg(s)