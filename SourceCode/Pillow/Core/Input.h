#pragma once
#include "DirectXMath-apr2025/DirectXMath.h"

namespace Pillow::Input
{
   // Not all keys are valid on a specific platform.
   // e.g. A mice is not intended to be supported on Android.
   enum class GenricKey : char
   {
      // Screen touch
      TouchLeft, TouchRight,
      // Mice
      MiceMiddle, MiceLeft, MiceRight, MiceSide0, MiceSide1,
      // GamePad
      PadX, PadY, PadA, PadB,
      PadUp, PadDown, PadLeft, PadRight,
      PadLB, PadLT, PadRB, PadRT,
      PadReturn, PadMenu,
      StickLeft, StickRight,
      // Keyboard Keys
      // Letters
      A, B, C, D, E, F, G, H, I, J, K, L, M, N, O, P, Q, R, S, T, U, V, W, X, Y, Z,
      // Numbers
      Num0, Num1, Num2, Num3, Num4, Num5, Num6, Num7, Num8, Num9,
      // Function Keys
      F1, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12,
      // Symbols
      Backtick, Minus, Equals, BracketLeft, BracketRight, Backslash,
      Semicolon, Quote, Comma, Period, Slash,
      // Control Keys
      Esc, Tab, CapsLock, Shift, Ctrl, Alt, Space, Backspace, Enter,
      // Arrow Keys
      ArrowUp, ArrowDown, ArrowLeft, ArrowRight,
      Count
   };

   const char* const GenricKeyName[]{""};

   void InputInitialize(const void* params);
   void InputClose();
   inline void InputCallback(const void* messages) {/*dumb*/};

   using DirectX::XMFLOAT2;

#if defined(_WIN64)
   XMFLOAT2 GetMicePos();
   XMFLOAT2 GetMiceOffset();
   float GetWheelOffset();
#elif defined(__ANDROID__)
   XMFLOAT2 GetLeftTouchPos();
   XMFLOAT2 GetLeftTouchOffset();
   XMFLOAT2 GetRightTouchPos();
   XMFLOAT2 GetRightTouchOffset();
#endif
   bool GetKey(GenricKey key);
   bool GetKeyDown(GenricKey key);
   bool GetKeyUp(GenricKey key);
}