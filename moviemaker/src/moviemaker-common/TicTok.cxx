
#include "TicTok.hxx"

#include <omp.h>
#include <iostream>
#include <algorithm>

TicTok::TicTok()
{
    _start.resize(omp_get_max_threads());
    _sum.resize(omp_get_max_threads(), 0);
}

void TicTok::Start()
{
    auto t = omp_get_thread_num();
    _start[t] = std::chrono::system_clock::now();  
}

void TicTok::Stop()
{
    auto end = std::chrono::system_clock::now();
    auto t = omp_get_thread_num();

    auto d = std::chrono::duration_cast<std::chrono::microseconds>(end - _start[t]).count();

    _sum[t] += d;   
}

void TicTok::Report(const std::string& what) 
{
    long s = 0;
    std::for_each(_sum.begin(), _sum.end(), [&](auto n) 
    { 
        s += n; 
    });
    std::cout << "  Time taken for " << what << " = " << double(s) / 1000000.0 << " s" << std::endl;
}
