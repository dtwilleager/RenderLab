#pragma once

#include <chrono>

using std::chrono::high_resolution_clock;
using std::chrono::microseconds;
using std::chrono::milliseconds;
using std::chrono::duration_cast;

namespace RenderLab
{
  class CpuTimer
  {
  public:
    CpuTimer();
    ~CpuTimer();

    void                start();
    unsigned long long  elapsedMicro();
    unsigned long long  elapsedMilli();

  private:
    high_resolution_clock::time_point  m_startTime;
  };
}

