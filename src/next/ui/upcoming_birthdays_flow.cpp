#include "next/ui/upcoming_birthdays_flow.h"

#include "next/application/gs_team_birthday_directory_read_query.h"
#include "next/application/korean_teacher_birthday_directory_read_query.h"
#include "next/application/native_english_teacher_birthday_directory_read_query.h"

#include <QByteArray>
#include <QCoreApplication>
#include <QString>

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace ClassMngr::Next::Ui
{
namespace
{

using Application::UpcomingBirthdayCandidate;
using Application::UpcomingBirthdayMonthDayInput;
using Application::UpcomingBirthdayScheduleUseCase;
using Application::UpcomingBirthdayStaffGroup;
using Domain::GregorianDate;

std::string utf8(const QString& value)
{
    const QByteArray bytes = value.toUtf8();
    return {bytes.constData(), static_cast<std::size_t>(bytes.size())};
}

std::u16string utf16(const QString& value)
{
    return value.toStdU16String();
}

QString fromUtf16(const std::u16string& value)
{
    return QString::fromUtf16(
        value.data(),
        static_cast<qsizetype>(value.size())
        );
}

int compareLocaleAwareText(
    const std::u16string_view left,
    const std::u16string_view right
    )
{
    return QString::localeAwareCompare(
        fromUtf16(std::u16string(left)).trimmed(),
        fromUtf16(std::u16string(right)).trimmed()
        );
}

QString preferredKoreanDisplayName(
    const Application::KoreanTeacherBirthdayDirectoryEntry& teacher
    )
{
    const QString preferred = fromUtf16(teacher.preferredName).trimmed();
    if (!preferred.isEmpty())
    {
        return preferred;
    }

    const QString english = fromUtf16(teacher.teacherEn).trimmed();
    if (!english.isEmpty())
    {
        return english;
    }

    const QString romanization =
        fromUtf16(teacher.preferredRomanization).trimmed();
    if (!romanization.isEmpty())
    {
        return romanization;
    }

    return fromUtf16(teacher.teacherKr).trimmed();
}

std::vector<UpcomingBirthdayCandidate> candidatesFor(
    const Application::KoreanTeacherBirthdayDirectorySnapshot& korean,
    const Application::NativeEnglishTeacherBirthdayDirectorySnapshot& native,
    const Application::GsTeamBirthdayDirectorySnapshot& gsTeam
    )
{
    std::vector<UpcomingBirthdayCandidate> candidates;
    candidates.reserve(korean.size() + native.size() + gsTeam.size());

    for (const auto& teacher : korean)
    {
        candidates.push_back({
            .birthdayMonthDay = UpcomingBirthdayMonthDayInput(
                utf8(fromUtf16(teacher.birthday).trimmed())
                ),
            .displayName = utf16(preferredKoreanDisplayName(teacher)),
            .group = UpcomingBirthdayStaffGroup::KoreanTeacher
        });
    }
    for (const auto& teacher : native)
    {
        candidates.push_back({
            .birthdayMonthDay = UpcomingBirthdayMonthDayInput(
                utf8(fromUtf16(teacher.birthday).trimmed())
                ),
            .displayName = utf16(fromUtf16(teacher.name).trimmed()),
            .position = utf16(fromUtf16(teacher.position).trimmed()),
            .group = UpcomingBirthdayStaffGroup::NativeEnglishTeacher
        });
    }
    for (const auto& member : gsTeam)
    {
        QString displayName = fromUtf16(member.name).trimmed();
        if (displayName.isEmpty())
        {
            displayName = fromUtf16(member.koreanName).trimmed();
        }
        candidates.push_back({
            .birthdayMonthDay = UpcomingBirthdayMonthDayInput(
                utf8(fromUtf16(member.birthday).trimmed())
                ),
            .displayName = utf16(displayName),
            .position = utf16(fromUtf16(member.position).trimmed()),
            .group = UpcomingBirthdayStaffGroup::GsTeam
        });
    }
    return candidates;
}

std::optional<GregorianDate> referenceDate(const QDate& date)
{
    if (!date.isValid())
    {
        return std::nullopt;
    }
    return GregorianDate::fromComponents(date.year(), date.month(), date.day());
}

QString localized(const char* source)
{
    return QCoreApplication::translate("NextUpcomingBirthdaysFlow", source);
}

std::string utf8Message(const QString& value)
{
    const QByteArray bytes = value.toUtf8();
    return {bytes.constData(), static_cast<std::size_t>(bytes.size())};
}

void warn(
    Application::NotificationPort& notifications,
    const std::string& details
    )
{
    QString message = localized("Birthdays could not be loaded.");
    if (!details.empty())
    {
        message += QStringLiteral("\n")
            + QString::fromUtf8(
                details.data(),
                static_cast<qsizetype>(details.size())
                );
    }
    static_cast<void>(notifications.show({
        .message = utf8Message(message),
        .severity = Application::NotificationSeverity::Warning
    }));
}

} // namespace

UpcomingBirthdaysFlow::UpcomingBirthdaysFlow(
    const Application::KoreanTeacherBirthdayDirectoryReadPort& koreanTeachers,
    const Application::NativeEnglishTeacherBirthdayDirectoryReadPort& nativeEnglishTeachers,
    const Application::GsTeamBirthdayDirectoryReadPort& gsTeam,
    const Application::UpcomingBirthdayDismissalPort& dismissal,
    Application::NotificationPort& notifications,
    DialogPresenter presentDialog
    )
    : m_koreanTeachers(&koreanTeachers)
    , m_nativeEnglishTeachers(&nativeEnglishTeachers)
    , m_gsTeam(&gsTeam)
    , m_dismissal(&dismissal)
    , m_notifications(&notifications)
    , m_presentDialog(std::move(presentDialog))
{
}

void UpcomingBirthdaysFlow::run(const QDate& localDate) const
{
    if (!m_koreanTeachers || !m_nativeEnglishTeachers || !m_gsTeam
        || !m_dismissal || !m_notifications || !m_presentDialog)
    {
        return;
    }

    const Application::KoreanTeacherBirthdayDirectoryReadQuery koreanQuery(
        *m_koreanTeachers);
    const auto korean = koreanQuery.execute();
    if (!korean)
    {
        if (korean.error().code != Domain::ErrorCode::NotFound)
        {
            warn(*m_notifications, korean.error().message);
        }
        return;
    }

    const Application::NativeEnglishTeacherBirthdayDirectoryReadQuery nativeQuery(
        *m_nativeEnglishTeachers);
    const Application::GsTeamBirthdayDirectoryReadQuery gsTeamQuery(*m_gsTeam);
    const auto native = nativeQuery.execute();
    const auto gsTeam = gsTeamQuery.execute();
    if (!native || !gsTeam)
    {
        warn(
            *m_notifications,
            !native ? native.error().message : gsTeam.error().message
            );
        return;
    }

    Application::UpcomingBirthdaySchedule schedule;
    {
        const std::vector<UpcomingBirthdayCandidate> candidates = candidatesFor(
            korean.value(),
            native.value(),
            gsTeam.value()
            );
        schedule = UpcomingBirthdayScheduleUseCase::build(
            candidates,
            referenceDate(localDate),
            compareLocaleAwareText
            );
    }

    if (!m_presentDialog(schedule))
    {
        return;
    }

    const QString isoDate = localDate.toString(Qt::ISODate);
    const QByteArray encodedDate = isoDate.toUtf8();
    const Application::CalendarEventDate dismissalDate(
        std::string(encodedDate.constData(),
            static_cast<std::size_t>(encodedDate.size()))
        );
    const Domain::Result<void> persisted = m_dismissal->write(dismissalDate);
    if (!persisted)
    {
        QString message = localized("The dismissal preference could not be saved.");
        if (!persisted.error().message.empty())
        {
            message += QStringLiteral("\n")
                + QString::fromUtf8(
                    persisted.error().message.data(),
                    static_cast<qsizetype>(persisted.error().message.size())
                    );
        }
        static_cast<void>(m_notifications->show({
            .message = utf8Message(message),
            .severity = Application::NotificationSeverity::Warning
        }));
    }
}

} // namespace ClassMngr::Next::Ui
