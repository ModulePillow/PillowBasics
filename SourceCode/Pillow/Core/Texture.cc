#include "Texture.h"
#include "fstream"
#include "filesystem"
#include "lodepng-apr2025/lodepng.h"

using namespace Pillow;
using namespace Pillow::Graphics;
using namespace DirectX;

namespace
{
   void BicubicDownsampling(const uint8_t* input, uint8_t* output, int32_t inputWidth, bool is4Channels = true)
   {
      //auto ToFloat_DecodeSRGB = [](uint8_t x) -> float
      //   {
      //      float output = (float)x / UINT8_MAX;
      //      output = MathF.Pow(((output + 0.055f) / 1.055f), 2.4f);
      //      output = MathF.Round(output * byte.MaxValue);
      //      value = (byte)output;
      //   };
      //auto ToUNORM = [](float x) -> uint8_t
      //   {

      //   };
      // Catmull-Rom spline kernel. Renowned for high sharpness.
      auto constexpr Weight = [](float x) -> float
         {
            x = x < 0 ? -x : x;
            if (x <= 1)
            {
               return 1.0 - 2.0 * x * x + x * x * x;
            }
            else if (x < 2)
            {
               return 4.0 - 8.0 * x + 5.0 * x * x - x * x * x;
            }
            return 0;
         };
      // Const & constexpr params.
      const int32_t scale = 2;
      const int32_t channels = is4Channels ? 4 : 1;
      const int32_t outputWidth = inputWidth / scale;
      const int32_t count = outputWidth * outputWidth;
      constexpr float w[2]{ Weight(0.5f), Weight(1.5f) };
      constexpr XMFLOAT4 _k0{ w[1] * w[0], w[0] * w[0], w[0] * w[0], w[1] * w[0] };
      constexpr XMFLOAT4 _k1{ w[1] * w[1], w[0] * w[1], w[0] * w[1], w[1] * w[1] };
      const XMVECTOR k0 = XMLoadFloat4(&_k0);
      const XMVECTOR k1 = XMLoadFloat4(&_k1);     
      // Downsampling.
      for (int32_t i = 0; i < count; i++)
      {
         // 1 Define the kernel and the coordinates of samples.
         // DirectX texture coordinate definition:
         // |---> (u)
         // |
         // v (v)
         int32_t u = (i % outputWidth) * scale;
         int32_t v = (i / outputWidth) * scale;
         XMVECTOR min = XMVectorZero();
         XMVECTOR max = XMVectorReplicateInt(inputWidth - 1);
         XMVECTOR sample_u = XMVectorSetInt(u - 1, u, u + 1, u);
         XMVECTOR sample_v = XMVectorSetInt(v - 1, v, v + 1, v);
         sample_u = XMVectorClamp(sample_u, min, max);
         sample_v = XMVectorClamp(sample_v, min, max);
         XMVECTOR result = XMVectorZero();
         // 2 Sampling and convolution.
         for (int row = 0; row < 4; row++)
         {
            const XMVECTOR& kernel = (row == 0 || row == 3) ? k1 : k0;
            XMVECTOR rowOffset = XMVectorReplicateInt(((int32_t*)&sample_v)[row] * inputWidth * channels);
            XMVECTOR offset = XMVectorAdd(rowOffset, XMVectorMultiply(sample_u, XMVectorReplicateInt(channels)));
            XMINT4 _offset;
            XMStoreSInt4(&_offset, offset);
            XMVECTOR sample = XMVectorZero();
            if (is4Channels)
            {
               XMVECTOR channelIndex = XMVectorSet(0, 1, 2, 3);
               for (int chl = 0; chl < 4; chl++)
               {
                  XMVECTOR singleChannel = XMVectorSet(input[_offset.x + chl], input[_offset.y + chl], input[_offset.z + chl], input[_offset.w + chl]);
                  XMVECTOR channelMask = XMVectorEqual(XMVectorReplicate(chl), channelIndex);
                  sample = XMVectorSelect(sample, XMVector4Dot(singleChannel, kernel), channelMask);
               }
            }
            else
            {
               sample = XMVectorSet(input[_offset.x], input[_offset.y], input[_offset.z], input[_offset.w]);
               sample = XMVector4Dot(sample, kernel);
            }
            result = XMVectorAdd(result, sample);
         }
         // 3 Store the result.
         if (is4Channels)
         {
            XMStoreFloat4((XMFLOAT4*)(output + i * channels), result);
         }
         else
         {
            *(output + i) = XMVectorGetX(result);
         }
      }
   }
}

GenericTextureInfo::GenericTextureInfo(GenericTexFmt format, int32_t width, bool bMips, CompressionMode compMode, bool bCube, int32_t arraySize) :
   f_Format(format),
   f_PixelSize(uint8_t(PixelSize[int32_t(format)])),
   f_Width(uint16_t(width)),
   f_ArrayCount(uint8_t(arraySize* (bCube ? 6 : 1))),
   f_IsCubemap(bCube),
   f_CompressionMode(compMode)
{
   if (width < 4 || (width & (width - 1))) throw std::exception("Texture width restriction: w=2^n and w>=4");
   int32_t power = std::log2f(width);
   // The lowest mipmap limit is 4x4, needed by block compression.
   f_MipCount = bMips ? power - 1 : 1;
   f_ArraySliceSize = bMips ? (((1 << (2 * f_MipCount + 4)) - 16) / 3) * GetPixelSize() : GetMipZeroSize();
   f_MipZeroSize = f_Width * f_Width * f_PixelSize;
   f_TotalSize = f_ArrayCount * f_ArraySliceSize;
}

void Pillow::Graphics::LoadTexture(const string& relativePath)
{
   // Read the binary file.
   string path = GetResourcePath(relativePath);
   std::ifstream file(path, std::ios::binary | std::ios::ate);
   if (!file.is_open()) throw std::runtime_error("Unable to open file");
   std::streamsize size = file.tellg();
   file.seekg(0, std::ios::beg);
   std::vector<unsigned char> fileData(size);
   if (size > 0 && !file.read((char*)fileData.data(), size)) throw std::runtime_error("Error reading file");
   file.close();
   // Decode it.
   std::vector<unsigned char> imageData;
   uint32_t w, h;
   lodepng::State state;
   //state.decoder.ignore_crc = 1;
   //state.decoder.zlibsettings.ignore_adler32 = 1;
   lodepng::decode(imageData, w, h, state, fileData);
   if (state.info_raw.bitdepth != 8) throw std::exception("Bitdepth should be 8.");
   if (w!=h) throw std::exception("The image should be square.");
   GenericTextureInfo texInfo;
   if (state.info_raw.colortype == LCT_GREY)
   {
      texInfo = GenericTextureInfo(GenericTexFmt::UnsignedNormalized_R8, w);
   }
   else if (state.info_raw.colortype == LCT_RGB)
   {
      texInfo = GenericTextureInfo(GenericTexFmt::UnsignedNormalized_R8G8B8A8, w);
   }
   else if (state.info_raw.colortype == LCT_RGBA)
   {
      texInfo = GenericTextureInfo(GenericTexFmt::UnsignedNormalized_R8G8B8A8, w);
   }

}