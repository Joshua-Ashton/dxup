#pragma once

#include <string>
#include <vector>
#include <memory>
#include <cstring>

namespace dxapex {

  template <typename T>
  void InitReturnPtr(T* ptr) {
    if (ptr)
      *ptr = nullptr;
  }

  template <typename T, typename J>
  void pushObject(std::vector<J>& vec, T& thing) {
    J* ptrThing = (J*)&thing;
    for (J i = 0; i < sizeof(T) / sizeof(J); i++)
      vec.push_back(*(ptrThing + i));
  }

  template <typename T>
  T* lastPtr(std::vector<T>& vec) {
    return &vec[vec.size() - 1];
  }

  template <typename T>
  T* nextPtr(std::vector<T>& vec) {
    return lastPtr(vec) + 1;
  }

  template <typename T>
  void pushAlignedString(std::vector<T>& vec, const char* str, size_t len) {
    len++; // for NULL terminator
    size_t paddedLen = len + (sizeof(T) - (len % sizeof(T)));

    uint32_t* end = nextPtr(vec);
    vec.resize(vec.size() + (paddedLen / sizeof(T)));
    std::memset(end, 0xAB, paddedLen); // FXC pads with 0xAB
    std::memcpy(end, str, len);
  }

  template <typename T>
  void pushAlignedString(std::vector<T>& vec, const std::string& str) {
    pushAlignedString(vec, str.c_str(), str.length());
  }

}