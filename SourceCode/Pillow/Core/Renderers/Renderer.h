#pragma once
#include <thread>
#include <barrier>
#include <atomic>
#include <vector>
#include <functional>
#include "../Auxiliaries.h"
#include "../Constants.h"
#include "../Texture.h"
#include "../Mesh.h"

using namespace Pillow::Graphics;
using namespace DirectX;

namespace Pillow::Graphics
{
   // Perceptual weightings for the importance of each channel.
   const XMVECTOR RGBLuminance = XMVectorSet(0.2125f / 0.7154f, 1, 0.0721f / 0.7154f, 1);
   const XMVECTOR RGBLuminanceInv = XMVectorSet(0.7154f / 0.2125f, 1, 0.7154f / 0.0721f, 1);

   extern int32_t RefreshRate;
   extern XMINT2 ScreenSize;
   class GenericRenderer;
   extern std::unique_ptr<GenericRenderer> Instance;

   typedef uint32_t ResourceHandle;

   enum class ResourceType : uint32_t
   {
      None = 0,
      Mesh = 1 << 28,
      Texture = 2 << 28,
      PiplelineState = 3 << 28,
      ConstantBuffer = 4 << 28,
   };

   struct Drawcall
   {
      void* sth;
   };

   class GenericPipelineConfig
   {
   public:
      enum struct TopologyType : uint8_t
      {
         TriangleList, // vertex buffer + index buffer
         TriangleStrip // only vertex buffer
      };

   public:
      string ConfigName;
      std::vector<KeyValuePair> Macros;
      std::vector<string> VSTextures;
      std::vector<string> PSTextures;
      std::vector<string> ConstantBuffers;
      int32_t RenderTargetCount;
      TopologyType Topology;

      ForceInline GenericPipelineConfig() : ConfigName("NullConfig") {};

      // Example
      // ConfigName: SimpleShader@CheckOn@Quality=2
      GenericPipelineConfig(string name, const std::vector<KeyValuePair>& macros,
         const std::vector<string>& cbv, const std::vector<string>& vsTex, const std::vector<string>& psTex, int32_t rtNum, TopologyType topology);

      bool EqualTo(const GenericPipelineConfig& right) const;
   };

   class GenericRenderer
   {
      DeleteDefautedMethods(GenericRenderer)
         ReadonlyProperty(string, RendererName)
         ReadonlyProperty(int32_t, ThreadCount)

   public:
      virtual ~GenericRenderer() = 0;
      virtual uint64_t GetFrameIndex() = 0;
      ForceInline int32_t GetFrameArrayIdx() { return GetFrameIndex() % Constants::SwapChainSize; }
      virtual void ReleaseResource(uint32_t handle) = 0;
      void Launch();
      void Terminate();
      void Commit();

   protected:
      GenericRenderer(int32_t threadCount, string name);
      virtual void Worker(int32_t workerIndex) = 0;
      virtual void Pioneer() = 0;
      virtual void Assembler() = 0;

   private:
      void BaseWorker(int32_t workerIndex);
      friend static void BarrierCompletionAction() noexcept;
   };

#if defined(_WIN64)
   class D3D12Renderer final: public GenericRenderer
   {
      DeleteDefautedMethods(D3D12Renderer)

   public:
      D3D12Renderer(HWND windowHandle, int32_t threadCount);
      ~D3D12Renderer();
      uint64_t GetFrameIndex();
      void ReleaseResource(uint32_t handle);

   private:
      void Worker(int32_t workerIndex);
      void Pioneer();
      void Assembler();
   };
#elif defined(__ANDROID__)
   //class GLES32Renderer : public GenericRenderer
   //{
   //   DeleteDefautedMethods(GLES32Renderer)
   //public:
   //   GLES32Renderer(HWND windowHandle);
   //   ~GLES32Renderer();
   //   int32_t CreateMesh() override;
   //   int32_t CreateTexture() override;
   //   int32_t CreatePiplelineState() override;
   //   int32_t CreateConstantBuffer() override;
   //   void ReleaseResource(int32_t handle) override;
   //   void CPUFrameBegin() override;
   //   void CPUFrameEnd() override;
   //};

#endif

   ForceInline ResourceType GetResourceType(ResourceHandle handle) { return ResourceType(handle & (7 << 28)); }

   ForceInline bool IsValidHandle(ResourceHandle handle) { return (handle & !(7 << 28)) != 0; }

   ForceInline void InitializeRenderer(int32_t threadCount, const void* parameter)
   {
      if (Instance) throw std::runtime_error("Renderer has already been initialized.");
#if defined(_WIN64)
      HWND hwnd = *(const HWND*)parameter;
      Instance = std::make_unique<Graphics::D3D12Renderer>(hwnd, threadCount);
#elif defined(__ANDROID__)
      //RendererInstance = std::make_unique<Pillow::GLES32Renderer>(Hwnd, 2);
#endif
   }
}
