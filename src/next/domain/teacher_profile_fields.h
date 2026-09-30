#pragma once

#include <string>

namespace ClassMngr::Next::Domain
{

// UTF-16 keeps the same code-unit semantics as the legacy QString fields
// while leaving the profile value independent of Qt.
struct TeacherProfileFields final
{
    std::u16string teacherKr;
    std::u16string teacherEn;
    std::u16string preferredRomanization;
    std::u16string preferredName;

    std::u16string roomNumber;
    std::u16string birthday;
    std::u16string phoneNumber;

    std::u16string wifiName;
    std::u16string wifiPassword;
    std::u16string internetType = u"WiFi";

    std::u16string zoomId;
    std::u16string zoomPassword;
    std::u16string projectionType = u"HDMI";

    std::u16string notes;

    friend bool operator==(
        const TeacherProfileFields&,
        const TeacherProfileFields&
        ) = default;
};

} // namespace ClassMngr::Next::Domain
