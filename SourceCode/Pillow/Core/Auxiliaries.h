#pragma once
#include <map>
#include <typeinfo>
#include <type_traits>
#include <exception>
#include <shared_mutex>
#include <string>
#include <ranges>
#include <filesystem>
#include <locale>
#include <chrono>
#if defined(_WIN64)
#define NOMINMAX
#include <Windows.h>
#elif defined(__ANDROID__)
#endif
#include "utfcpp-4.0.6/utf8.h"
#include "DirectXMath-apr2025/DirectXMath.h"

// Template
#if defined(_WIN64)
#elif defined(__ANDROID__)
#endif

#if defined(_MSC_VER)
#define ForceInline __forceinline
#elif defined(__GNUC__) | defined(__clang__)
#define ForceInline __attribute__((always_inline))
#endif

// A known issue: VS applies wrong formats for consecutive "PropertyReadonly" macros.
// _Aaa is reserved for the compiler in C++, makes a C# programmer wanna cry.
// "f_" means field; it's from C#.
#define ReadonlyProperty(type, name) \
protected: type f_##name{}; \
public: ForceInline type Get##name() const { return f_##name; }

#define SingletonCheck() \
static decltype(this) instance = nullptr; \
if(instance) throw std::exception("A singleton class cannot be created twice."); \
instance = this;

#define DeleteDefautedMethods(type) \
public: \
type() = delete; \
type(const type&) = delete; \
type(type&&) = delete; \
type& operator=(const type&) = delete; \
type& operator=(type&&) = delete;

namespace Pillow
{
   // I. The Mystery of Character Set
   // ASCII char set:   char = code point(a number)  = encoding(actual binaries)
   // Unicode char set: char = code point(a number) != encoding(actual binaries)
   // Unicode has several encoding methods. The most famous ones are UTF-8, UTF-16, and UTF-32
   // Only UTF-32 is completely equal to the code points of Unicode.
   // 
   // II. Win and UTF-8
   // On Win, it's bad to invoke the system's interfaces with UTF-8.
   // Cause UTF-8 is optional(Region->Change System locale->Beta: Use Unicode UTF-8...),
   // which means the client's machines may not support it.
   // For example, MessageBoxA supports UTF-8 only when clients enable the above option(off 
   // by default), but MessageBoxW always supports UTF-16.
   // 
   // III. The Naughty Wide Character
   // whcar_t represents UTF-16 on Win, However, represents UTF-32 on Android typically.
   // 
   // IV. The String Convention of Pillow Basics
   // Only use UTF-8 strings in Pillow Basics for coherence.
   using std::string;
   using namespace DirectX;

   struct alignas(64) CacheLine
   {
      uint8_t padding[64]{}; // 64 bytes cache line padding
   };

   // The alignment must be a power of two.
   ForceInline int32_t GetAlignedSize(int32_t size, int32_t alignment)
   {
      return (size + alignment - 1) & ~(alignment - 1);
   }

   class KeyValuePair
   {
   public:
      enum struct ValueType : uint8_t
      {
         String,
         Integer,
         Float,
         Float4
      };

      ReadonlyProperty(string, Key)
         ReadonlyProperty(string, ValueRaw)
         ReadonlyProperty(ValueType, Type)

   public:
      // isStringValue: True if using quick initializing route.
      KeyValuePair(string key, string value, bool isStringValue = false);

      ForceInline bool IsKeyOnly() const { return f_ValueRaw.empty(); }

      ForceInline int32_t GetInteger() const { return std::stoi(f_ValueRaw); }

      ForceInline float GetFloat() const { return std::stof(f_ValueRaw); }

      XMFLOAT4A GetFloat4Aligned();

      bool operator==(const KeyValuePair& right) const;

      bool operator>(const KeyValuePair& right) const;

      bool operator<(const KeyValuePair& right) const;
   };

   // Create 64-bytes-aligned memory.
   ForceInline std::unique_ptr<CacheLine[]> CreateAlignedMemory(int32_t unalignedSize)
   {
      return std::make_unique<CacheLine[]>((unalignedSize + sizeof(CacheLine) - 1) / sizeof(CacheLine));
   }

   ForceInline bool CheckUTF8(const string& str)
   {
      return utf8::is_valid(str.begin(), str.end());
   }

   string GetResourcePath(const string& name);
   void LogSystem(const string& text);
   void LogGame(const string& text);

   /*
   * std::chrono::steady_clock
   *
   * [https://en.cppreference.com/w/cpp/chrono/steady_clock]
   *
   * [Member types]
   * rep: a number type used to count ticks. e.g.: int64_t, double
   * period: a std::ratio type representing the tick size in seconds. e.g.: ratio<1, 1000> means 1 millisecond.
   * duration: a type representing ranges of ticks.
   * time_point: a type representing time points.
   */
   class GameClock
   {
      ReadonlyProperty(double, DeltaTime)
         ReadonlyProperty(double, LastingTime)
   public:
      void Start();
      void Tick();
      static double GetPrecisionMilliseconds();

   private:
      std::chrono::steady_clock::time_point startPoint;
      std::chrono::steady_clock::time_point lastPoint;
   };
}