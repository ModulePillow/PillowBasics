// TODO: bundle cmd lists
#if defined(_WIN64)
#include "Renderer.h"
#include <memory>
#include <vector>
#include <comdef.h>
#include <queue>
#include <wrl.h> // import Component Object Model Pointer
#include <d3d12.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <fstream>

using namespace Pillow;
using Microsoft::WRL::ComPtr;

extern double TEMP_GetDeltaTime();
extern double TEMP_GetLastingTime();

// __LINE__ in an inline function doesn't show the line number of the caller, thus choose a macro.
#define CheckHResult(hr)\
{\
   if (FAILED(hr))\
   {\
      string msg;\
      msg = msg + "File:" + __FILE__ + ", Line:" + std::to_string(__LINE__);\
      msg = msg + "\nError: ";\
      std::wstring systemMsg = _com_error(hr).ErrorMessage();\
      utf8::utf16to8(systemMsg.begin(), systemMsg.end(), std::back_inserter(msg));\
      throw std::exception(msg.c_str());\
   }\
}

typedef IDXGIFactory5 IFactory;                  // Has CheckFeatureSupport()
typedef ID3D12Device4 IDevice;                   // Has CreateCommandList1()
typedef ID3D12GraphicsCommandList2 ICommandList; // Has WriteBufferImmediate()
typedef IDXGISwapChain1 ISwapChain;              // Has SetBackgroundColor() 
typedef ID3D12Resource IResource;                // The original one is fine

// An anonymous namespace has internal linkage (accessable in local translation unit)
// Static variables
namespace
{
   const int32_t CBAlignment = D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT;
   const int32_t BCBlockLength = 16; // 4 rows, 4 columns
   const int32_t BC1BlockSize = 8; // C0(2B) C1(2B) Indices(16*2bits = 4B)
   const int32_t BC4BlockSize = 8; // C0(1B) C1(1B) Indices(16*3bits = 6B)
   const int32_t BC3BlockSize = BC1BlockSize + BC4BlockSize;
   const int32_t BC5BlockSize = BC4BlockSize * 2;

   const DXGI_FORMAT NativeTexFmt[int32_t(GenericTexFmt::Count)]
   {
      DXGI_FORMAT_R8G8B8A8_UNORM,
      DXGI_FORMAT_R8G8B8A8_UNORM,
      DXGI_FORMAT_R8G8_SNORM,
      DXGI_FORMAT_R8_UNORM,
   };
   const DXGI_FORMAT NativeBCTexFmt[int32_t(GenericTexFmt::Count)]
   {
      DXGI_FORMAT_BC3_UNORM,
      DXGI_FORMAT_BC1_UNORM,
      DXGI_FORMAT_BC5_UNORM,
      DXGI_FORMAT_BC4_UNORM,
   };
#define DEFAULT_LAYOUT \
0,D3D12_APPEND_ALIGNED_ELEMENT,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0
   const D3D12_INPUT_ELEMENT_DESC _BasicVertex[3]
   {
      { "position", 0, DXGI_FORMAT_R32G32B32_FLOAT, DEFAULT_LAYOUT },
      { "texIdx", 0, DXGI_FORMAT_R8G8_UINT, DEFAULT_LAYOUT },
      { "uv01", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, DEFAULT_LAYOUT }
   };
   const D3D12_INPUT_ELEMENT_DESC _StaticVertex[5]
   {
      { "position", 0, DXGI_FORMAT_R32G32B32_FLOAT, DEFAULT_LAYOUT },
      { "texIdx", 0, DXGI_FORMAT_R8G8_UINT, DEFAULT_LAYOUT },
      { "uv01", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, DEFAULT_LAYOUT },
      { "normal", 0, DXGI_FORMAT_R32G32B32_FLOAT, DEFAULT_LAYOUT },
      { "tangent", 0, DXGI_FORMAT_R32G32B32_FLOAT, DEFAULT_LAYOUT }
   };
   const D3D12_INPUT_ELEMENT_DESC _SkeletalVertex[5]
   {
      { "position", 0, DXGI_FORMAT_R32G32B32_FLOAT, DEFAULT_LAYOUT },
      { "texIdx_boneIdx", 0, DXGI_FORMAT_R8G8B8A8_UINT, DEFAULT_LAYOUT },
      { "uv01", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, DEFAULT_LAYOUT },
      { "normal_boneWeight0", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, DEFAULT_LAYOUT },
      { "tangent_boneWeight1", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, DEFAULT_LAYOUT }
   };
   const D3D12_INPUT_LAYOUT_DESC InputLayoutBasic{ _BasicVertex , 3};
   const D3D12_INPUT_LAYOUT_DESC InputLayoutStatic{ _StaticVertex, 5 };
   const D3D12_INPUT_LAYOUT_DESC InputLayoutSkeletal{ _SkeletalVertex, 5 };

#define TEX_WRAP D3D12_TEXTURE_ADDRESS_MODE_WRAP
#define TEX_CLAMP D3D12_TEXTURE_ADDRESS_MODE_CLAMP
#define SMAPLER_DESC(filter, addressMode, cmpFunc, maxLOD, registerNum) \
{filter, addressMode, addressMode, addressMode, 0, Constants::AnisotropyLevel, cmpFunc, \
D3D12_STATIC_BORDER_COLOR(0), 0, maxLOD, registerNum, 0, D3D12_SHADER_VISIBILITY_ALL}
   const D3D12_STATIC_SAMPLER_DESC StaticSamplers[6]
   {
      SMAPLER_DESC(D3D12_FILTER_MIN_MAG_MIP_POINT, TEX_CLAMP, D3D12_COMPARISON_FUNC(0), 0, 0),         // Point-Clamp (Post-processing)
      SMAPLER_DESC(D3D12_FILTER_MIN_MAG_MIP_LINEAR, TEX_CLAMP, D3D12_COMPARISON_FUNC(0), 99, 1),       // Trilinear-Clamp (Post-processing / UI)
      SMAPLER_DESC(D3D12_FILTER_MIN_MAG_MIP_LINEAR, TEX_WRAP, D3D12_COMPARISON_FUNC(0), 99, 2),        // Trilinear-Wrap (Post-processing / UI)
      SMAPLER_DESC(D3D12_FILTER_ANISOTROPIC, TEX_CLAMP, D3D12_COMPARISON_FUNC(0), 99, 3),              // Anisotropic-Clamp (Mesh)
      SMAPLER_DESC(D3D12_FILTER_ANISOTROPIC, TEX_WRAP, D3D12_COMPARISON_FUNC(0), 99, 4),               // Anisotropic-Wrap (Mesh)
      SMAPLER_DESC(D3D12_FILTER_MIN_MAG_MIP_LINEAR, TEX_CLAMP, D3D12_COMPARISON_FUNC_LESS_EQUAL, 0, 5) // LessEqual-PCF-Comparison (Shadow)
   };

   class FenceSync;
   class DescriptorHeapManager;
   class LateReleaseManager;
   class UnitedBuffer;
   std::unique_ptr<FenceSync> fenceSync;
   std::unique_ptr<DescriptorHeapManager> descriptorMgr;
   std::unique_ptr<LateReleaseManager> lateReleaseMgr;
   ComPtr<IFactory> factory;
   ComPtr<IDevice> device;
   ComPtr<ID3D12CommandQueue> cmdQueue;
   std::vector<ComPtr<ICommandList>> cmdLists;
   std::vector<ID3D12CommandList*> _cmdLists; // A copy of cmdLists, prepared for ExecuteCommandLists()
   std::vector<ComPtr<ID3D12CommandAllocator>> cmdAllocators;
   ComPtr<ISwapChain> swapChain;

   uint16_t tempRTVs[Constants::SwapChainSize] = { 0 }; // Temporary RTVs for swapchain buffers
   ComPtr<IResource> backbuffers[Constants::SwapChainSize]{};

   HWND hwnd;
   int32_t threads;
   bool allowTearing;
   XMINT2 backbufferSize;
   int32_t verticalBlanks{ 1 };
}

// Types
namespace
{
   ForceInline void ApplyBarrier(ComPtr<ICommandList>& cmdList, ComPtr<IResource>& resource, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after);

   // Fence synchronization wrapper
   class FenceSync
   {
      ReadonlyProperty(uint64_t, FrameIndex)

   public:
      FenceSync(ComPtr<IDevice>& device, ComPtr<ID3D12CommandQueue>& commandQueue)
      {
         this->commandQueue = commandQueue;
         syncEventHandle = CreateEventEx(nullptr, L"D3D12Renderer Fence Event", 0, EVENT_ALL_ACCESS);
         if (syncEventHandle == 0) throw std::exception("Failed to create fence sync event handle.");
         CheckHResult(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)));
      }

      ~FenceSync()
      {
         CloseHandle(syncEventHandle);
      }

      uint64_t GetTargetFence() { return _FrameIndex + 1; }
      uint64_t GetCompletedFence() { return fence->GetCompletedValue(); }
      int32_t GetFrameArrayIdx() { return _FrameIndex % Constants::SwapChainSize; }

      // Get the next frame.
      // ***WARNING***
      // Invoke this AFTER ExecuteCommandList() in one frame.
      void NextFrame()
      {
         _FrameIndex++;
         commandQueue->Signal(fence.Get(), _FrameIndex);
         uint64_t minFence = (_FrameIndex < Constants::SwapChainSize) ? 0 : (_FrameIndex - Constants::SwapChainSize + 1);
         Synchronize(minFence);
      }

      // Get all GPU's work done.
      // ***WARNING***
      // Invoke this BEFORE entering worker threads (to avoid resource references existing in uncommitted cmd lists), or AFTER NextFrame() in one frame.
      void FlushQueue()
      {
         uint64_t minFence = _FrameIndex;
         Synchronize(minFence);
      }

   private:
      void Synchronize(uint64_t targetFence)
      {
         // Make sure the GPU arrives at the targetFence.
         if (fence->GetCompletedValue() < targetFence)
         {
            fence->SetEventOnCompletion(targetFence, syncEventHandle);
            WaitForSingleObjectEx(syncEventHandle, INFINITE, true);
         }
      }

   private:
      HANDLE syncEventHandle;
      ComPtr<ID3D12Fence> fence;
      ComPtr<ID3D12CommandQueue> commandQueue;
   };

   class LateReleaseManager
   {
   public:
      LateReleaseManager() {};

      // Enqueue an element that will be released after current frame.
      void Enqueue(std::unique_ptr<UnitedBuffer>&& buffer)
      {
         Item item{ std::move(buffer), fenceSync->GetTargetFence() };
         releaseQueue.push(std::move(item));
      }

      void ReleaseGarbage()
      {
         uint64_t completedFence = fenceSync->GetCompletedFence();
         if (completedFence == 0) return;
         while (!releaseQueue.empty())
         {
            Item& item = releaseQueue.front();
            // FIFO indicates that if one element dequeued is incomplete, so are the remnants.
            if (item.targetFence > completedFence) break;
            item.buffer.reset();
            releaseQueue.pop();
         }
      }

   private:
      struct Item
      {
         std::unique_ptr<UnitedBuffer> buffer;
         uint64_t targetFence;
      };

      std::queue<Item> releaseQueue;
   };

   enum class ViewType : uint8_t
   {
      // Stored in srvUavDescHeap.
      CBV,
      SRV,
      UAV,
      // Stored in rtvDescHeap.
      RTV,
      // stored in dsvDescHeap.
      DSV
   };

   class DescriptorHeapManager
   {
   public:
      DescriptorHeapManager(ComPtr<IDevice>& device) :
         csuSize(device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV)),
         rtvSize(device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV)),
         dsvSize(device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV))
      {
         csuFreePool.reserve(MaxCsuCount);
         rtvFreePool.reserve(MaxRtvCount);
         dsvFreePool.reserve(MaxDsvCount);
         for (uint16_t i = MaxCsuCount; i > 0; i--)
         {
            csuFreePool.push_back(i | uint16_t(InnerFlag::CSU) << 14);
            if (i <= MaxRtvCount) rtvFreePool.push_back(i | uint16_t(InnerFlag::RTV) << 14);
            if (i <= MaxDsvCount) dsvFreePool.push_back(i | uint16_t(InnerFlag::DSV) << 14);
         }

         D3D12_DESCRIPTOR_HEAP_DESC descHeapDesc
         {
            D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
            (UINT)MaxCsuCount,
            D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE
         };
         CheckHResult(device->CreateDescriptorHeap(&descHeapDesc, IID_PPV_ARGS(&csuDescHeap)));
         descHeapDesc =
         {
            D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
            (UINT)MaxRtvCount,
         };
         CheckHResult(device->CreateDescriptorHeap(&descHeapDesc, IID_PPV_ARGS(&rtvDescHeap)));
         descHeapDesc =
         {
            D3D12_DESCRIPTOR_HEAP_TYPE_DSV,
            (UINT)MaxDsvCount
         };
         CheckHResult(device->CreateDescriptorHeap(&descHeapDesc, IID_PPV_ARGS(&dsvDescHeap)));
         csuCpuHandle0 = csuDescHeap->GetCPUDescriptorHandleForHeapStart();
         csuGpuHandle0 = csuDescHeap->GetGPUDescriptorHandleForHeapStart();
         rtvCpuHandle0 = rtvDescHeap->GetCPUDescriptorHandleForHeapStart();
         dsvCpuHandle0 = dsvDescHeap->GetCPUDescriptorHandleForHeapStart();
      }

      ForceInline void BindSrvHeap(ComPtr<ICommandList>& cmd)
      {
         cmd->SetDescriptorHeaps(1, csuDescHeap.GetAddressOf());
      }

      ForceInline D3D12_CPU_DESCRIPTOR_HANDLE GetCPUHandle(uint16_t handle)
      {
         D3D12_CPU_DESCRIPTOR_HANDLE result{};
         auto flag = GetInnerFlag(handle);
         handle &= 0x3FFF; // Clear the flag bits
         switch (flag)
         {
         case InnerFlag::CSU:
            result.ptr = csuCpuHandle0.ptr + csuSize * handle;
            break;
         case InnerFlag::RTV:
            result.ptr = rtvCpuHandle0.ptr + rtvSize * handle;
            break;
         case InnerFlag::DSV:
            result.ptr = dsvCpuHandle0.ptr + dsvSize * handle;
            break;
         }
         return result;
      }

      ForceInline D3D12_GPU_DESCRIPTOR_HANDLE GetGPUHandle(uint16_t handle)
      {
         D3D12_GPU_DESCRIPTOR_HANDLE result{};
         auto flag = GetInnerFlag(handle);
         handle = RemoveFlag(handle);
         switch (flag)
         {
         case InnerFlag::CSU:
            result.ptr = csuGpuHandle0.ptr + csuSize * handle;
            break;
         default:
            throw std::exception("GPU handle is not supported for RTV and DSV.");
         }
         return result;
      }

      uint16_t CreateView(ComPtr<IDevice>& device, ComPtr<IResource>& res, void* viewDesc, ViewType type)
      {
         uint16_t handle{};
         auto GetHandle = [&](std::vector<uint16_t>& freePool, const char* name)
            {
               if (freePool.empty())
                  throw std::exception((std::string(name) + ": This descriptor heap is full.").c_str());
               handle = freePool.back();
               freePool.pop_back();
            };
         switch (type)
         {
         case ViewType::CBV:
            GetHandle(csuFreePool, "CSV_SRV_UAV");
            device->CreateConstantBufferView((D3D12_CONSTANT_BUFFER_VIEW_DESC*)viewDesc, GetCPUHandle(handle));
            break;
         case ViewType::SRV:
            GetHandle(csuFreePool, "CSV_SRV_UAV");
            device->CreateShaderResourceView(res.Get(), (D3D12_SHADER_RESOURCE_VIEW_DESC*)viewDesc, GetCPUHandle(handle));
            break;
         case ViewType::UAV:
            GetHandle(csuFreePool, "CSV_SRV_UAV");
            device->CreateUnorderedAccessView(res.Get(), nullptr, (D3D12_UNORDERED_ACCESS_VIEW_DESC*)viewDesc, GetCPUHandle(handle));
            break;
         case ViewType::RTV:
            GetHandle(rtvFreePool, "RTV");
            device->CreateRenderTargetView(res.Get(), (D3D12_RENDER_TARGET_VIEW_DESC*)viewDesc, GetCPUHandle(handle));
            break;
         case ViewType::DSV:
            GetHandle(dsvFreePool, "DSV");
            device->CreateDepthStencilView(res.Get(), (D3D12_DEPTH_STENCIL_VIEW_DESC*)viewDesc, GetCPUHandle(handle));
         }
#ifdef PILLOW_DEBUG
         //LogSystem(L"ViewHandle=" + std::to_wstring(handle) + L" Index=" + std::to_wstring(RemoveFlag(handle)));
#endif
         return handle;
      }

      void ReleaseView(uint16_t handle)
      {
         auto ReleaseHandle = [&handle](std::vector<uint16_t>& freePool)
            {
#ifdef PILLOW_DEBUG
               bool found = std::find(freePool.begin(), freePool.end(), handle) != freePool.end();
               if (found) throw std::exception("Invalid index.");
#endif
               freePool.push_back(handle);
            };
         auto flag = GetInnerFlag(handle);
         switch (flag)
         {
         case InnerFlag::CSU:
            ReleaseHandle(csuFreePool);
            break;
         case InnerFlag::RTV:
            ReleaseHandle(rtvFreePool);
            break;
         case InnerFlag::DSV:
            ReleaseHandle(dsvFreePool);
            break;
         }
      }

   private:
      enum struct InnerFlag : uint32_t
      {
         CSU = 0,
         RTV = 1,
         DSV = 2
      };

      ForceInline InnerFlag GetInnerFlag(uint16_t handle)
      {
         return InnerFlag(handle >> 14);
      }

      ForceInline uint16_t RemoveFlag(uint16_t handle)
      {
         return handle & 0x3FFF; // Clear the flag bits
      }

   private:
      const uint32_t FlagBits = 2;
      const uint32_t HandleMaxNum = (1 << (16 - FlagBits)); // value=16384

      const int32_t MaxCsuCount = 4096;
      const int32_t MaxRtvCount = 64;
      const int32_t MaxDsvCount = 16;

      const int32_t csuSize, rtvSize, dsvSize;
      ComPtr<ID3D12DescriptorHeap> csuDescHeap;
      ComPtr<ID3D12DescriptorHeap> rtvDescHeap;
      ComPtr<ID3D12DescriptorHeap> dsvDescHeap;
      std::vector<uint16_t> csuFreePool;
      std::vector<uint16_t> rtvFreePool;
      std::vector<uint16_t> dsvFreePool;
      D3D12_CPU_DESCRIPTOR_HANDLE csuCpuHandle0;
      D3D12_GPU_DESCRIPTOR_HANDLE csuGpuHandle0;
      // RTV and DSV don't have gpu handles.
      D3D12_CPU_DESCRIPTOR_HANDLE rtvCpuHandle0;
      D3D12_CPU_DESCRIPTOR_HANDLE dsvCpuHandle0;
   };

   // A superior wrapper for D3D12 resources of all types.
   class UnitedBuffer
   {
      DeleteDefautedMethods(UnitedBuffer)

   public:
      // Use none-scoped enumerations for convenience.
      enum HeapType : uint8_t
      {
         Upload = D3D12_HEAP_TYPE_UPLOAD,
         TextureUpload = D3D12_HEAP_TYPE_CUSTOM,
         Readback = D3D12_HEAP_TYPE_READBACK,
         Default = D3D12_HEAP_TYPE_DEFAULT
      };

      enum DataType : uint8_t
      {
         Texture,
         ConstBuffer,
         VertexOrIdxBuffer
      };

      // For texture arrays, create only a minimal number of mid buffers for uploading, which saves a lot of memory.
      static const int32_t MaxMidPoolSize = 4;

      const HeapType _HeapType;
      const DataType _DataType;
      const GenericTextureInfo TexInfo;
      const int32_t ElementCount;
      const int32_t RawElementSize;
      const int32_t AlignedElementSize;
      const int32_t TotalSize;
      const bool KeepMidPool;

      UnitedBuffer(HeapType heapType, DataType dataType, int32_t _rawElementSize, int32_t count, bool keepMiddlePool = false):
         UnitedBuffer(heapType, dataType, _rawElementSize, count, keepMiddlePool, GenericTextureInfo{})
      {
         bool wrongUseCheck = dataType == Texture;
         if (wrongUseCheck) throw std::runtime_error("Wrong constructor usage.");
      }

      UnitedBuffer(HeapType heapType, DataType dataType, const GenericTextureInfo& texInfo, bool keepMiddlePool = false):
         UnitedBuffer(heapType, dataType, 0, 0, keepMiddlePool, texInfo)
      {
         bool wrongUseCheck = dataType != Texture;
         wrongUseCheck |= dataType == Texture && (heapType == Upload || heapType == TextureUpload);
         if (wrongUseCheck) throw std::runtime_error("Wrong constructor usage.");
      }

      ~UnitedBuffer() = default;

      uint64_t GetGPUAddress(int index = 0) { return pointerGPU + index * RawElementSize; };

      // The destination data should align with 64 bytes(the cache line size).
      void ReadBack(std::unique_ptr<CacheLine[]>& destination, int32_t destinationSize = 0)
      {
         if (_HeapType != HeapType::Readback) throw std::exception("Cannot use ReadBack() with non-readback buffers.");
         if (!destination) destination = CreateAlignedMemory(TotalSize);
         else if (destinationSize < TotalSize) throw std::exception("Destination buffer is too small.");
         if (_DataType == DataType::Texture)
         {
            int32_t rowPitch = TexInfo.GetWidth() * TexInfo.GetPixelSize();
            int32_t depthPitch = TexInfo.GetMipZeroSize();
            heap->ReadFromSubresource(destination.get(), rowPitch, depthPitch, 0, nullptr);
         }
         else memcpy(destination.get(), pointerCPU, TotalSize);
      }

      void WriteNumericData(const uint8_t* rawData, int indexOffset = 0, int _elementCount = 1)
      {
         if (_DataType == DataType::Texture) throw new std::runtime_error("Cannot use WriteNumericData() with textures.");
         if (indexOffset + _elementCount > ElementCount) throw std::exception("Out of Range");
         if (_HeapType == HeapType::Default)
         {
            RegisterGPUCopy();
            middlePool[0]->WriteNumericData(rawData, indexOffset, _elementCount);
            return;
         }
         // Write to the middle buffer
         if (_DataType == DataType::VertexOrIdxBuffer)
         {
            memcpy(pointerCPU + indexOffset * RawElementSize, rawData, _elementCount * RawElementSize);
         }
         else if (_DataType == DataType::ConstBuffer)
         {
            int32_t alignedSize = GetAlignedSize(RawElementSize, CBAlignment);
            for (int32_t i = 0; i < _elementCount; i++)
            {
               int32_t destOffset = (indexOffset + i) * alignedSize;
               int32_t srcOffset = (indexOffset + i) * RawElementSize;
               memcpy(pointerCPU + destOffset, rawData + srcOffset, RawElementSize);
            }
         }
      }

      // 1.D3D12 texture subresource indexing: SubRes[PlaneIdx][ArrayIdx][MipIdx]
      // Normally, planar formats are not used to store RGBA data.
      // 
      // 2.ABOUT THE FOOTPRINT: In Direct3D 12 terminology, footprint describes the memory layouts of D3D12 resources.
      // In detail, the size of a texture row should be aligned(D3D12_TEXTURE_DATA_PITCH_ALIGNMENT), which makes memory allocation more sophisticated.
      // But there are ways to avoid touching the footprints.
      // For instance, we can use ID3D12Resource::WriteToSubresource to copy unaligned data into a custom upload heap,
      // then use ID3DCommandList::CopyTextureRegion to copy it into a default buffer while ignoring the footprints.
      void WriteTexture(const uint8_t* rawTexture, const GenericTextureInfo& texInfo, int32_t arrayIndex = 0)
      {
         if (_DataType != DataType::Texture) throw std::runtime_error("Cannot use WriteTexture() with numeric data.");
         if(middleTargets.size() == MaxMidPoolSize)  throw std::runtime_error("The middle pool is exhausted.");
         if (_HeapType == HeapType::Default)
         {
            RegisterGPUCopy();
            if (std::find(middleTargets.begin(), middleTargets.end(), arrayIndex) != middleTargets.end())
               throw std::runtime_error("Write to a same texture twice in one frame.");
            middlePool[middleTargets.size()]->WriteTexture(rawTexture, texInfo, arrayIndex);
            middleTargets.push_back(arrayIndex);
            return;
         }
         // Write to the middle buffer
         for (int32_t mip = 0; mip < texInfo.GetMipCount(); mip++)
         {
            int32_t width = texInfo.GetWidth() >> mip;
            int32_t rowPitch = width * texInfo.GetPixelSize();
            int32_t depthPitch = width * rowPitch;
            heap->WriteToSubresource(arrayIndex * texInfo.GetMipCount() + mip, nullptr, rawTexture, rowPitch, depthPitch);
            rawTexture += depthPitch;
         }
      }

      static void GPUCopy(ComPtr<ICommandList>& cmdList)
      {
         if (DirtyPool.empty()) return;
         while (!DirtyPool.empty())
         {
            UnitedBuffer& buffer = *DirtyPool.back();
            DirtyPool.pop_back();
            if (buffer.middlePool.size() == 1)
            {
               ApplyBarrier(cmdList, buffer.middlePool[0]->heap, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_COPY_SOURCE);
               ApplyBarrier(cmdList, buffer.heap, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_COPY_DEST);
               cmdList->CopyResource(buffer.heap.Get(), buffer.middlePool[0]->heap.Get()); // GPU Copy
               ApplyBarrier(cmdList, buffer.middlePool[0]->heap, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_GENERIC_READ);
               ApplyBarrier(cmdList, buffer.heap, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_GENERIC_READ);
            }
            else // Texture array
            {
               while (buffer.middleTargets.size())
               {
                  // Preparation
                  int32_t target = buffer.middleTargets.back();
                  buffer.middleTargets.pop_back();
                  int32_t midIdx = buffer.middleTargets.size();
                  D3D12_TEXTURE_COPY_LOCATION src{ buffer.middlePool[midIdx]->heap.Get(), D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX, 0 };
                  D3D12_TEXTURE_COPY_LOCATION dst{ buffer.heap.Get(), D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX, 0 };
                  // GPU copy
                  ApplyBarrier(cmdList, buffer.middlePool[midIdx]->heap, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_COPY_SOURCE);
                  ApplyBarrier(cmdList, buffer.heap, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_COPY_DEST);
                  for (int mip = 0; mip < buffer.TexInfo.GetMipCount(); mip++)
                  {
                     src.SubresourceIndex = mip;
                     dst.SubresourceIndex = target * buffer.TexInfo.GetMipCount() + mip;
                     cmdList->CopyTextureRegion(&dst, 0, 0, 0, &src, NULL);
                  }
                  ApplyBarrier(cmdList, buffer.middlePool[midIdx]->heap, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_GENERIC_READ);
                  ApplyBarrier(cmdList, buffer.heap, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_GENERIC_READ);
               }
            }

         }
      }

   private:
      const D3D12_RESOURCE_DESC DefaultResDesc
      {
         D3D12_RESOURCE_DIMENSION_BUFFER, 0, uint64_t(TotalSize), 1, 1, 1, DXGI_FORMAT_UNKNOWN,
         DXGI_SAMPLE_DESC{1, 0}, D3D12_TEXTURE_LAYOUT_ROW_MAJOR, D3D12_RESOURCE_FLAG_NONE
      };

      inline static std::vector<UnitedBuffer*> DirtyPool{};
      std::vector<std::unique_ptr<UnitedBuffer>> middlePool;
      std::vector<int8_t> middleTargets;
      ComPtr<IResource> heap{};
      uint64_t pointerGPU{};
      uint8_t* pointerCPU{};

      UnitedBuffer(HeapType heapType, DataType dataType, int32_t _rawElementSize, int32_t count, bool keepMiddlePool, const GenericTextureInfo& texInfo) :
         _HeapType(heapType),
         _DataType(dataType),
         TexInfo(texInfo),
         ElementCount(count),
         RawElementSize(_rawElementSize),
         AlignedElementSize(GetAlignedSize(_rawElementSize, dataType == ConstBuffer ? CBAlignment : 1)),
         TotalSize(GetAlignedSize(_rawElementSize, dataType == ConstBuffer ? CBAlignment : 1)* count),
         KeepMidPool(keepMiddlePool)
      {
         bool isUpload = heapType == Upload || heapType == TextureUpload;
         bool isRdBack = heapType == Readback;
         if (isRdBack && dataType == Texture && texInfo.GetMipCount() != 1)
            throw std::runtime_error("Texture readback buffers don't support mipmaps. It's a restriction of the Pillow Basics design.");

         // Write-combining disables the CPU cache and enables the write-combining buffer. It's suitable for CPU-write-only actions.
         auto pageType = isUpload ? D3D12_CPU_PAGE_PROPERTY_WRITE_COMBINE : (isRdBack ? D3D12_CPU_PAGE_PROPERTY_WRITE_BACK : D3D12_CPU_PAGE_PROPERTY_NOT_AVAILABLE);
         // Level 0 memory pool = CPU main memory
         auto memPool = (isRdBack || isUpload) ? D3D12_MEMORY_POOL_L0 : D3D12_MEMORY_POOL_L1;
         D3D12_HEAP_PROPERTIES heapProperties{ D3D12_HEAP_TYPE(heapType), pageType, memPool };
         D3D12_RESOURCE_DESC resourceDesc = DefaultResDesc;
         if (dataType == Texture)
         {
            int32_t fmt = int32_t(texInfo.GetFormat());
            resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
            resourceDesc.Width = texInfo.GetWidth();
            resourceDesc.Height = texInfo.GetWidth();
            resourceDesc.DepthOrArraySize = uint16_t(texInfo.GetArrayCount());
            resourceDesc.MipLevels = uint16_t(texInfo.GetMipCount());
            resourceDesc.Format = texInfo.GetCompressionMode() == CompressionMode::None ? NativeTexFmt[fmt] : NativeBCTexFmt[fmt];
            resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
         }
         auto flags = D3D12_HEAP_FLAG_NONE;
         auto state = heapType == Readback ? D3D12_RESOURCE_STATE_COPY_DEST : D3D12_RESOURCE_STATE_GENERIC_READ;
         CheckHResult(device->CreateCommittedResource(&heapProperties, flags, &resourceDesc, state, nullptr, IID_PPV_ARGS(&heap)));
         GetCPUGPUPointers();
         CreateMiddleBuffers();
      }

      void CreateMiddleBuffers()
      {
         if (_HeapType != Default) return;
         if (!middlePool.empty()) throw std::runtime_error("The middle buffer has been created.");
         HeapType heapType = _DataType == Texture ? TextureUpload : Upload;
         int32_t count = _DataType == Texture ? MaxMidPoolSize : 1;
         middlePool.reserve(count);
         middleTargets.reserve(count);
         for (int i = 0; i < count; i++)
         {
            auto ptr = std::unique_ptr<UnitedBuffer>(new UnitedBuffer(heapType, _DataType, ElementCount, RawElementSize, KeepMidPool, TexInfo));
            middlePool.push_back(std::move(ptr));
         }
      }

      void GetCPUGPUPointers()
      {
         if (_HeapType != Default)
         {
            D3D12_RANGE range{ 0, 0 };
            // CPU read is only needed by readback buffers.
            CheckHResult(heap->Map(0, _HeapType == Readback ? nullptr : &range, (void**)(&pointerCPU)));
         }
         if (_DataType != Texture)
         {
            pointerGPU = heap->GetGPUVirtualAddress();
         }
      }

      void RegisterGPUCopy()
      {
         if (middlePool.empty()) throw std::runtime_error("The middle buffer of current default buffer died.");
         DirtyPool.push_back(this);
         // Release the mid pool.
         if (KeepMidPool) return;
         while (middlePool.size())
         {
            lateReleaseMgr->Enqueue(std::move(middlePool.back()));
            middlePool.pop_back();
         }
      }
   };

   class HLSLInclude : public ID3DInclude
   {
      ReadonlyProperty(std::filesystem::path, ParentDir)

   public:
      HLSLInclude(std::filesystem::path location)
      {
         _ParentDir = location.parent_path();
      }

      HRESULT Open(D3D_INCLUDE_TYPE IncludeType, LPCSTR pFileName, LPCVOID pParentData, LPCVOID* ppData, UINT* pBytes)
      {
         std::filesystem::path location = GetResourcePath("Shaders");
         location = location / pFileName;
         // If the root dir doesn't own the file, use the local dir.
         // Ignore D3D_INCLUDE_TYPE, which makes things complicated.
         if (!std::filesystem::exists(location))
         {
            location = _ParentDir;
            location /= pFileName;
            if (!std::filesystem::exists(location)) return E_FAIL;
         }
         std::ifstream file(location, std::ios::binary | std::ios::ate);
         if (!file.is_open()) return E_FAIL;
         uint32_t size = uint32_t(file.tellg());
         file.seekg(0, std::ios::beg);
         std::vector<char> buffer;
         buffer.reserve(size);
         if (!file.read(buffer.data(), size)) return E_FAIL;
         file.close();
         *ppData = buffer.data();
         *pBytes = size;
         buffers.push_back(std::move(buffer));
         return S_OK;
      }

      HRESULT Close(LPCVOID pData)
      {
         for (std::vector<char>& a : buffers)
         {
            if (a.data() != pData) continue;
            buffers.clear();
            break;
         }
         return S_OK;
      }

   private:
      std::vector<std::vector<char>> buffers;
   };

   class PipelineStateManager
   {

   };
}

// Static functions
namespace
{
   ForceInline void PlaceBCIndex(uint8_t* destination, uint32_t value, uint32_t pixelIndex)
   {
      uint32_t bitCount = pixelIndex * 3;
      uint32_t byteOffset = bitCount / 8;
      uint32_t bitOffset = bitCount % 8;
      destination[byteOffset] = (destination[byteOffset] & ~(0x7 << bitOffset)) | value << bitOffset;
   }

   void XM_CALLCONV OptimizeRGB(XMVECTOR& color0, XMVECTOR& color1, const XMVECTOR* block)
   {
      const uint32_t steps = 4;
      constexpr float fEpsilon = (0.25f / 64.f) * (0.25f / 64.f);
      static constexpr float pC[] = { 1, 2.f / 3.f, 1.f / 3.f, 0 };
      static constexpr float pD[] = { pC[3], pC[2], pC[1], pC[0] };
      // Find Min and Max points, as starting point
      XMVECTOR c0 = RGBLuminance;
      XMVECTOR c1 = XMVectorZero();
      for (int32_t i = 0; i < BCBlockLength; i++)
      {
         XMVECTOR select = XMVectorLess(block[i], c0);
         c0 = XMVectorSelect(c0, block[i], select);
         select = XMVectorGreater(block[i], c1);
         c1 = XMVectorSelect(c1, block[i], select);
      }
      // Diagonal axis
      const XMVECTOR AB = XMVectorSubtract(c1, c0);
      const float fAB = XMVectorGetX(XMVector3Dot(AB, AB));
      // Single color block.. no need to root-find
      if (fAB < FLT_MIN)
      {
         color0 = c0;
         color1 = c1;
         return;
      }
      // Try all four axis directions, to determine which diagonal best fits data
      XMVECTOR dir = XMVectorScale(AB, 1.f / fAB);
      const XMVECTOR Mid = XMVectorLerp(c0, c1, 0.5f);
      XMVECTOR fDir = XMVectorZero();
      for (int32_t i = 0; i < BCBlockLength; i++)
      {
         XMVECTOR pt = XMVectorMultiply(XMVectorSubtract(block[i], Mid), dir);
         //XMVectorSetW(pt, 0);
         XMFLOAT3A _pt;
         XMStoreFloat3A(&_pt, pt);
         XMVECTOR f = XMVectorReplicate(_pt.x);
         f = XMVectorAdd(f, XMVectorSet(_pt.y, _pt.y, -_pt.y, -_pt.y));
         f = XMVectorAdd(f, XMVectorSet(_pt.z, -_pt.z, _pt.z, -_pt.z));
         fDir = XMVectorMultiply(f, f);
      }     
      XMFLOAT4A _fDir {};
      XMStoreFloat4A(&_fDir, fDir);
      float fDirMax = _fDir[0];
      int32_t  iDirMax = 0;
      for (size_t i = 1; i < 4; i++)
      {
         if (_fDir[i] <= fDirMax) continue;
         fDirMax = _fDir[i];
         iDirMax = i;
      }
      if (iDirMax & 2)
      {
         const XMVECTOR select = XMVectorSelectControl(0, 1, 0, 0);
         XMVECTOR temp = c0;
         c0 = XMVectorSelect(c0, c1, select);
         c1 = XMVectorSelect(c1, temp, select);
      }
      if (iDirMax & 1)
      {
         const XMVECTOR select = XMVectorSelectControl(0, 0, 1, 0);
         XMVECTOR temp = c0;
         c0 = XMVectorSelect(c0, c1, select);
         c1 = XMVectorSelect(c1, temp, select);
      }
      // Two color block.. no need to root-find
      if (fAB < 1.f / 4096.f)
      {
         color0 = c0;
         color1 = c1;
         return;
      }
      // Use Newton's Method to find local minima of sum-of-squares error.
      const float fSteps = steps - 1;
      for (int32_t i = 0; i < 8; i++)
      {
         // Calculate new steps
         XMVECTOR pSteps[4];
         for (size_t iStep = 0; iStep < steps; iStep++)
         {
            pSteps[iStep] = XMVectorAdd(XMVectorScale(c0, pC[iStep]), XMVectorScale(c1, pD[iStep]));
         }
         // Calculate color direction
         dir = XMVectorSubtract(c1, c0);
         const float fLen = XMVectorGetX(XMVector3Dot(dir, dir));
         if (fLen < (1.0f / 4096.0f)) break;
         dir = XMVectorScale(dir, fSteps / fLen);
         // Evaluate function, and derivatives
         float d2X = 0;
         float d2Y = 0;
         XMVECTOR dX = XMVectorZero();
         XMVECTOR dY = XMVectorZero();
         for (int32_t i = 0; i < BCBlockLength; i++)
         {
            const float fDot = XMVectorGetX(XMVector3Dot(XMVectorSubtract(block[i], c0), dir));
            uint32_t iStep;
            if (fDot <= 0) iStep = 0;
            else if (fDot >= fSteps) iStep = steps - 1;
            else iStep = fDot + 0.5f;
            XMVECTOR diff = XMVectorSubtract(pSteps[iStep], block[i]);
            const float fC = pC[iStep] * (1.f / 8.f);
            const float fD = pD[iStep] * (1.f / 8.f);
            d2X += fC * pC[iStep];
            dX = XMVectorAdd(dX, XMVectorScale(diff, fC));
            d2Y += fD * pD[iStep];
            dY = XMVectorAdd(dY, XMVectorScale(diff, fD));
         }
         // Move endpoints
         if (d2X > 0) c0 = XMVectorAdd(c0, XMVectorScale(dX, -1 / d2X));
         if (d2Y > 0) c1 = XMVectorAdd(c1, XMVectorScale(dY, -1 / d2Y));
         XMVECTOR cmp1 = XMVectorLess(XMVectorMultiply(dX, dX), XMVectorReplicate(fEpsilon));
         XMVECTOR cmp2 = XMVectorLess(XMVectorMultiply(dY, dY), XMVectorReplicate(fEpsilon));
         XMVECTOR cmp = XMVectorAndInt(cmp1, cmp2);
         cmp = XMVectorAndInt(XMVectorAndInt(cmp, XMVectorSplatY(cmp)), XMVectorSplatZ(cmp));
         if (XMVectorGetIntX(cmp)) break;
      }
      color0 = c0;
      color1 = c1;
   }

   void OptimizeAlpha(float& colorMin, float& colorMax, const float* block, uint32_t steps)
   {
      static constexpr float pC6[] = { 1, 4.f / 5.f, 3.f / 5.f, 2.f / 5.f, 1.f / 5.f, 0 };
      static constexpr float pD6[] = { pC6[5], pC6[4], pC6[3], pC6[2], pC6[1], pC6[0] };
      static constexpr float pC8[] = { 1, 6.f / 7.f, 5.f / 7.f, 4.f / 7.f, 3.f / 7.f, 2.f / 7.f, 1.f / 7.f, 0 };
      static constexpr float pD8[] = { pC8[7], pC8[6], pC8[5], pC8[4], pC8[3], pC8[2], pC8[1], pC8[0] };
      const float* pC = (6 == steps) ? pC6 : pC8;
      const float* pD = (6 == steps) ? pD6 : pD8;
      // Find Min and Max points, as starting point
      float _min = 1;
      float _max = 0;
      for (size_t i = 0; i < BCBlockLength; i++)
      {
         if (block[i] < _min) _min = block[i];
         if (block[i] > _max) _max = block[i];
      }
      if (steps == 6 && _min == _max) _max = 1;
      // Use Newton's Method to find local minima of sum-of-squares error.
      const float fSteps = steps - 1;
      for (size_t i = 0; i < 8; i++)
      {
         if ((_max - _min) < (1.0f / 256.0f)) break;
         float const fScale = fSteps / (_max - _min);
         // Calculate new steps
         float pSteps[8];
         for (size_t iStep = 0; iStep < steps; iStep++)
            pSteps[iStep] = pC[iStep] * _min + pD[iStep] * _max;
         if (steps == 6)
         {
            pSteps[6] = 0;
            pSteps[7] = 1;
         }
         // Evaluate function, and derivatives
         float dX = 0.0f;
         float dY = 0.0f;
         float d2X = 0.0f;
         float d2Y = 0.0f;
         for (int32_t iPoint = 0; iPoint < BCBlockLength; iPoint++)
         {
            const float fDot = (block[iPoint] - _min) * fScale;
            uint32_t iStep;
            if (fDot == 0.0f)
            {
               iStep = (steps == 6 && block[iPoint] <= _min * 0.5f) ? 6u : 0u;
            }
            else if (fDot >= fSteps)
            {
               iStep = (steps == 6 && block[iPoint] >= (_max + 1) * 0.5f) ? 7u : (steps - 1);
            }
            else
            {
               iStep = fDot + 0.5f;
            }
            if (iStep < steps)
            {
               // D3DX had this computation backwards (pPoints[iPoint] - pSteps[iStep])
               // this fix improves RMS of the alpha component
               const float fDiff = pSteps[iStep] - block[iPoint];
               dX += pC[iStep] * fDiff;
               d2X += pC[iStep] * pC[iStep];
               dY += pD[iStep] * fDiff;
               d2Y += pD[iStep] * pD[iStep];
            }
         }
         // Move endpoints
         if (d2X > 0.0f) _min -= dX / d2X;
         if (d2Y > 0.0f) _max -= dY / d2Y;
         if (_min > _max) std::swap(_min, _max);
         if (dX * dX < 1.f / 64.f && dY * dY < 1.f / 64.f) break;
      }
      colorMin = std::clamp(_min, 0.f, 1.f);
      colorMax = std::clamp(_max, 0.f, 1.f);
   }

   void EncodeBC1RGB(const XMFLOAT4A* blockRGB, uint8_t* destination, bool RGBDithering)
   {
      const uint32_t uSteps = 4;
      // Quantize block to R56B5, using Floyd Stienberg error diffusion. This
      // increases the chance that colors will map directly to the quantized
      // axis endpoints.
      XMVECTOR colors[BCBlockLength];
      XMVECTOR errors[BCBlockLength];
      if (RGBDithering) for (int32_t i = 0; i < BCBlockLength; i++) errors[i] = XMVectorZero();
      for (int32_t i = 0; i < BCBlockLength; i++)
      {
         XMVECTOR c = XMLoadFloat4A(&blockRGB[i]);
         if (RGBDithering) c = XMVectorAdd(c, errors[i]);
         const XMVECTOR v2 = XMVectorSet(31.f, 63.f, 31.f, 0);
         const XMVECTOR v3 = XMVectorReplicate(0.5f);
         const XMVECTOR factor = XMVectorSet(1 / 31.f, 1 / 63.f, 1 / 31.f, 0);
         colors[i] = XMVectorMultiply(XMVectorFloor(XMVectorMultiplyAdd(c, v2, v3)), factor);
         colors[i] = XMVectorMultiply(colors[i], RGBLuminance);
         if (!RGBDithering) continue;
         XMVECTOR diff = XMVectorSubtract(c, colors[i]);
         if (3 != (i & 3))
         {
            const XMVECTOR factor = XMVectorReplicate(7.f / 16.f);
            errors[i + 1] = XMVectorMultiplyAdd(diff, factor, errors[i + 1]);
         }
         if (i < 12)
         {
            const XMVECTOR factor = XMVectorReplicate(5.f / 16.f);
            errors[i + 4] = XMVectorMultiplyAdd(diff, factor, errors[i + 4]);
            if (i & 3)
            {
               const XMVECTOR factor = XMVectorReplicate(3.f / 16.f);
               errors[i + 3] = XMVectorMultiplyAdd(diff, factor, errors[i + 3]);
            }
            if (3 != (i & 3))
            {
               const XMVECTOR factor = XMVectorReplicate(1 / 16.f);
               errors[i + 5] = XMVectorMultiplyAdd(diff, factor, errors[i + 5]);
            }
         }
      }
      // Perform 6D root finding function to find two endpoints of color axis.
      // Then quantize and sort the endpoints depending on mode.
      XMVECTOR ColorA, ColorB, ColorC, ColorD;
      OptimizeRGB(ColorA, ColorB, colors);
      ColorC = XMVectorMultiply(ColorA, RGBLuminanceInv);
      ColorD = XMVectorMultiply(ColorB, RGBLuminanceInv);
      const uint16_t wColorA = EncodeRGB565(ColorC);
      const uint16_t wColorB = EncodeRGB565(ColorD);
      if (wColorA == wColorB)
      {
         reinterpret_cast<uint16_t*>(destination)[0] = wColorA;
         reinterpret_cast<uint16_t*>(destination)[1] = wColorA;
         reinterpret_cast<uint32_t*>(destination)[1] = 0x0;
         return;
      }
      ColorC = DecodeRGB565(wColorA);
      ColorD = DecodeRGB565(wColorB);
      ColorA = XMVectorMultiply(ColorC, RGBLuminance);
      ColorB = XMVectorMultiply(ColorD, RGBLuminance);
      // Calculate color steps
      XMVECTOR Step[4];
      reinterpret_cast<uint16_t*>(destination)[0] = wColorB;
      reinterpret_cast<uint16_t*>(destination)[1] = wColorA;
      Step[0] = ColorB;
      Step[1] = ColorA;
      static const int32_t pSteps[] = { 0, 2, 3, 1 };
      Step[2] = XMVectorLerp(Step[0], Step[1], 1 / 3.f);
      Step[3] = XMVectorLerp(Step[0], Step[1], 2 / 3.f);
      // Calculate color direction
      XMVECTOR Dir;
      Dir = Step[1] - Step[0];
      const float fSteps = uSteps - 1;
      const float fScale = (wColorA != wColorB) ? (fSteps / XMVectorGetX(XMVector3Dot(Dir, Dir))) : 0;
      Dir = XMVectorScale(Dir, fScale);
      // Encode colors, 2 bits per pixel
      uint32_t encodedIndices = 0;
      if (RGBDithering) for (int32_t i = 0; i < BCBlockLength; i++) errors[i] = XMVectorZero();
      for (int32_t i = 0; i < BCBlockLength; i++)
      {
         XMVECTOR c = XMLoadFloat4A(&blockRGB[i]);
         c = XMVectorMultiply(c, RGBLuminance);
         if (RGBDithering) c = XMVectorAdd(c, errors[i]);
         const float fDot = XMVectorGetX(XMVector3Dot(XMVectorSubtract(c, Step[0]), Dir));
         uint32_t iStep;
         if (fDot <= 0.0f) iStep = 0;
         else if (fDot >= fSteps) iStep = 1;
         else iStep = pSteps[uint32_t(fDot + 0.5f)];
         encodedIndices = (iStep << 30) | (encodedIndices >> 2);
         if (!RGBDithering) continue;
         XMVECTOR diff = XMVectorSubtract(c, Step[iStep]);
         if (3 != (i & 3))
         {
            const XMVECTOR factor = XMVectorReplicate(7.f / 16.f);
            errors[i + 1] = XMVectorMultiplyAdd(diff, factor, errors[i + 1]);
         }
         if (i < 12)
         {
            const XMVECTOR factor = XMVectorReplicate(5.f / 16.f);
            errors[i + 4] = XMVectorMultiplyAdd(diff, factor, errors[i + 4]);
            if (i & 3)
            {
               const XMVECTOR factor = XMVectorReplicate(3.f / 16.f);
               errors[i + 3] = XMVectorMultiplyAdd(diff, factor, errors[i + 3]);
            }
            if (3 != (i & 3))
            {
               const XMVECTOR factor = XMVectorReplicate(1.f / 16.f);
               errors[i + 5] = XMVectorMultiplyAdd(diff, factor, errors[i + 5]);
            }
         }
      }
      reinterpret_cast<uint32_t*>(destination)[1] = encodedIndices;
   }

   void EncodeBC3RGBA(const XMFLOAT4A* blockRGB, const float* blockA, uint8_t* destination, bool RGBDithering)
   {
      void EncodeBC4Alpha(const float*, uint8_t*);
      EncodeBC4Alpha(blockA, destination);
      EncodeBC1RGB(blockRGB, destination + BC4BlockSize, RGBDithering);
   }

   void EncodeBC4Alpha(const float* block, uint8_t* destination)
   {
      // Step 1: Find end points.
      bool bUsing4BlockCodec = false;
      for (size_t i = 0; i < BCBlockLength; ++i)
      {
         //  If there are boundary values in input texels, should use 4 interpolated color values to guarantee
         //  the exact code of the boundary values.
         if (block[i] == 0 || block[i] == 1)
         {
            bUsing4BlockCodec = true;
            break;
         }
      }
      float min, max;
      OptimizeAlpha(min, max, block, bUsing4BlockCodec ? 6 : 8);
      ColorFloat2Byte(destination[0], bUsing4BlockCodec ? min : max);
      ColorFloat2Byte(destination[1], bUsing4BlockCodec ? max : min);
      // Step 2: Compute indices, which follows the below mapping:
      // 0:C0, 1:C1, 2:Interpolation1, ..., 5:Interpolation4, 6:Interpolation5/0.0f, 7:Interpolation6/1.0f
      for (size_t i = 0; i < BCBlockLength; i++)
      {
         uint32_t value;
         if (bUsing4BlockCodec)
         {
            if (block[i] == 0) value = 6;
            else if (block[i] == 1) value = 7;
            else if (block[i] < min) value = (min - block[i]) / min <= 0.5f ? 6 : 0;
            else if (block[i] > max) value = (block[i] - max) / (1 - max) <= 0.5f ? 1 : 7;
            else
            {
               value = 5.f * (block[i] - min) / (max - min) + 0.5f;
               if (value == 0) value = 0;
               else if (value == 5) value = 1;
               else value += 1;
            }
         }
         else
         {
            value = 7.f * (block[i] - max) / (min - max) + 0.5f;
            if (value == 0) value = 0;
            else if (value == 7) value = 1;
            else value += 1;
         }
         PlaceBCIndex(destination + 2, value, i); // +2: Point it to the index block
      }
   }

   void EncodeBC5Normal(const float* blockRed, const float* blockGreen, uint8_t* destination)
   {
      EncodeBC4Alpha(blockRed, destination);
      EncodeBC4Alpha(blockGreen, destination + BC4BlockSize);
   }

   ForceInline void ApplyBarrier(ComPtr<ICommandList>& cmdList, ComPtr<IResource>& resource, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after)
   {
      D3D12_RESOURCE_BARRIER barrier
      {
         D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
         D3D12_RESOURCE_BARRIER_FLAG_NONE,
         D3D12_RESOURCE_TRANSITION_BARRIER { resource.Get(), D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, before, after }
      };
      cmdList->ResourceBarrier(1, &barrier);
   }

   // Return true if the client size doesn't change.
   ForceInline bool GetClientSize()
   {
      RECT rect{};
      GetClientRect(hwnd, &rect);
      auto oldValue = backbufferSize;
      backbufferSize = XMINT2{ rect.right, rect.bottom };
      return oldValue == backbufferSize;
   }

   void CreateBase()
   {
      // Factory
      uint32_t factoryFlags = 0;
#ifdef PILLOW_DEBUG
      factoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
      ComPtr<ID3D12Debug3> debugController;
      CheckHResult(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)));
      debugController->EnableDebugLayer();
#endif
      CheckHResult(CreateDXGIFactory2(factoryFlags, IID_PPV_ARGS(&factory)));
      BOOL winBool = 0;
      factory->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &winBool, sizeof(winBool));
      allowTearing = (winBool == TRUE);
      // Device
      try
      {
         CheckHResult(D3D12CreateDevice(nullptr, Constants::DX12FeatureLevel, IID_PPV_ARGS(&device))); // Default adapter
      }
      catch (...)
      {
         ComPtr<IDXGIAdapter> Warp;
         CheckHResult(factory->EnumWarpAdapter(IID_PPV_ARGS(&Warp)));
         CheckHResult(D3D12CreateDevice(Warp.Get(), Constants::DX12FeatureLevel, IID_PPV_ARGS(&device)));
      }
      // Queue
      D3D12_COMMAND_QUEUE_DESC queueDesc{ D3D12_COMMAND_LIST_TYPE_DIRECT, 0, D3D12_COMMAND_QUEUE_FLAG_NONE, 0 };
      CheckHResult(device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&cmdQueue)));
      // Fence
      fenceSync = std::make_unique<FenceSync>(device, cmdQueue);
      // Swapchain
      DXGI_SWAP_CHAIN_DESC1 swapChainDesc
      {
         0,0, DXGI_FORMAT::DXGI_FORMAT_R8G8B8A8_UNORM, false, DXGI_SAMPLE_DESC{1, 0}/*no obselete MSAA*/,
         DXGI_USAGE_RENDER_TARGET_OUTPUT, Constants::SwapChainSize, DXGI_SCALING_NONE,
         DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL/*need to access previous frame buffers*/, DXGI_ALPHA_MODE_IGNORE,
         uint32_t(allowTearing ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING/*allow to disable V-Sync*/ : 0)
      };
      CheckHResult(factory->CreateSwapChainForHwnd(cmdQueue.Get(), hwnd, &swapChainDesc, nullptr, nullptr, swapChain.GetAddressOf()));
      DXGI_RGBA color{ 0.f, 0.f, 0.f, 1.f };
      swapChain->SetBackgroundColor(&color);
      // Command Allocators & Lists
      int32_t count = Constants::SwapChainSize * threads;
      cmdAllocators.reserve(count);
      for (int i = 0; i < count; i++)
      {
         ComPtr<ID3D12CommandAllocator> temp;
         CheckHResult(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&temp)));
         cmdAllocators.push_back(std::move(temp));
      }
      // CreateCommandList1 closes the cmd list automatically.
      cmdLists.reserve(threads);
      _cmdLists.reserve(threads);
      for (int i = 0; i < threads; i++)
      {
         ComPtr<ICommandList> temp;
         CheckHResult(device->CreateCommandList1(0, D3D12_COMMAND_LIST_TYPE_DIRECT, D3D12_COMMAND_LIST_FLAG_NONE, IID_PPV_ARGS(&temp)));
         _cmdLists.push_back(temp.Get());
         cmdLists.push_back(std::move(temp));
      }
      // Others
      lateReleaseMgr = std::make_unique<LateReleaseManager>();
   }

   void CreateHeapsAndPSOs()
   {
      // Build all descriptor heaps.
      descriptorMgr = std::make_unique<DescriptorHeapManager>(device);

      // Create constant buffer and pass cbv.

   }

   void CreateFrames()
   {
      D3D12_RENDER_TARGET_VIEW_DESC rtvDesc
      {
         DXGI_FORMAT_R8G8B8A8_UNORM, D3D12_RTV_DIMENSION_TEXTURE2D
      };
      rtvDesc.Texture2D = { 0,0 };
      for (int i = 0; i < Constants::SwapChainSize; i++)
      {
         // After resizing the swapchain, the frame array index may not be euqal to the active backbuffer index.
         // So, we should associate the first buffer of the resized swapchain to the current frame array index.
         // e.g. frameIdx = 8, frameArrayIdx = 2, in this case, backbuffers[2] should refer to swapChain->GetBuffer(0).
         int32_t offset = (fenceSync->GetFrameIndex() + i) % Constants::SwapChainSize;
         CheckHResult(swapChain->GetBuffer(i, IID_PPV_ARGS(&backbuffers[offset])));
         tempRTVs[offset] = descriptorMgr->CreateView(device, backbuffers[offset], &rtvDesc, ViewType::RTV);
      }
   }

   void TryResizingSwapchain()
   {
      // Interval check.
      constexpr double MinInterval = 1.0 / 60.0;
      static double interval = 0;
      interval += TEMP_GetDeltaTime();
      if (interval < MinInterval) return;
      interval = 0;
      if (GetClientSize()) return;
      // Resize the swapchain.
      fenceSync->FlushQueue();
      for (int i = 0; i < Constants::SwapChainSize; i++)
      {
         backbuffers[i].Reset();
         descriptorMgr->ReleaseView(tempRTVs[i]);
      }
      CheckHResult(swapChain->ResizeBuffers(Constants::SwapChainSize, 0, 0, DXGI_FORMAT_R8G8B8A8_UNORM,
         allowTearing ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING/*allow to disable V-Sync*/ : 0));
      CreateFrames();
   }

   void BlockCompressionEncode()
   {

   }

   void RendererTestZone()
   {
      // footprint
      D3D12_RESOURCE_DESC resourceDesc
      {
         D3D12_RESOURCE_DIMENSION_TEXTURE2D, 0, 512, 512, 1, 10, DXGI_FORMAT_R8G8B8A8_UNORM,
         DXGI_SAMPLE_DESC{1, 0}, D3D12_TEXTURE_LAYOUT_UNKNOWN, D3D12_RESOURCE_FLAG_NONE
      };
      D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint[10];
      uint32_t rows[10];
      uint64_t rowSize[10];
      uint64_t totalSize;
      device->GetCopyableFootprints(&resourceDesc, 0, 10, 0, footprint, rows, rowSize, &totalSize);
   }
}

D3D12Renderer::D3D12Renderer(HWND windowHandle, int32_t threadCount) : GenericRenderer(threadCount, "D3D12Renderer")
{
   SingletonCheck();
   hwnd = windowHandle;
   threads = threadCount;
   GetClientSize();
   CreateBase();
   CreateHeapsAndPSOs();
   CreateFrames();
   RendererTestZone();
}

D3D12Renderer::~D3D12Renderer()
{
}

uint64_t D3D12Renderer::GetFrameIndex()
{
   return fenceSync->GetFrameIndex();
}

void D3D12Renderer::ReleaseResource(uint32_t handle)
{
}

void D3D12Renderer::Worker(int32_t workerIndex)
{
   int32_t frameIdx = fenceSync->GetFrameArrayIdx();
   ComPtr<ICommandList>& cmdList = cmdLists[workerIndex];
   ID3D12CommandAllocator* allocator = cmdAllocators[frameIdx * threads + workerIndex].Get();
   CheckHResult(allocator->Reset());
   CheckHResult(cmdList->Reset(allocator, nullptr));
   if (workerIndex == 0) UnitedBuffer::GPUCopy(cmdList); // Copy all dirty buffers to default heaps.
   // Do actual work.
   if (workerIndex == 0)
   {
      ApplyBarrier(cmdList, backbuffers[frameIdx], D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
      XMVECTOR _color = XMVectorReplicate(TEMP_GetLastingTime());
      _color = XMVectorAdd(_color, XMVectorSet(0, XM_PI * 0.66f, XM_PI * 1.33f, 0));
      _color = XMVectorMultiplyAdd(XMVectorSin(_color), XMVectorReplicate(0.5f), XMVectorReplicate(0.5f));
      XMFLOAT4 color;
      XMStoreFloat4(&color, _color);
      cmdList->ClearRenderTargetView(descriptorMgr->GetCPUHandle(tempRTVs[frameIdx]), (float*)(&color), 0, nullptr);
      ApplyBarrier(cmdList, backbuffers[frameIdx], D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
   }
   CheckHResult(cmdList->Close());
}

void Pillow::Graphics::D3D12Renderer::Pioneer()
{
   TryResizingSwapchain();
}

void D3D12Renderer::Assembler()
{
   lateReleaseMgr->ReleaseGarbage(); // Place it here, so it works not in the main thread.
   cmdQueue->ExecuteCommandLists(threads, _cmdLists.data());
   CheckHResult(swapChain->Present(verticalBlanks, (allowTearing && verticalBlanks == 0) ? DXGI_PRESENT_ALLOW_TEARING : 0));
   fenceSync->NextFrame();
}
#endif