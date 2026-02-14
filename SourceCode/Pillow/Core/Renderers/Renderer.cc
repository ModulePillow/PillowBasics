#include "Renderer.h"
#include <ranges>
#include <algorithm>

using namespace Pillow;
using namespace Pillow::Graphics;
using namespace DirectX;

int32_t Pillow::Graphics::RefreshRate;
XMINT2 Pillow::Graphics::ScreenSize;
std::unique_ptr<GenericRenderer> Pillow::Graphics::Instance;

namespace
{
   // Asynchronous vs synchronous multithreading rendering
   // 
   // If the CPU job comprises two parts: Tt(tick computation time) and Tg(graphics compuation time)
   // 
   // An async method with one buffer generates a higher frame rate but with a longer delay.
   // The maximum span of a CPU frame is 2*Tg (Tt < Tg) or Tt + Tg (Tt >= Tg), if we don't take into account the GPU.
   // 
   // On the other hand, a sync method generates a lower framerate, but provides a better delay.
   // The maximum span is Tt + Tg, if we don't take into account the GPU.
   //
   // We choose the first method for a better performance.

   std::vector<Drawcall> cachedDrawcalls;
   std::vector<Drawcall> submittedDrawcalls;

   std::vector<std::thread> workers;
   std::optional<std::barrier<void(*)() noexcept>> frameBarrier;
   std::atomic<bool> signal_IsActive;
   std::atomic<bool> signal_IsComputing;

   ForceInline std::vector<KeyValuePair> Sort(const std::vector<KeyValuePair>& macros)
   {
      std::vector<KeyValuePair> result = macros;
      std::sort(result.begin(), result.end());
      return result;
   }
}

GenericPipelineConfig::GenericPipelineConfig(string name, const std::vector<KeyValuePair>& macros,
   const std::vector<string>& cbv, const std::vector<string>& vsTex, const std::vector<string>& psTex, int32_t rtNum) :
   Macros(Sort(macros)),
   VSTextures(vsTex),
   PSTextures(psTex),
   ConstantBuffers(cbv),
   RenderTargetCount(rtNum)
{
   const char prefixMacro = '@';
   const char prefixValue = '=';
   for (const auto& pair : Macros)
   {
      name += "@" + pair.GetKey();
      if (!pair.IsKeyOnly())
      {
         name += "=" + pair.GetValueRaw();
      }
   }
   ConfigName = name;
   //auto view = std::ranges::split_view(name, _char0) | std::ranges::views::drop(1);
   //_Macros.reserve(std::ranges::distance(view));
   //for (auto&& _macro : view)
   //{
   //   // Uses std::string_view to avoid a string copy.
   //   auto subView = std::ranges::split_view(std::string_view(_macro.begin(), _macro.end()), _char1);
   //   auto iterator = subView.begin();
   //   ShaderMacro macro;
   //   macro.Name = string((*iterator).begin(), (*iterator).end());
   //   iterator++;
   //   if (iterator != subView.end())
   //   {
   //      macro.Value = string((*iterator).begin(), (*iterator).end());
   //   }
   //   _Macros.push_back(std::move(macro));
   //}
}

bool GenericPipelineConfig::EqualTo(const GenericPipelineConfig& right) const
{
   return this->ConfigName == right.ConfigName;
}

static void Pillow::Graphics::BarrierCompletionAction() noexcept
{
   if(Instance) Instance->Assembler();
   signal_IsComputing.store(false, std::memory_order::release);
}

GenericRenderer::GenericRenderer(int32_t threadCount, std::string name) :
   f_RendererName(name),
   f_ThreadCount(threadCount)
{
   workers.reserve(threadCount);
   frameBarrier.emplace(threadCount, BarrierCompletionAction);
   signal_IsActive.store(true);
   signal_IsComputing.store(false);
}

GenericRenderer::~GenericRenderer()
{
   workers.clear();
   frameBarrier.reset();
}

void GenericRenderer::Launch()
{
   for (int32_t i = 0; i < f_ThreadCount; i++)
   {
      workers.emplace_back(std::thread(&GenericRenderer::BaseWorker, this, i));
   }
}

void GenericRenderer::Terminate()
{
   signal_IsActive.store(false, std::memory_order::release);
   for (auto& thread : workers)
   {
      if (thread.joinable()) thread.join();
   }
}

void GenericRenderer::Commit()
{
   while (signal_IsComputing.load(std::memory_order::acquire)) std::this_thread::yield();
   this->Pioneer();
   signal_IsComputing.store(true, std::memory_order::release);
}

//#include <Windows.h>
//#include <format>
void GenericRenderer::BaseWorker(int32_t workerIndex)
{
   while(true)
   {
      while (!signal_IsComputing.load(std::memory_order::acquire))
      {
         if (!signal_IsActive.load(std::memory_order::acquire)) return;
         std::this_thread::yield();
      }
      //OutputDebugString(std::format(L"Frame={} Worker={}\n", this->GetFrameIndex(), workerIndex).c_str());
      this->Worker(workerIndex);
      frameBarrier->arrive_and_wait();
   }
}