#pragma once
#include <string>

using std::string;

namespace RenderLab
{
  class GraphicsContext
  {
  public:
    GraphicsContext(string name);
    ~GraphicsContext();

  protected:
    string m_name;
  };
}
