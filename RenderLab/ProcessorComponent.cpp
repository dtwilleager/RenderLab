#include "stdafx.h"
#include "ProcessorComponent.h"

namespace RenderLab
{
  ProcessorComponent::ProcessorComponent(string name, bool sendKeyboardEvents, bool sendMouseEvents): Component(name, Component::PROCESS),
    m_sendKeyboardEvents(sendKeyboardEvents),
    m_sendMouseEvents(sendMouseEvents)
  {
  }


  ProcessorComponent::~ProcessorComponent()
  {
  }

  bool ProcessorComponent::sendKeyboardEvents()
  {
    return m_sendKeyboardEvents;
  }

  bool ProcessorComponent::sendMouseEvents()
  {
    return m_sendMouseEvents;
  }

  void ProcessorComponent::handleKeyboard(MSG* event)
  {

  }

  void ProcessorComponent::handleMouse(MSG* event)
  {

  }

  void ProcessorComponent::execute(double absoluteTime, double deltaTime)
  {
  }
}
