#include "stdafx.h"
#include "UniformBuffer.h"

namespace RenderLab
{
  UniformBuffer::UniformBuffer(string name, size_t size):
    m_name(name),
    m_size(size),
    m_graphicsData(nullptr)
  {
  }


  UniformBuffer::~UniformBuffer()
  {
  }

  size_t UniformBuffer::getSize()
  {
    return m_size;
  }

  void UniformBuffer::setGraphicsData(void * graphicsData)
  {
    m_graphicsData = graphicsData;
  }

  void* UniformBuffer::getGraphicsData()
  {
    return m_graphicsData;
  }

}