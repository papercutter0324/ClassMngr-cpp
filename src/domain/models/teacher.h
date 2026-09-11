#ifndef TEACHER_H
#define TEACHER_H

#include "classmngr/engine/teacher.h"

#include <QByteArray>
#include <QString>
#include <QStringList>

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

struct Teacher;

namespace teacher_detail
{
inline std::string toUtf8(const QString& value)
{
    const QByteArray encoded = value.toUtf8();
    return {
        encoded.constData(),
        static_cast<std::size_t>(encoded.size())
    };
}

inline QString fromUtf8(std::string_view value)
{
    return QString::fromUtf8(
        value.data(),
        static_cast<qsizetype>(value.size())
        );
}

inline classmngr::engine::Teacher toEngine(const Teacher& teacher);
} // namespace teacher_detail

struct Teacher
{
    int id = -1;

    QString teacherKr;
    QString teacherEn;
    QString preferredRomanization;
    QString preferredName;

    QString roomNumber;
    QString birthday;
    QString phoneNumber;

    QString wifiName;
    QString wifiPassword;
    QString internetType = QStringLiteral("WiFi");

    QString zoomId;
    QString zoomPassword;
    QString projectionType = QStringLiteral("HDMI");

    QString notes;

    [[nodiscard]] QStringList preferredNameChoices() const
    {
        const std::vector<std::string> engineChoices =
            teacher_detail::toEngine(*this).preferredNameChoices();
        QStringList choices;
        choices.reserve(static_cast<qsizetype>(engineChoices.size()));
        for (const std::string& choice : engineChoices)
        {
            choices.append(teacher_detail::fromUtf8(choice));
        }
        return choices;
    }

    [[nodiscard]] QString preferredDisplayName() const
    {
        return teacher_detail::fromUtf8(
            teacher_detail::toEngine(*this).preferredDisplayName()
            );
    }
};

namespace teacher_detail
{
inline classmngr::engine::Teacher toEngine(const Teacher& teacher)
{
    classmngr::engine::Teacher result;
    result.teacherKr = toUtf8(teacher.teacherKr);
    result.teacherEn = toUtf8(teacher.teacherEn);
    result.preferredRomanization = toUtf8(teacher.preferredRomanization);
    result.preferredName = toUtf8(teacher.preferredName);
    return result;
}
} // namespace teacher_detail

#endif // TEACHER_H
