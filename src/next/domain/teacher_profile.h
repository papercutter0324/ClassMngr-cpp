#pragma once

#include <compare>
#include <optional>
#include <string>
#include <utility>

namespace ClassMngr::Next::Domain
{

class TeacherId final
{
public:
    [[nodiscard]] static std::optional<TeacherId> fromInt(
        const int value
        )
    {
        if (value <= 0)
        {
            return std::nullopt;
        }

        return TeacherId(value);
    }

    [[nodiscard]] int value() const
    {
        return m_value;
    }

    friend bool operator==(
        const TeacherId&,
        const TeacherId&
        ) = default;

    friend auto operator<=>(
        const TeacherId&,
        const TeacherId&
        ) = default;

private:
    explicit TeacherId(
        const int value
        )
        : m_value(value)
    {
    }

    int m_value;
};

// std::u16string preserves QString's UTF-16 code-unit length semantics while
// keeping this value and its policies independent of Qt.
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

struct TeacherProfile final
{
    TeacherId id;
    TeacherProfileFields fields;

    friend bool operator==(
        const TeacherProfile&,
        const TeacherProfile&
        ) = default;
};

} // namespace ClassMngr::Next::Domain
