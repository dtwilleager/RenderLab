#include "stdafx.h"
#include "Texture.h"

namespace RenderLab
{
  Texture::Texture(string name, size_t width, size_t height, size_t depth, size_t bitsPerPixel, size_t size, int format) :
    m_name(name),
    m_width(width),
    m_height(height),
    m_depth(depth),
    m_bitsPerPixel(bitsPerPixel),
    m_size(size),
    m_format(format),
    m_data(nullptr),
    m_graphicsData(nullptr)
  {
  }


  Texture::~Texture()
  {
    if (m_data != nullptr)
    {
      delete m_data;
    }
  }

  string Texture::getName()
  {
    return m_name;
  }

  size_t Texture::getWidth()
  {
    return m_width;
  }

  size_t Texture::getHeight()
  {
    return m_height;
  }

  size_t Texture::getDepth()
  {
    return m_depth;
  }

  size_t Texture::getBitsPerPixel()
  {
    return m_bitsPerPixel;
  }

  size_t Texture::getSize()
  {
    return m_size;
  }

  int Texture::getFormat()
  {
    return m_format;
  }

  void Texture::setData(unsigned char* data)
  {
    m_data = new unsigned char[m_size];
    memcpy(m_data, data, m_size);
  }

  unsigned char* Texture::getData()
  {
    return m_data;
  }

  void Texture::setGraphicsData(void * graphicsData)
  {
    m_graphicsData = graphicsData;
  }

  void* Texture::getGraphicsData()
  {
    return m_graphicsData;
  }
}
