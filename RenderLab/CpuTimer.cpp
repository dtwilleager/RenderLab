#include "stdafx.h"
#include "CpuTimer.h"

namespace RenderLab
{
  CpuTimer::CpuTimer()
  {
  }


  CpuTimer::~CpuTimer()
  {
  }

  void CpuTimer::start()
  {
    m_startTime = high_resolution_clock::now();
  }

  unsigned long long CpuTimer::elapsedMicro()
  {
    high_resolution_clock::time_point endTime = high_resolution_clock::now();
    microseconds total_ms = duration_cast<microseconds>(endTime - m_startTime);
    return total_ms.count();
  }

  unsigned long long CpuTimer::elapsedMilli()
  {
    high_resolution_clock::time_point endTime = high_resolution_clock::now();
    milliseconds total_ms = duration_cast<milliseconds>(endTime - m_startTime);
    return total_ms.count();
  }
}