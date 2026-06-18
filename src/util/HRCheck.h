#pragma once
#include <system_error>
#include <windows.h>

namespace LightRec::Util {

inline void ThrowIfFailed(HRESULT hr, const char* context = nullptr) {
    if (FAILED(hr)) {
        throw std::system_error(hr, std::system_category(), context ? context : "HRESULT failure");
    }
}

} // namespace LightRec::Util
