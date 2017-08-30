#pragma once

#include <string>
#include <memory>

using std::string;
using std::shared_ptr;

namespace RenderLab
{
  class UniformBuffer
  {
  public:
    UniformBuffer(string name, size_t size);
    ~UniformBuffer();

    size_t  getSize();
    void    setGraphicsData(void * graphicsData);
    void*   getGraphicsData();

  private:
    string  m_name;
    size_t  m_size;
    void *  m_graphicsData;
  };
}
