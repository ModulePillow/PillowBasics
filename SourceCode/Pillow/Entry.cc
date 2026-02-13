#include <iostream>
#include <thread>
#include "DirectXMath-apr2025/DirectXMath.h"
#include "Core/Constants.h"
#include "Core/Renderers/Renderer.h"
#include "Core/Input.h"
#include "Core/Auxiliaries.h"
#if defined(_WIN64)
#define NOMINMAX
#include <Windows.h>
#include <WinUser.h>
#undef NOMINMAX
#elif defined(__ANDROID__)
#endif

extern void TempCode();

// Static definitions. External code cannot access those contents.
namespace
{
   using namespace Pillow;
   using namespace DirectX; // DXMath

   void EngineLaunch();
   void EngineTick();
   void EngineTerminate();

   GameClock GlobalClock;

#if defined(_WIN64)
   HWND hwnd;
   uint64_t timerHandle;
   bool isFullscreen = false;
   XMINT2 screenOrigin;
   const XMINT2 minClientSize{ 400, 300 };
   XMINT2 minWindowSize; // The border makes the window size bigger than the client size.

   void CreateGameWindow(HINSTANCE hInstance, int show);
   void GameMessageLoop();
   LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
   void CALLBACK TimerEvent(HWND hwnd, UINT arg1, UINT_PTR arg2, DWORD arg3);
   void GetMonitorParams();
   void GetWindowSize();
   void SetWindowMode(bool fullScreen, bool allowResizing = true);

   void CreateGameWindow(HINSTANCE hInstance, int nShowCmd)
   {
      GetMonitorParams();
      GetWindowSize();
      int32_t posX = screenOrigin.x + (ScreenSize.x - minWindowSize.x) / 2;
      int32_t posY = screenOrigin.y + (ScreenSize.y - minWindowSize.y) / 2;
      // 1 Register Window
      const wchar_t* className = L"PillowBasics";
      WNDCLASS windowSettings{};
      windowSettings.style = CS_HREDRAW | CS_VREDRAW;
      windowSettings.lpfnWndProc = WndProc;
      windowSettings.cbClsExtra = 0;
      windowSettings.cbWndExtra = 0;
      windowSettings.hInstance = hInstance;
      windowSettings.hIcon = LoadIcon(0, IDI_APPLICATION);
      windowSettings.hCursor = LoadCursor(0, IDC_ARROW);
      windowSettings.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
      windowSettings.lpszMenuName = 0;
      windowSettings.lpszClassName = className;
      if (!RegisterClass(&windowSettings))
      {
         MessageBoxA(0, "RegisterClass FAILED", 0, MB_OK);
         exit(EXIT_FAILURE);
      }
      // 2 Create and show window
      hwnd = CreateWindow(className, L"DefaultTitle", WS_OVERLAPPEDWINDOW, posX, posY, minWindowSize.x, minWindowSize.y, 0, 0, hInstance, 0);
      if (hwnd == 0)
      {
         MessageBoxA(0, "CreateWindow FAILED", 0, MB_OK);
         exit(EXIT_FAILURE);
      }
      // 3 Display Window
      ShowWindow(hwnd, nShowCmd);
      UpdateWindow(hwnd); // Update the window before initializing the game engine.
   }

   void GameMessageLoop()
   {
      try
      {
         EngineLaunch();
#ifdef PILLOW_DEBUG
         TempCode();
#endif
         MSG message{};
         while (message.message != WM_QUIT)
         {
            if (PeekMessage(&message, 0, 0, 0, PM_REMOVE))
            {
               TranslateMessage(&message);
               DispatchMessage(&message);
            }
            else
            {
               EngineTick();
            }
         }
         EngineTerminate();
      }
      catch (std::exception& e)
      {
         EngineTerminate();
         MessageBoxA(hwnd, e.what(), 0, MB_OK);
         exit(EXIT_FAILURE);
      }
   }

   // Recieve message （callback from system）
   LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
   {
      switch (msg)
      {
      case WM_INPUT:
         // Process raw input here if needed
         Pillow::Input::InputCallback((const void*)lParam);
         return 0;
         break;
      case WM_GETMINMAXINFO:
      {
         auto& info = *(MINMAXINFO*)lParam;
         info.ptMinTrackSize.x = minWindowSize.x;
         info.ptMinTrackSize.y = minWindowSize.y;
         break;
      }
      case WM_KEYDOWN:
         if (wParam == VK_F11)
         {
            SetWindowMode(!isFullscreen, true); // Toggle fullscreen mode
         }
         break;
      case WM_ENTERSIZEMOVE:
         // 1. When users resize or move the form, the program will be trapped in a modal loop,
         // which will stop our message loop and causing the engine not to run.
         // So create a timer to run the engine.
         // 2. USER_TIMER_MINIMUM: Let the renderer determines the resizing-check interval,
         // which provides a much higher framerate.
         timerHandle = SetTimer(hwnd, 1, USER_TIMER_MINIMUM, TimerEvent);
         break;
      case WM_EXITSIZEMOVE:
         if (timerHandle != 0)
         {
            KillTimer(hwnd, timerHandle); // Stop the timer
            timerHandle = 0;
         }
         break;
      case WM_DESTROY:// End message loop
         PostQuitMessage(0);
         return 0;
      }
      // Default procedure
      return DefWindowProc(hwnd, msg, wParam, lParam);
   }

   void TimerEvent(HWND hwnd, UINT arg1, UINT_PTR arg2, DWORD arg3)
   {
      EngineTick();
   }

   void GetMonitorParams()
   {
      HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
      MONITORINFOEX info = { sizeof(MONITORINFOEX) };
      if (GetMonitorInfo(monitor, &info) == FALSE)
      {
         MessageBoxA(0, "GetMonitorInfo FAILED", 0, MB_OK);
         exit(EXIT_FAILURE);
      }
      screenOrigin = XMINT2{ info.rcMonitor.left, info.rcMonitor.top };
      DEVMODE devMode{ 0 };
      devMode.dmSize = sizeof(DEVMODE);
      if (EnumDisplaySettings(info.szDevice, ENUM_CURRENT_SETTINGS, &devMode) == FALSE)
      {
         MessageBoxA(0, "EnumDisplaySettings FAILED", 0, MB_OK);
         exit(EXIT_FAILURE);
      }
      RefreshRate = int32_t(devMode.dmDisplayFrequency);
      ScreenSize = XMINT2{ int32_t(devMode.dmPelsWidth), int32_t(devMode.dmPelsHeight) };
   }

   void GetWindowSize()
   {
      RECT rect{ 0,0,minClientSize.x,minClientSize.y };
      AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);
      minWindowSize = XMINT2{ rect.right - rect.left, rect.bottom - rect.top };
   }

   void SetWindowMode(bool fullScreen, bool allowResizing)
   {
      static RECT lastRect{};
      const uint32_t flags = SWP_NOZORDER | SWP_FRAMECHANGED | SWP_SHOWWINDOW;
      if (fullScreen && !isFullscreen) // To fullscreen
      {
         GetMonitorParams();
         GetWindowRect(hwnd, &lastRect);
         SetWindowLongPtr(hwnd, GWL_STYLE, WS_OVERLAPPED);
         SetWindowPos(hwnd, 0, screenOrigin.x, screenOrigin.y, ScreenSize.x, ScreenSize.y, flags);
      }
      else if (!fullScreen && isFullscreen) // To a window
      {
         SetWindowLongPtr(hwnd, GWL_STYLE, WS_OVERLAPPEDWINDOW & (allowResizing ? UINT32_MAX : !WS_THICKFRAME));
         SetWindowPos(hwnd, 0, lastRect.left, lastRect.top, lastRect.right - lastRect.left, lastRect.bottom - lastRect.top, flags);
      }
      isFullscreen = fullScreen;
   }
#elif defined(__ANDROID__)
#endif

void EngineLaunch()
{
   GlobalClock.Start();
   Constants::SetThreadNumbers();
#if defined(_WIN64)
   Graphics::InitializeRenderer(Constants::ThreadNumRenderer, (void*)&hwnd);
#elif defined(__ANDROID__)
   //...
#endif
   Graphics::Instance->Launch();
   return;
}

void EngineTick()
{
   GlobalClock.Tick();
   Graphics::Instance->Commit();
   //Pillow::Input::Update();
}

void EngineTerminate()
{
   Graphics::Instance->Terminate();
   Graphics::Instance.reset();
}
}

extern double TEMP_GetDeltaTime()
{
   return GlobalClock.GetDeltaTime();
}

extern double TEMP_GetLastingTime()
{
   return GlobalClock.GetLastingTime();
}

#if defined(_WIN64)
// Program Entry Point
int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ int nShowCmd)
{
   CreateGameWindow(hInstance, nShowCmd);
   GameMessageLoop();
   return EXIT_SUCCESS;
}
#elif defined(__ANDROID__)
#endif