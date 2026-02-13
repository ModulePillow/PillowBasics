#include "Input.h"
#include "Renderers/Renderer.h"
#include<map>
#if defined(_WIN64)
#include <Windows.h>
#include <winuser.h>
#include <hidusage.h>
#include <xinput.h>
#elif defined(__ANDROID__)
#endif

using namespace Pillow;
using namespace Pillow::Input;

namespace
{
   enum class State : char
   {
      Released,
      Pressed,
      Down,
      Up
   };

#if defined(_WIN64)
   const char TriggerThreshold = XINPUT_GAMEPAD_TRIGGER_THRESHOLD;

   std::map<int32_t, GenricKey> KeyMapping =
   {
      // Mouse Buttons
      {VK_LBUTTON, GenricKey::MiceLeft},
      {VK_RBUTTON, GenricKey::MiceRight},
      {VK_MBUTTON, GenricKey::MiceMiddle},
      {VK_XBUTTON1, GenricKey::MiceSide0},
      {VK_XBUTTON2, GenricKey::MiceSide1},
      // GamePad Buttons (XInput)
      // GamePad Triggers are analog buttons, so we need to check their values.
      {XINPUT_GAMEPAD_A, GenricKey::PadA},
      {XINPUT_GAMEPAD_B, GenricKey::PadB},
      {XINPUT_GAMEPAD_X, GenricKey::PadX},
      {XINPUT_GAMEPAD_Y, GenricKey::PadY},
      {XINPUT_GAMEPAD_DPAD_UP, GenricKey::PadUp},
      {XINPUT_GAMEPAD_DPAD_DOWN, GenricKey::PadDown},
      {XINPUT_GAMEPAD_DPAD_LEFT, GenricKey::PadLeft},
      {XINPUT_GAMEPAD_DPAD_RIGHT, GenricKey::PadRight},
      {XINPUT_GAMEPAD_BACK, GenricKey::PadReturn},
      {XINPUT_GAMEPAD_START, GenricKey::PadMenu},
      {XINPUT_GAMEPAD_LEFT_THUMB, GenricKey::StickLeft},
      {XINPUT_GAMEPAD_RIGHT_THUMB, GenricKey::StickRight},
      // Keyboard Letters
      {'A', GenricKey::A},
      {'B', GenricKey::B},
      {'C', GenricKey::C},
      {'D', GenricKey::D},
      {'E', GenricKey::E},
      {'F', GenricKey::F},
      {'G', GenricKey::G},
      {'H', GenricKey::H},
      {'I', GenricKey::I},
      {'J', GenricKey::J},
      {'K', GenricKey::K},
      {'L', GenricKey::L},
      {'M', GenricKey::M},
      {'N', GenricKey::N},
      {'O', GenricKey::O},
      {'P', GenricKey::P},
      {'Q', GenricKey::Q},
      {'R', GenricKey::R},
      {'S', GenricKey::S},
      {'T', GenricKey::T},
      {'U', GenricKey::U},
      {'V', GenricKey::V},
      {'W', GenricKey::W},
      {'X', GenricKey::X},
      {'Y', GenricKey::Y},
      {'Z', GenricKey::Z},
      // Keyboard Numbers
      {'0', GenricKey::Num0},
      {'1', GenricKey::Num1},
      {'2', GenricKey::Num2},
      {'3', GenricKey::Num3},
      {'4', GenricKey::Num4},
      {'5', GenricKey::Num5},
      {'6', GenricKey::Num6},
      {'7', GenricKey::Num7},
      {'8', GenricKey::Num8},
      {'9', GenricKey::Num9},
      // Function Keys
      {VK_F1, GenricKey::F1},
      {VK_F2, GenricKey::F2},
      {VK_F3, GenricKey::F3},
      {VK_F4, GenricKey::F4},
      {VK_F5, GenricKey::F5},
      {VK_F6, GenricKey::F6},
      {VK_F7, GenricKey::F7},
      {VK_F8, GenricKey::F8},
      {VK_F9, GenricKey::F9},
      {VK_F10, GenricKey::F10},
      {VK_F11, GenricKey::F11},
      {VK_F12, GenricKey::F12},
      // Symbols
      {VK_OEM_3, GenricKey::Backtick},        // `~ key
      {VK_OEM_MINUS, GenricKey::Minus},       // -_ key
      {VK_OEM_PLUS, GenricKey::Equals},       // =+ key
      {VK_OEM_4, GenricKey::BracketLeft},     // [{ key
      {VK_OEM_6, GenricKey::BracketRight},    // ]} key
      {VK_OEM_5, GenricKey::Backslash},       // \| key
      {VK_OEM_1, GenricKey::Semicolon},       // ;: key
      {VK_OEM_7, GenricKey::Quote},           // '" key
      {VK_OEM_COMMA, GenricKey::Comma},       // ,< key
      {VK_OEM_PERIOD, GenricKey::Period},     // .> key
      {VK_OEM_2, GenricKey::Slash},           // /? key
      // Control Keys
      {VK_ESCAPE, GenricKey::Esc},
      {VK_TAB, GenricKey::Tab},
      {VK_CAPITAL, GenricKey::CapsLock},
      {VK_SHIFT, GenricKey::Shift},           // Covers both left and right Shift
      {VK_CONTROL, GenricKey::Ctrl},          // Covers both left and right Ctrl
      {VK_MENU, GenricKey::Alt},              // Covers both left and right Alt
      {VK_SPACE, GenricKey::Space},
      {VK_BACK, GenricKey::Backspace},
      {VK_RETURN, GenricKey::Enter},
      // Arrow Keys
      {VK_UP, GenricKey::ArrowUp},
      {VK_DOWN, GenricKey::ArrowDown},
      {VK_LEFT, GenricKey::ArrowLeft},
      {VK_RIGHT, GenricKey::ArrowRight}
   };
#elif defined(__ANDROID__)
#endif

   std::map<GenricKey, State> keyStates{};
   uint64_t frameIndex = UINT64_MAX;
#if defined(_WIN64)
   XMFLOAT2 micePos{};
   XMFLOAT2 miceOffset{};
   float wheelOffset{};
#elif defined(__ANDROID__)
#endif
}

void Pillow::Input::InputInitialize(const void* params)
{
   for (int32_t i = 0; i < (int32_t)GenricKey::Count; i++)
   {
      keyStates.emplace((GenricKey)i, State::Released);
   }
#if defined(_WIN64)
   alignas(32) RAWINPUTDEVICE devices[3]
   {
      {HID_USAGE_PAGE_GENERIC, HID_USAGE_GENERIC_MOUSE, 0, 0}, // Mice
      {HID_USAGE_PAGE_GENERIC, HID_USAGE_GENERIC_KEYBOARD, 0, 0}, // Keyboard
      {HID_USAGE_PAGE_GAME, HID_USAGE_GENERIC_GAMEPAD, 0, 0} // Gamepad
   };
   RegisterRawInputDevices(devices, 3, sizeof(RAWINPUTDEVICE));
#elif defined(__ANDROID__)
#endif
}

void Pillow::Input::InputClose()
{
#if defined(_WIN64)
   alignas(32) RAWINPUTDEVICE devices[3]
   {
      {HID_USAGE_PAGE_GENERIC, HID_USAGE_GENERIC_MOUSE, RIDEV_REMOVE, 0}, // Mice
      {HID_USAGE_PAGE_GENERIC, HID_USAGE_GENERIC_KEYBOARD, RIDEV_REMOVE, 0}, // Keyboard
      {HID_USAGE_PAGE_GAME, HID_USAGE_GENERIC_GAMEPAD, RIDEV_REMOVE, 0} // Gamepad
   };
   RegisterRawInputDevices(devices, 3, sizeof(RAWINPUTDEVICE));
#elif defined(__ANDROID__)
#endif
}

void Pillow::Input::InputCallback(const void* messages)
{
   // Update once per frame.
   auto newFrameIndex = Graphics::Instance->GetFrameIndex();
   if (newFrameIndex != frameIndex)
   {
      frameIndex = newFrameIndex;
      for (auto& keyState : keyStates)
      {
         if (keyState.second == State::Up)
         {
            keyState.second = State::Released;
         }
         else if (keyState.second == State::Down)
         {
            keyState.second = State::Pressed;
         }
      }
#if defined(_WIN64)
      miceOffset = { 0,0 };
      wheelOffset = 0;
#elif defined(__ANDROID__)
#endif
   }
   // Message processing.
#if defined(_WIN64)
   const RAWINPUT rawInput = *reinterpret_cast<const RAWINPUT*>(messages);
   if (rawInput.header.dwType == RIM_TYPEKEYBOARD)
   {
      auto key = (GenricKey)rawInput.data.keyboard.VKey;
      if (keyStates.contains(key))
      {
         switch (rawInput.data.keyboard.Message)
         {
         case WM_KEYDOWN:
            keyStates[key] = State::Down;
            break;
         case WM_KEYUP:
            keyStates[key] = State::Up;
            break;
         }
      }
   }
   else if (rawInput.header.dwType == RIM_TYPEMOUSE)
   {
      micePos.x += rawInput.data.mouse.lLastX;
      micePos.y += rawInput.data.mouse.lLastY;
      miceOffset.x += rawInput.data.mouse.lLastX;
      miceOffset.y += rawInput.data.mouse.lLastY;
      wheelOffset += rawInput.data.mouse.usButtonFlags & RI_MOUSE_WHEEL ? (float)(rawInput.data.mouse.usButtonData / WHEEL_DELTA) : 0.0f;
   }
#elif defined(__ANDROID__)

#endif
}

#if defined(_WIN64)
XMFLOAT2 Pillow::Input::GetMicePos()
{
   return micePos;
}

XMFLOAT2 Pillow::Input::GetMiceOffset()
{
   return miceOffset;
}

float Pillow::Input::GetWheelOffset()
{
   return wheelOffset;
}
#elif defined(__ANDROID__)
XMFLOAT2 Pillow::Input::GetLeftTouchPos()
{
   return XMFLOAT2();
}

XMFLOAT2 Pillow::Input::GetLeftTouchOffset()
{
   return XMFLOAT2();
}

XMFLOAT2 Pillow::Input::GetRightTouchPos()
{
   return XMFLOAT2();
}

XMFLOAT2 Pillow::Input::GetRightTouchOffset()
{
   return XMFLOAT2();
}
#endif

bool Pillow::Input::GetKey(GenricKey key)
{
   return keyStates[key] == State::Pressed || keyStates[key] == State::Down;
}

bool Pillow::Input::GetKeyDown(GenricKey key)
{
   return keyStates[key] == State::Down;
}

bool Pillow::Input::GetKeyUp(GenricKey key)
{
   return keyStates[key] == State::Up;
}


