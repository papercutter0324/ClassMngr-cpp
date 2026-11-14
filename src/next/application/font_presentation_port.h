#pragma once

namespace ClassMngr::Next::Application
{

// Supplies host-specific point sizes used by the Next UI's font presentation.
// Platform details stay in the adapter rather than feature and UI classes.
class FontPresentationPort
{
public:
    virtual ~FontPresentationPort() = default;

    [[nodiscard]] virtual int applicationBaseFontPointSize() const = 0;
    [[nodiscard]] virtual int teacherProfileKoreanFontPointSize() const = 0;
};

}  // namespace ClassMngr::Next::Application
