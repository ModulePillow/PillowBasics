#include "Auxiliaries.h"
#include <regex>

using namespace Pillow;
using namespace std::chrono;

namespace
{
   GameClock globalGameClock;
}

KeyValuePair::KeyValuePair(string key, string value, bool isStringValue) :
   _Key(std::regex_replace(key, std::regex("\\s"), "")),
   _ValueRaw(std::regex_replace(value, std::regex("\\s"), ""))
{
   if (value.empty() || isStringValue)
   {
      _Type = ValueType::String;
   }
   else if (std::regex_match(value, std::regex(R"(^\-?\d+$)")))
   {
      _Type = ValueType::Integer;
   }
   else if (std::regex_match(value, std::regex(R"(^\-?\d+\.\d+$)")))
   {
      _Type = ValueType::Float;
   }
   else if (std::regex_match(value, std::regex(R"(^\-?\d+\.\d+(,\-?\d+\.\d+){1,3}$)")))
   {
      _Type = ValueType::Float4;
   }
   else
   {
      _Type = ValueType::String;
   }
}

XMFLOAT4A Pillow::KeyValuePair::GetFloat4Aligned()
{

   auto view = std::ranges::split_view(_ValueRaw, ',');
   XMFLOAT4A result{};
   int32_t i = 0;
   for (auto it = view.begin(); it != view.end(); it++, i++)
   {
      string temp((*it).begin(), (*it).end());
      result[i] = std::stof(temp);
   }
   return result;
}

bool KeyValuePair::operator==(const KeyValuePair& right) const
{
   if (this->_Type == ValueType::Integer)
   {
      return this->GetInteger() == right.GetInteger();
   }
   if (this->_Type == ValueType::Float)
   {
      return this->GetFloat() == right.GetFloat();
   }
   return this->_Key == right._Key && this->_ValueRaw == right._ValueRaw;
}

bool Pillow::KeyValuePair::operator>(const KeyValuePair& right) const
{
   if (this->_Type == ValueType::Integer)
   {
      return this->GetInteger() > right.GetInteger();
   }
   if (this->_Type == ValueType::Float)
   {
      return this->GetFloat() > right.GetFloat();
   }
   return this->_Key > right._Key;
}

bool Pillow::KeyValuePair::operator<(const KeyValuePair& right) const
{
   return this->operator>(right);
}

string Pillow::GetResourcePath(const string& name)
{
   using namespace std::filesystem;
   static std::filesystem::path resourceRootPath;
   if (resourceRootPath.empty())
   {
      path currentPath = current_path();
      do
      {
         path searchPath = currentPath / path("Resources");
         if (exists(searchPath))
         {
            resourceRootPath = searchPath;
            break;
         }
         currentPath = currentPath.parent_path();
      } while (currentPath != currentPath.root_path());
      if (resourceRootPath.empty()) throw std::exception("\"Resources\" folder does not exist.");
   }
   string result;
#if defined(_WIN64)
   std::wstring _result = resourceRootPath / name;
   utf8::utf16to8(_result.begin(), _result.end(), std::back_inserter(result));
#elif defined(__ANDROID__)
#endif
   return result;
}

void Pillow::LogSystem(const string& text)
{
#if defined(_WIN64)
   std::wstring _text;
   utf8::utf8to16(text.begin(), text.end(), std::back_inserter(_text));
   OutputDebugString(_text.c_str());
   OutputDebugString(L"\n");
#elif defined(__ANDROID__)
#endif
}

void Pillow::LogGame(const string& text)
{
   //
}

void GameClock::Start()
{
   startPoint = steady_clock::now();
   lastPoint = startPoint;
}

void GameClock::Tick()
{
   auto currentPoint = steady_clock::now();
   _DeltaTime = duration_cast<duration<double, std::ratio<1>>>(currentPoint - lastPoint).count();
   _LastingTime = duration_cast<duration<double, std::ratio<1>>>(currentPoint - startPoint).count();
   lastPoint = currentPoint;
}

double GameClock::GetPrecisionMilliseconds()
{
   const int32_t test_rounds = 5;
   auto last = steady_clock::now();
   steady_clock::duration precision = steady_clock::duration::max();
   for (size_t i = 0; i < test_rounds; ++i) {
      auto next = steady_clock::now();
      while (next == last) next = steady_clock::now();
      auto interval = next - last;
      precision = std::min(precision, interval);
      last = next;
   }
   return duration_cast<duration<double, std::milli>>(precision).count();
}