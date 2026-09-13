#include <OpenXLSX.hpp>

int main()
{
    OpenXLSX::XLDocument document;
    return document.isOpen() ? 1 : 0;
}
