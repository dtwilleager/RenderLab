#pragma once

#include "Component.h"

#include <string>

using std::string;

namespace RenderLab
{
  class ProcessorComponent: public Component
  {
  public:
    ProcessorComponent(string name, bool sendKeyboardEvents, bool sendMouseEvents);
    ~ProcessorComponent();

    bool sendKeyboardEvents();
    bool sendMouseEvents();

    virtual void handleKeyboard(MSG* event);
    virtual void handleMouse(MSG* event);
    virtual void execute(double absoluteTime, double deltaTime);

  private:
    bool m_sendKeyboardEvents;
    bool m_sendMouseEvents;
  };
}
