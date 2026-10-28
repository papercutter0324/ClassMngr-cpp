#include "next/platform/workspace_store_class_analytics_actions.h"

#include "core/utils/student_name_utils.h"
#include "data/database/workspace_store.h"
#include "data/repositories/class_info_repository.h"
#include "data/repositories/roster_repository.h"
#include "data/repositories/settings_repository.h"
#include "data/repositories/speaking_eval_repository.h"
#include "next/application/class_analytics_dashboard_read_query.h"
#include "next/application/academic_calendar_schedule_preferences.h"
#include "next/application/evaluation_default_selection.h"
#include "next/application/academic_year_schedule_policy.h"
#include "next/application/evaluation_default_policy_preferences.h"
#include "next/domain/course.h"
#include "next/platform/repository_error_code.h"

#include <QByteArray>
#include <QDate>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QString>
#include <QVariant>

#include <array>
#include <charconv>
#include <exception>
#include <limits>
#include <optional>
#include <map>
#include <string>
#include <system_error>
#include <utility>

namespace ClassMngr::Next::Platform
{
namespace
{

[[nodiscard]] Domain::OperationError error(
    const Domain::ErrorCode code,
    std::string message
    )
{
    return {
        .code = code,
        .message = std::move(message),
        .recoverable = code != Domain::ErrorCode::InvalidInput
    };
}

[[nodiscard]] std::optional<int> legacyClassId(const Domain::ClassId& classId)
{
    const std::string& value = classId.value();
    int parsed = 0;
    const auto [end, parseError] = std::from_chars(
        value.data(), value.data() + value.size(), parsed, 10);
    if (parseError != std::errc{} || end != value.data() + value.size()
        || parsed <= 0 || std::to_string(parsed) != value)
    {
        return std::nullopt;
    }
    return parsed;
}

[[nodiscard]] std::string utf8(const QString& value)
{
    const QByteArray bytes = value.toUtf8();
    return {bytes.constData(), static_cast<std::size_t>(bytes.size())};
}

class WorkspaceStoreEvaluationDefaultPolicyPort final
    : public Application::EvaluationDefaultPolicyPreferencesPort
{
public:
    explicit WorkspaceStoreEvaluationDefaultPolicyPort(
        SettingsRepository* settings) noexcept
        : m_settings(settings)
    {
    }

    [[nodiscard]] Application::EvaluationDefaultPolicyPreferencesLoadResult
    load() const override
    {
        if (!m_settings)
        {
            return Application::EvaluationDefaultPolicyPreferencesLoadResult::success(
                Application::EvaluationDefaultPolicy::All);
        }
        const auto stored = m_settings->loadSetting(QStringLiteral(
            "classes_navigation_evaluation_default_policy"));
        if (!stored)
        {
            return Application::EvaluationDefaultPolicyPreferencesLoadResult::failure(
                error(repositoryDomainErrorCode(stored.error().code),
                    stored.error().diagnostic.toStdString()));
        }
        if (stored->isValid()
            && stored->toString().trimmed().toLower()
                == QStringLiteral("current_or_previous_term"))
        {
            return Application::EvaluationDefaultPolicyPreferencesLoadResult::success(
                Application::EvaluationDefaultPolicy::CurrentOrPreviousTerm);
        }
        return Application::EvaluationDefaultPolicyPreferencesLoadResult::success(
            Application::EvaluationDefaultPolicy::All);
    }

    [[nodiscard]] Application::EvaluationDefaultPolicyPreferencesSaveResult
    save(const Application::EvaluationDefaultPolicy) const override
    {
        return Application::EvaluationDefaultPolicyPreferencesSaveResult::failure(
            error(Domain::ErrorCode::Validation,
                "Class Analytics does not edit evaluation defaults."));
    }

private:
    SettingsRepository* m_settings = nullptr;
};

class WorkspaceStoreAcademicCalendarSchedulePreferencesPort final
    : public Application::AcademicCalendarSchedulePreferencesPort
{
public:
    explicit WorkspaceStoreAcademicCalendarSchedulePreferencesPort(
        SettingsRepository* settings) noexcept
        : m_settings(settings)
    {
    }

    [[nodiscard]] Application::AcademicCalendarSchedulePreferencesReadResult
    read() const override
    {
        if (!m_settings)
        {
            return Application::AcademicCalendarSchedulePreferencesReadResult::success({});
        }
        const auto stored = m_settings->loadSetting(
            QStringLiteral("calendar/academicSchedule/v1"));
        if (!stored)
        {
            return Application::AcademicCalendarSchedulePreferencesReadResult::failure(
                error(repositoryDomainErrorCode(stored.error().code),
                    stored.error().diagnostic.toStdString()));
        }
        const QByteArray bytes = stored->toString().toUtf8();
        return Application::AcademicCalendarSchedulePreferencesReadResult::success(
            Application::AcademicCalendarScheduleJsonText(std::string(
                bytes.constData(), static_cast<std::size_t>(bytes.size()))));
    }

    [[nodiscard]] Application::AcademicCalendarSchedulePreferencesSaveResult
    write(const Application::AcademicCalendarScheduleJsonText&) const override
    {
        return Application::AcademicCalendarSchedulePreferencesSaveResult::failure(
            error(Domain::ErrorCode::Validation,
                "Class Analytics does not edit the academic calendar schedule."));
    }

private:
    SettingsRepository* m_settings = nullptr;
};

[[nodiscard]] std::u16string evaluationName(
    const Application::EvaluationPeriod period
    )
{
    return std::u16string(Application::storedEvaluationName(period));
}

struct SavedTermYear final
{
    QDate winterStart;
    std::array<int, 4> weeks{};
};

enum class ScheduleSchoolLevel
{
    Elementary,
    Middle
};

using SavedTermYears = std::map<int, SavedTermYear>;

[[nodiscard]] QDate initialWinterStart()
{
    return QDate(2025, 12, 29);
}

[[nodiscard]] std::array<int, 4> defaultWeeks(const ScheduleSchoolLevel level)
{
    return level == ScheduleSchoolLevel::Elementary
        ? std::array<int, 4>{11, 19, 11, 11}
        : std::array<int, 4>{11, 19, 4, 18};
}

[[nodiscard]] bool loadSavedTermYears(
    const QJsonObject& profiles,
    const QString& profileName,
    SavedTermYears* destination
    )
{
    if (!destination || !profiles.value(profileName).isObject())
    {
        return false;
    }
    const QJsonObject profile = profiles.value(profileName).toObject();
    for (auto entry = profile.begin(); entry != profile.end(); ++entry)
    {
        bool yearOk = false;
        const int year = entry.key().toInt(&yearOk);
        const QJsonObject schedule = entry.value().toObject();
        const QDate winterStart = QDate::fromString(
            schedule.value(QStringLiteral("winterStart")).toString(),
            Qt::ISODate);
        const QJsonArray weeks = schedule.value(QStringLiteral("weeks")).toArray();
        std::array<int, 4> weekCounts{};
        std::array<Application::AcademicTermWeekCount, 4> typedWeekCounts{};
        bool validWeeks = weeks.size() == 4;
        for (qsizetype index = 0; validWeeks && index < 4; ++index)
        {
            if (!weeks.at(index).isDouble())
            {
                validWeeks = false;
                break;
            }
            const double raw = weeks.at(index).toDouble();
            const int count = weeks.at(index).toInt(-1);
            if (raw != static_cast<double>(count))
            {
                validWeeks = false;
                break;
            }
            weekCounts[static_cast<std::size_t>(index)] = count;
            typedWeekCounts[static_cast<std::size_t>(index)] =
                Application::AcademicTermWeekCount{count};
        }
        if (!yearOk
            || !Application::isAcademicYearScheduleValid(
                Application::AcademicTermYear{year},
                winterStart.isValid() && winterStart.dayOfWeek() == Qt::Monday,
                typedWeekCounts)
            || !validWeeks)
        {
            return false;
        }
        destination->insert_or_assign(year, SavedTermYear{winterStart, weekCounts});
    }
    return true;
}

[[nodiscard]] std::optional<Application::EvaluationPeriod>
evaluationPeriodForTermIndex(const int termIndex)
{
    switch (termIndex)
    {
    case 0: return Application::EvaluationPeriod::Winter;
    case 1: return Application::EvaluationPeriod::Spring;
    case 2: return Application::EvaluationPeriod::Summer;
    case 3: return Application::EvaluationPeriod::Fall;
    default: return std::nullopt;
    }
}

[[nodiscard]] std::optional<Application::EvaluationPeriod> currentEvaluationPeriod(
    const QJsonObject& root,
    const ScheduleSchoolLevel schoolLevel,
    const QDate& referenceDate
    )
{
    if (root.value(QStringLiteral("version")).toInt() != 1
        || !root.value(QStringLiteral("profiles")).isObject()
        || !referenceDate.isValid() || referenceDate < initialWinterStart())
    {
        return std::nullopt;
    }
    const QJsonObject profiles = root.value(QStringLiteral("profiles")).toObject();
    SavedTermYears elementary;
    SavedTermYears middle;
    if (!loadSavedTermYears(profiles, QStringLiteral("elementary"), &elementary)
        || !loadSavedTermYears(profiles, QStringLiteral("middle"), &middle)
        || elementary.empty() || middle.empty())
    {
        return std::nullopt;
    }
    const SavedTermYears& custom = schoolLevel == ScheduleSchoolLevel::Elementary
        ? elementary : middle;
    SavedTermYear current{
        initialWinterStart(), defaultWeeks(schoolLevel)};
    int termYear = Application::FirstTermYear;
    if (const auto saved = custom.find(termYear); saved != custom.end())
    {
        current = saved->second;
    }

    while (true)
    {
        int totalWeeks = 0;
        for (const int count : current.weeks)
        {
            totalWeeks += count;
        }
        const QDate end = current.winterStart.addDays(totalWeeks * 7);
        if (!end.isValid())
        {
            return std::nullopt;
        }
        if (referenceDate < end)
        {
            break;
        }
        if (termYear == std::numeric_limits<int>::max())
        {
            return std::nullopt;
        }
        ++termYear;
        current = {end, defaultWeeks(schoolLevel)};
        if (const auto saved = custom.find(termYear); saved != custom.end())
        {
            current = saved->second;
        }
    }

    for (int term = 3; term >= 0; --term)
    {
        int priorWeeks = 0;
        for (int index = 0; index < term; ++index)
        {
            priorWeeks += current.weeks[static_cast<std::size_t>(index)];
        }
        const QDate termStart = current.winterStart.addDays(priorWeeks * 7);
        if (!termStart.isValid())
        {
            return std::nullopt;
        }
        if (referenceDate >= termStart)
        {
            return evaluationPeriodForTermIndex(term);
        }
    }
    return std::nullopt;
}

[[nodiscard]] std::optional<std::u16string> defaultSelection(
    WorkspaceStore& store,
    const int classId,
    const QDate& referenceDate
    )
{
    SettingsRepository* const settings = store.settingsRepository();
    ClassInfoRepository* const classInfo = store.classInfoRepository();
    SpeakingEvalRepository* const speaking = store.speakingEvalRepository();
    if (!settings || !classInfo || !speaking)
    {
        return std::nullopt;
    }

    // Preserve the legacy boundary semantics: a missing/unknown preference,
    // incomplete class-grade context, or absent/invalid saved schedule means
    // All. A settings read error also falls back to All, as the embedded legacy
    // selector does when preference/calendar services are unavailable.
    WorkspaceStoreEvaluationDefaultPolicyPort policyPort(settings);
    const auto policy = policyPort.load();
    if (!policy || policy.value()
            != Application::EvaluationDefaultPolicy::CurrentOrPreviousTerm)
    {
        return std::nullopt;
    }

    const auto grade = classInfo->loadSelectedClassGradeRecordDetailed(classId);
    if (!grade || grade->parentMissing || grade->record.classGrade.trimmed().isEmpty())
    {
        return std::nullopt;
    }

    WorkspaceStoreAcademicCalendarSchedulePreferencesPort schedulePort(settings);
    const auto scheduleSetting = schedulePort.read();
    if (!scheduleSetting || scheduleSetting.value().empty())
    {
        return std::nullopt;
    }
    const std::string& scheduleJson = scheduleSetting.value().value();
    const QByteArray scheduleBytes(scheduleJson.data(),
        static_cast<qsizetype>(scheduleJson.size()));
    if (scheduleBytes.isEmpty())
    {
        return std::nullopt;
    }
    QJsonParseError parseError;
    const QJsonDocument scheduleDocument = QJsonDocument::fromJson(
        scheduleBytes, &parseError);
    if (parseError.error != QJsonParseError::NoError
        || !scheduleDocument.isObject())
    {
        return std::nullopt;
    }
    const std::string gradeText = utf8(grade->record.classGrade.trimmed().toUpper());
    const Domain::CourseSchoolLevel courseLevel =
        Domain::courseSchoolLevelForGradeBand(
            Domain::Course::gradeBandForName(gradeText));
    const ScheduleSchoolLevel schoolLevel = courseLevel
            == Domain::CourseSchoolLevel::Middle
        ? ScheduleSchoolLevel::Middle : ScheduleSchoolLevel::Elementary;
    const std::optional<Application::EvaluationPeriod> currentPeriod =
        currentEvaluationPeriod(
        scheduleDocument.object(), schoolLevel, referenceDate);
    if (!currentPeriod)
    {
        return std::nullopt;
    }

    const QString currentName = QString::fromStdU16String(
        evaluationName(*currentPeriod));
    const auto currentEvaluation = speaking->loadSpeakingEvalDetailed(
        classId, currentName);
    if (!currentEvaluation)
    {
        return std::nullopt;
    }
    Application::EvaluationRows rows;
    rows.reserve(static_cast<std::size_t>(currentEvaluation->size()));
    for (const QStringList& sourceRow : *currentEvaluation)
    {
        std::vector<std::u16string> row;
        row.reserve(static_cast<std::size_t>(sourceRow.size()));
        for (const QString& cell : sourceRow)
        {
            row.push_back(cell.toStdU16String());
        }
        rows.push_back(std::move(row));
    }
    const auto selected = Application::selectDefaultEvaluationPeriod(
        Application::EvaluationDefaultPolicy::CurrentOrPreviousTerm,
        *currentPeriod, rows);
    return selected ? std::optional<std::u16string>(evaluationName(*selected))
                    : std::nullopt;
}

class WorkspaceStoreClassAnalyticsReadPort final
    : public Application::ClassAnalyticsDashboardReadPort
    , public Application::ClassAnalyticsNameSemanticsPort
{
public:
    explicit WorkspaceStoreClassAnalyticsReadPort(WorkspaceStore& store) noexcept
        : m_store(store)
    {
    }

    [[nodiscard]] Domain::Result<Application::ClassAnalyticsRosterNames>
    readRosterNames(const Domain::ClassId& classId) const override
    {
        const auto parsedId = legacyClassId(classId);
        RosterRepository* const repository = m_store.rosterRepository();
        if (!parsedId || !repository)
        {
            return Domain::Result<Application::ClassAnalyticsRosterNames>::failure(
                error(parsedId ? Domain::ErrorCode::NotFound
                               : Domain::ErrorCode::InvalidInput,
                    "The class roster could not be loaded."));
        }

        try
        {
            Application::ClassAnalyticsRosterNames result;
            const RepositoryStatus loaded = repository->forEachClassAnalyticsRosterNameCell(
                *parsedId,
                [&](const RosterRepository::ClassAnalyticsRosterNamesMetadata& metadata)
                {
                    result.rowCount = Application::ClassAnalyticsRosterRowCount{
                        metadata.rowCount};
                    result.hasEnglishColumn = metadata.hasEnglishColumn;
                    result.hasKoreanColumn = metadata.hasKoreanColumn;
                },
                [&](const RosterRepository::ClassAnalyticsRosterNameCell& cell)
                {
                    auto& names = cell.isEnglishName
                        ? result.englishNames : result.koreanNames;
                    names.push_back(cell.value.toStdU16String());
                });
            if (!loaded)
            {
                return Domain::Result<Application::ClassAnalyticsRosterNames>::failure(
                    error(repositoryDomainErrorCode(loaded.error().code),
                        loaded.error().diagnostic.toStdString()));
            }
            return Domain::Result<Application::ClassAnalyticsRosterNames>::success(
                std::move(result));
        }
        catch (const std::exception&)
        {
            return Domain::Result<Application::ClassAnalyticsRosterNames>::failure(
                error(Domain::ErrorCode::Technical,
                    "Class Analytics roster names could not be loaded."));
        }
    }

    [[nodiscard]] Domain::Result<Application::ClassAnalyticsEvaluationBatch>
    readEvaluationBatch(const Domain::ClassId& classId) const override
    {
        const auto parsedId = legacyClassId(classId);
        SpeakingEvalRepository* const repository = m_store.speakingEvalRepository();
        if (!parsedId || !repository)
        {
            return Domain::Result<Application::ClassAnalyticsEvaluationBatch>::failure(
                error(parsedId ? Domain::ErrorCode::NotFound
                               : Domain::ErrorCode::InvalidInput,
                    "Class Analytics evaluations could not be loaded."));
        }

        try
        {
            std::array<QString, Application::kClassAnalyticsEvaluationNames.size()>
                names;
            for (std::size_t index = 0; index < names.size(); ++index)
            {
                names[index] = QString::fromStdU16String(std::u16string(
                    Application::kClassAnalyticsEvaluationNames[index]));
            }
            Application::ClassAnalyticsEvaluationBatch batch;
            const RepositoryStatus loaded = repository->forEachClassAnalyticsEvaluationRow(
                *parsedId, names,
                [&](const SpeakingEvalRepository::ClassAnalyticsEvaluationRow& source)
                {
                    Application::ClassAnalyticsEvaluationRow row;
                    row.englishName = source.englishName.toStdU16String();
                    row.koreanName = source.koreanName.toStdU16String();
                    for (std::size_t index = 0; index < row.scores.size(); ++index)
                    {
                        row.scores[index] = source.scores[index].toStdU16String();
                    }
                    batch[source.evaluationIndex].push_back(std::move(row));
                });
            if (!loaded)
            {
                return Domain::Result<Application::ClassAnalyticsEvaluationBatch>::failure(
                    error(repositoryDomainErrorCode(loaded.error().code),
                        loaded.error().diagnostic.toStdString()));
            }
            return Domain::Result<Application::ClassAnalyticsEvaluationBatch>::success(
                std::move(batch));
        }
        catch (const std::exception&)
        {
            return Domain::Result<Application::ClassAnalyticsEvaluationBatch>::failure(
                error(Domain::ErrorCode::Technical,
                    "Class Analytics evaluations could not be loaded."));
        }
    }

    [[nodiscard]] std::u16string trimmed(
        const std::u16string_view value) const override
    {
        return QString::fromStdU16String(std::u16string(value))
            .trimmed().toStdU16String();
    }

    [[nodiscard]] std::u16string normalizedEnglishIdentity(
        const std::u16string_view value) const override
    {
        return StudentNameUtils::normalizeEnglishName(
            QString::fromStdU16String(std::u16string(value)))
            .toCaseFolded().toStdU16String();
    }

    [[nodiscard]] std::u16string baseKoreanName(
        const std::u16string_view value) const override
    {
        return StudentNameUtils::baseKoreanName(
            QString::fromStdU16String(std::u16string(value))).toStdU16String();
    }

    [[nodiscard]] bool equalsCaseInsensitive(
        const std::u16string_view left,
        const std::u16string_view right) const override
    {
        return QString::compare(
            QString::fromStdU16String(std::u16string(left)),
            QString::fromStdU16String(std::u16string(right)),
            Qt::CaseInsensitive) == 0;
    }

    [[nodiscard]] int compareEnglishNames(
        const std::u16string_view left,
        const std::u16string_view right) const override
    {
        return QString::localeAwareCompare(
            QString::fromStdU16String(std::u16string(left)),
            QString::fromStdU16String(std::u16string(right)));
    }

private:
    WorkspaceStore& m_store;
};

} // namespace

Application::ClassAnalyticsPageReadResult readClassAnalyticsAtPath(
    const QString& databasePath,
    const Application::ClassAnalyticsPageReadRequest& request
    )
{
    return readClassAnalyticsAtPathForDate(
        databasePath, request, QDate::currentDate());
}

Application::ClassAnalyticsPageReadResult readClassAnalyticsAtPathForDate(
    const QString& databasePath,
    const Application::ClassAnalyticsPageReadRequest& request,
    const QDate& referenceDate
    )
{
    const auto parsedId = legacyClassId(request.classId);
    if (!parsedId)
    {
        return Application::ClassAnalyticsPageReadResult::failure(error(
            Domain::ErrorCode::InvalidInput,
            "Class ID must be a canonical positive integer."));
    }

    WorkspaceStore store;
    const RepositoryStatus opened = store.openExisting(databasePath);
    if (!opened)
    {
        return Application::ClassAnalyticsPageReadResult::failure(error(
            repositoryDomainErrorCode(opened.error().code),
            opened.error().diagnostic.toStdString()));
    }

    std::optional<std::u16string> selectedEvaluation =
        request.selectedEvaluationName;
    if (request.useDefaultSelection)
    {
        selectedEvaluation = defaultSelection(store, *parsedId, referenceDate);
    }

    WorkspaceStoreClassAnalyticsReadPort readPort(store);
    const Application::ClassAnalyticsEvaluationSelection selection =
        selectedEvaluation
            ? Application::ClassAnalyticsEvaluationSelection::exactName(
                *selectedEvaluation)
            : Application::ClassAnalyticsEvaluationSelection::all();
    const Application::ClassAnalyticsDashboardReadRequest dashboardRequest{
        .classId = request.classId,
        .evaluationSelection = selection
    };
    try
    {
        Application::ClassAnalyticsDashboardResult dashboard =
            Application::ClassAnalyticsDashboardReadQuery::execute(
                dashboardRequest, readPort, readPort);
        if (!dashboard)
        {
            return Application::ClassAnalyticsPageReadResult::failure(
                dashboard.error());
        }
        return Application::ClassAnalyticsPageReadResult::success({
            .classId = request.classId,
            .dashboard = std::move(dashboard.value()),
            .selectedEvaluationName = std::move(selectedEvaluation)
        });
    }
    catch (const std::exception&)
    {
        return Application::ClassAnalyticsPageReadResult::failure(error(
            Domain::ErrorCode::Technical,
            "Class Analytics could not be loaded."));
    }
    catch (...)
    {
        return Application::ClassAnalyticsPageReadResult::failure(error(
            Domain::ErrorCode::Technical,
            "Class Analytics could not be loaded."));
    }
}

} // namespace ClassMngr::Next::Platform
