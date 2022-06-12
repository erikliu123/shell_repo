#include <cmath>
#include <iostream>

const double e = 2.7182818284590452353602874713527; 

double mysinh(double x)
{
    return (1 - pow(e, (-2 * x))) / (2 * pow(e, -x));
}

double mycosh(double x)
{
    return (1 + pow(e, (-2 * x))) / (2 * pow(e, -x));
}

double mytanh(double x)
{
    return mysinh(x) / mycosh(x);
}

void print_msg(const char *s)//(std::string s)
{
    std::cout<<1122334455667788<<std::endl;
    std::cout<<s<<1234<<std::endl;
}

int add_Integer(int a,int b)
{
        return a+b;
}