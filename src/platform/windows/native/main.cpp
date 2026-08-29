#include "native_application.h"

#include <objbase.h>
#include <shellapi.h>

#include <string>
#include <vector>

int WINAPI wWinMain(
    HINSTANCE instance,
    HINSTANCE,
    PWSTR,
    int
    )
{
    const HRESULT comResult = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(comResult))
    {
        return static_cast<int>(comResult);
    }

    int argumentCount = 0;
    LPWSTR* rawArguments = CommandLineToArgvW(
        GetCommandLineW(),
        &argumentCount
        );

    std::vector<std::wstring> arguments;
    if (rawArguments)
    {
        arguments.reserve(static_cast<std::size_t>(argumentCount));
        for (int index = 1; index < argumentCount; ++index)
        {
            arguments.emplace_back(rawArguments[index]);
        }
        LocalFree(rawArguments);
    }

    NativeApplication application(instance);
    const int result = application.run(arguments);
    CoUninitialize();
    return result;
}
