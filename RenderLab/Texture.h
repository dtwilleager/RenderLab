#pragma once
#include <string>

using std::string;

namespace RenderLab
{
  class Texture
  {
  public:
    Texture(string name, size_t width, size_t height, size_t depth, size_t bitsPerPixel, size_t size, int format);
    ~Texture();

    string          getName();
    size_t          getWidth();
    size_t          getHeight();
    size_t          getDepth();
    size_t          getBitsPerPixel();
    size_t          getSize();
    int             getFormat();
    void            setData(unsigned char* data);
    unsigned char * getData();
    void            setGraphicsData(void * graphicsData);
    void*           getGraphicsData();

  private:
    string          m_name;
    size_t          m_width;
    size_t          m_height;
    size_t          m_depth;
    size_t          m_bitsPerPixel;
    size_t          m_size;
    int             m_format;
    unsigned char*  m_data;
    void*           m_graphicsData;
  };
}

