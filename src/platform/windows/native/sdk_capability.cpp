#include <d2d1_3.h>
#include <sdkddkver.h>

static_assert(NTDDI_VERSION >= NTDDI_WIN10_RS2);

int main()
{
    ID2D1Factory3* factory = nullptr;
    ID2D1Device2* device = nullptr;
    ID2D1DeviceContext2* context = nullptr;

    return (factory == nullptr && device == nullptr && context == nullptr)
        ? 0
        : 1;
}
