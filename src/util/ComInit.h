#pragma once
#include <combaseapi.h>
#include <mfapi.h>

namespace LightRec::Util {

class ComInit {
public:
    ComInit() {
        CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        MFStartup(MF_VERSION, MFSTARTUP_NOSOCKET);
    }

    ~ComInit() {
        MFShutdown();
        CoUninitialize();
    }

    ComInit(const ComInit&) = delete;
    ComInit& operator=(const ComInit&) = delete;
    ComInit(ComInit&&) = delete;
    ComInit& operator=(ComInit&&) = delete;
};

} // namespace LightRec::Util
