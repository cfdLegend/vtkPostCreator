
#ifndef TICTOC
#define TICTOC

#include <chrono>
#include <string>
#include <vector>

using TimePoint = std::chrono::time_point<std::chrono::system_clock>;

// thread safe (sums over threads) clock
class TicTok
{
public:
    TicTok();
    void Start();
    void Stop();
    void Report(const std::string& what);
private:
    std::vector<TimePoint> _start;
    std::vector<long> _sum;
};

#endif
