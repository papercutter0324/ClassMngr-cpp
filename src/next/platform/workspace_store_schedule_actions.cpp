#include "next/platform/workspace_store_schedule_actions.h"

#include "data/database/workspace_store.h"
#include "data/repositories/class_info_repository.h"
#include "data/repositories/intensive_slot_state_repository.h"
#include "data/repositories/settings_repository.h"
#include "data/repositories/testing_block_repository.h"
#include "next/application/schedule_slot_state_save_use_case.h"
#include "next/application/schedule_testing_assignment_save_use_case.h"
#include "next/application/positive_decimal_id_validation.h"
#include "next/platform/repository_error_code.h"

#include <QByteArray>
#include <QDebug>
#include <QVariant>

#include <algorithm>
#include <exception>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace ClassMngr::Next::Platform
{
namespace
{

using namespace Application;

inline constexpr std::size_t kScheduleSourceMaximumClasses = 4'096;
inline constexpr std::size_t kScheduleSourceMaximumRowsPerClass = 64;
inline constexpr std::size_t kScheduleSourceMaximumRows = 32'768;
inline constexpr std::size_t kScheduleAuxiliaryMaximumRows = 32'768;
inline constexpr std::size_t kScheduleMaximumShortText = 256;
inline constexpr std::size_t kScheduleMaximumToken = 64;

[[nodiscard]] std::string utf8(const QString& value)
{
    const QByteArray bytes = value.toUtf8();
    return {bytes.constData(), static_cast<std::size_t>(bytes.size())};
}

[[nodiscard]] Domain::OperationError operationError(
    const Domain::ErrorCode code,
    std::string message,
    const bool recoverable
    )
{
    return {.code = code, .message = std::move(message), .recoverable = recoverable};
}

[[nodiscard]] Domain::OperationError repositoryError(
    const RepositoryError& source
    )
{
    const QString diagnostic = toLegacyDiagnostic(source);
    const std::string message = utf8(diagnostic);
    return operationError(
        repositoryDomainErrorCode(source.code),
        message.empty() ? "Schedule workspace data is unavailable." : message,
        source.recoverableOverride.value_or(
            source.code != RepositoryErrorCode::Technical)
        );
}

template <typename Result>
[[nodiscard]] Result openFailure(const RepositoryError& source)
{
    return Result::failure(repositoryError(source));
}

[[nodiscard]] RepositoryStatus boundedInputFailure(const QString& message)
{
    return std::unexpected(RepositoryError(
        RepositoryErrorCode::Validation,
        message,
        false
        ));
}

[[nodiscard]] bool isBoundedText(
    const QString& value,
    const std::size_t maximum,
    const bool allowEmpty = true
    )
{
    const std::size_t size = static_cast<std::size_t>(value.size());
    return size <= maximum && (allowEmpty || !value.trimmed().isEmpty());
}

[[nodiscard]] Domain::Result<bool> loadBooleanSetting(
    SettingsRepository& repository,
    const QString& key
    )
{
    const auto stored = repository.loadSetting(key);
    if (!stored)
    {
        return Domain::Result<bool>::failure(repositoryError(stored.error()));
    }
    if (!stored->isValid())
    {
        return Domain::Result<bool>::success(false);
    }

    const QString text = stored->toString().trimmed().toLower();
    if (text == QStringLiteral("true") || text == QStringLiteral("1"))
    {
        return Domain::Result<bool>::success(true);
    }
    if (text == QStringLiteral("false") || text == QStringLiteral("0"))
    {
        return Domain::Result<bool>::success(false);
    }
    return Domain::Result<bool>::success(stored->toBool());
}

[[nodiscard]] QString modeSettingValue(const ScheduleDisplayMode mode)
{
    switch (mode)
    {
    case ScheduleDisplayMode::Regular:
        return QStringLiteral("regular");
    case ScheduleDisplayMode::Intensive:
        return QStringLiteral("intensive");
    case ScheduleDisplayMode::Testing:
        return QStringLiteral("testing");
    }
    return {};
}

[[nodiscard]] std::optional<ScheduleDisplayMode> displayModeFromSetting(
    const QVariant& value
    )
{
    const QString normalized = value.toString().trimmed().toLower();
    if (normalized == QStringLiteral("regular"))
    {
        return ScheduleDisplayMode::Regular;
    }
    if (normalized == QStringLiteral("intensive"))
    {
        return ScheduleDisplayMode::Intensive;
    }
    if (normalized == QStringLiteral("testing"))
    {
        return ScheduleDisplayMode::Testing;
    }
    return std::nullopt;
}

[[nodiscard]] Domain::Result<ScheduleDisplayPreferences> loadDisplayPreferences(
    SettingsRepository& repository
    )
{
    const auto use24Hour = loadBooleanSetting(
        repository, QStringLiteral("schedule_use_24h"));
    if (!use24Hour)
    {
        return Domain::Result<ScheduleDisplayPreferences>::failure(
            use24Hour.error());
    }
    const auto showEnglishNames = loadBooleanSetting(
        repository,
        QStringLiteral("schedule_show_korean_teacher_english_names"));
    if (!showEnglishNames)
    {
        return Domain::Result<ScheduleDisplayPreferences>::failure(
            showEnglishNames.error());
    }
    const auto showWeekends = loadBooleanSetting(
        repository, QStringLiteral("schedule_show_weekends"));
    if (!showWeekends)
    {
        return Domain::Result<ScheduleDisplayPreferences>::failure(
            showWeekends.error());
    }
    const auto showAllIntensiveHours = loadBooleanSetting(
        repository, QStringLiteral("schedule_show_all_hours_v2"));
    if (!showAllIntensiveHours)
    {
        return Domain::Result<ScheduleDisplayPreferences>::failure(
            showAllIntensiveHours.error());
    }
    const auto testingAffectsM1 = loadBooleanSetting(
        repository, QStringLiteral("schedule_testing_affects_m1"));
    if (!testingAffectsM1)
    {
        return Domain::Result<ScheduleDisplayPreferences>::failure(
            testingAffectsM1.error());
    }

    return Domain::Result<ScheduleDisplayPreferences>::success({
        .use24HourTime = use24Hour.value(),
        .showEnglishNames = showEnglishNames.value(),
        .showWeekends = showWeekends.value(),
        .showAllIntensiveHours = showAllIntensiveHours.value(),
        .testingAffectsM1 = testingAffectsM1.value()
    });
}

[[nodiscard]] Domain::Result<ScheduleDisplayMode> loadDisplayMode(
    SettingsRepository& repository
    )
{
    const auto storedMode = repository.loadSetting(
        QStringLiteral("schedule_display_mode"));
    if (!storedMode)
    {
        return Domain::Result<ScheduleDisplayMode>::failure(
            repositoryError(storedMode.error()));
    }

    if (const auto parsed = displayModeFromSetting(storedMode.value()))
    {
        return Domain::Result<ScheduleDisplayMode>::success(*parsed);
    }

    const auto legacyMode = repository.loadSetting(
        QStringLiteral("schedule_show_intensive"));
    if (!legacyMode)
    {
        return Domain::Result<ScheduleDisplayMode>::failure(
            repositoryError(legacyMode.error()));
    }
    const QString legacyText = legacyMode->toString().trimmed().toLower();
    const bool useIntensive = legacyMode->isValid()
        && (legacyText == QStringLiteral("true")
            || legacyText == QStringLiteral("1")
            || (legacyText != QStringLiteral("false")
                && legacyText != QStringLiteral("0")
                && legacyMode->toBool()));
    const ScheduleDisplayMode mode = useIntensive
        ? ScheduleDisplayMode::Intensive
        : ScheduleDisplayMode::Regular;

    if (!storedMode->isValid())
    {
        const RepositoryStatus migrated = repository.saveSetting(
            QStringLiteral("schedule_display_mode"), modeSettingValue(mode));
        if (!migrated)
        {
            qWarning("Could not migrate the stored schedule display mode.");
        }
    }
    return Domain::Result<ScheduleDisplayMode>::success(mode);
}

class WorkspaceStoreSchedulePageReadPort final
    : public SchedulePageReadPort
{
public:
    explicit WorkspaceStoreSchedulePageReadPort(WorkspaceStore& store) noexcept
        : m_store(&store)
    {
    }

    [[nodiscard]] ScheduleBuilderSourceResult readScheduleClasses(
        const ScheduleBuilderSourceQuery&
        ) const override
    {
        if (!m_store || !m_store->isOpen())
        {
            return unavailable<ScheduleBuilderSourceSnapshot>();
        }
        ClassInfoRepository* const repository = m_store->classInfoRepository();
        if (!repository)
        {
            return unavailable<ScheduleBuilderSourceSnapshot>();
        }

        try
        {
            ScheduleBuilderSourceSnapshot snapshot;
            std::unordered_map<int, std::size_t> indexesByClassId;
            std::vector<std::size_t> regularRows;
            std::vector<std::size_t> intensiveRows;
            std::size_t totalRows = 0;
            RepositoryStatus loaded = repository->visitScheduleClassInfoRows(
                [&](const ClassInfo& info) -> RepositoryStatus
                {
                    if (snapshot.classes.size() >= kScheduleSourceMaximumClasses)
                    {
                        return boundedInputFailure(QStringLiteral(
                            "The schedule has more classes than the supported 4,096 class limit."));
                    }
                    if (info.classId <= 0 || indexesByClassId.contains(info.classId))
                    {
                        return boundedInputFailure(QStringLiteral(
                            "The schedule source contains an invalid or duplicate class ID."));
                    }
                    if (!isBoundedText(info.teacherKr, kScheduleMaximumShortText)
                        || !isBoundedText(info.teacherEn, kScheduleMaximumShortText)
                        || !isBoundedText(info.teacherPreferredName, kScheduleMaximumShortText)
                        || !isBoundedText(info.roomNumber, kScheduleMaximumShortText)
                        || !isBoundedText(info.classGrade, kScheduleMaximumToken)
                        || !isBoundedText(info.classLevel, kScheduleMaximumShortText)
                        || !isBoundedText(info.classColor, 64)
                        || !isBoundedText(info.fontColor, 64))
                    {
                        return boundedInputFailure(QStringLiteral(
                            "Schedule class text exceeds the supported display limit."));
                    }

                    const auto classId = Domain::ClassId::fromString(
                        std::to_string(info.classId));
                    if (!classId)
                    {
                        return boundedInputFailure(QStringLiteral(
                            "The schedule source contains an invalid class ID."));
                    }
                    const std::size_t index = snapshot.classes.size();
                    indexesByClassId.emplace(info.classId, index);
                    snapshot.classes.push_back({
                        .classId = *classId,
                        .teacherKoreanName = info.teacherKr.toStdU16String(),
                        .teacherEnglishName = info.teacherEn.toStdU16String(),
                        .teacherPreferredName = info.teacherPreferredName.toStdU16String(),
                        .roomNumber = info.roomNumber.toStdU16String(),
                        .grade = info.classGrade.toStdU16String(),
                        .level = info.classLevel.toStdU16String(),
                        .classColor = info.classColor.toStdU16String(),
                        .fontColor = info.fontColor.toStdU16String()
                    });
                    regularRows.push_back(0);
                    intensiveRows.push_back(0);
                    return {};
                },
                [&](const int classId, const ClassTime& time, const bool intensive)
                    -> RepositoryStatus
                {
                    const auto index = indexesByClassId.find(classId);
                    if (index == indexesByClassId.end())
                    {
                        return boundedInputFailure(QStringLiteral(
                            "A schedule row references a class that was not loaded."));
                    }
                    if (!isBoundedText(time.day, kScheduleMaximumToken)
                        || !isBoundedText(time.startTime, kScheduleMaximumToken)
                        || !isBoundedText(time.endTime, kScheduleMaximumToken))
                    {
                        return boundedInputFailure(QStringLiteral(
                            "A schedule row contains text above the supported token limit."));
                    }

                    std::vector<std::size_t>& counts = intensive
                        ? intensiveRows
                        : regularRows;
                    if (counts[index->second] >= kScheduleSourceMaximumRowsPerClass)
                    {
                        return boundedInputFailure(QStringLiteral(
                            "A class exceeds the supported 64 schedule rows per mode."));
                    }
                    if (totalRows >= kScheduleSourceMaximumRows)
                    {
                        return boundedInputFailure(QStringLiteral(
                            "The schedule exceeds the supported 32,768 meeting rows."));
                    }

                    ++counts[index->second];
                    ++totalRows;
                    auto& rows = intensive
                        ? snapshot.classes[index->second].intensiveSchedule
                        : snapshot.classes[index->second].regularSchedule;
                    rows.push_back({
                        .day = ScheduleBuilderSourceScheduleText(
                            time.day.toStdU16String()),
                        .startTime = ScheduleBuilderSourceScheduleText(
                            time.startTime.toStdU16String()),
                        .endTime = ScheduleBuilderSourceScheduleText(
                            time.endTime.toStdU16String())
                    });
                    return {};
                });
            if (!loaded)
            {
                return ScheduleBuilderSourceResult::failure(
                    repositoryError(loaded.error()));
            }
            return ScheduleBuilderSourceResult::success(std::move(snapshot));
        }
        catch (const std::exception&)
        {
            return failure<ScheduleBuilderSourceSnapshot>(
                Domain::ErrorCode::Technical,
                "Schedule class data could not be loaded.", false);
        }
        catch (...)
        {
            return failure<ScheduleBuilderSourceSnapshot>(
                Domain::ErrorCode::Technical,
                "Schedule class data could not be loaded.", false);
        }
    }

    [[nodiscard]] ScheduleDisplayPreferencesResult
    readDisplayPreferences() const override
    {
        SettingsRepository* const repository =
            m_store && m_store->isOpen() ? m_store->settingsRepository() : nullptr;
        if (!repository)
        {
            return unavailable<ScheduleDisplayPreferences>();
        }
        try
        {
            const auto result = loadDisplayPreferences(*repository);
            return result
                ? ScheduleDisplayPreferencesResult::success(result.value())
                : ScheduleDisplayPreferencesResult::failure(result.error());
        }
        catch (...)
        {
            return failure<ScheduleDisplayPreferences>(
                Domain::ErrorCode::Technical,
                "Schedule display preferences could not be loaded.", false);
        }
    }

    [[nodiscard]] ScheduleDisplayModePreferencesLoadResult
    readDisplayMode() const override
    {
        SettingsRepository* const repository =
            m_store && m_store->isOpen() ? m_store->settingsRepository() : nullptr;
        if (!repository)
        {
            return unavailable<ScheduleDisplayMode>();
        }
        try
        {
            const auto result = loadDisplayMode(*repository);
            return result
                ? ScheduleDisplayModePreferencesLoadResult::success(result.value())
                : ScheduleDisplayModePreferencesLoadResult::failure(result.error());
        }
        catch (...)
        {
            return failure<ScheduleDisplayMode>(
                Domain::ErrorCode::Technical,
                "Schedule display mode could not be loaded.", false);
        }
    }

    [[nodiscard]] ScheduleSlotStateReadResult readSlotStates(
        const ScheduleSlotStateReadQuery&
        ) const override
    {
        IntensiveSlotStateRepository* const repository =
            m_store && m_store->isOpen() ? m_store->intensiveSlotStateRepository() : nullptr;
        if (!repository)
        {
            return unavailable<ScheduleSlotStateReadSnapshot>();
        }
        try
        {
            const auto loaded = repository->loadIntensiveSlotStates();
            if (!loaded)
            {
                return ScheduleSlotStateReadResult::failure(
                    repositoryError(loaded.error()));
            }
            if (static_cast<std::size_t>(loaded->size())
                > kScheduleAuxiliaryMaximumRows)
            {
                return failure<ScheduleSlotStateReadSnapshot>(
                    Domain::ErrorCode::Validation,
                    "Stored schedule slot states exceed the supported 32,768 row limit.",
                    false);
            }

            ScheduleSlotStateReadSnapshot snapshot;
            snapshot.rows.reserve(static_cast<std::size_t>(loaded->size()));
            for (const IntensiveSlotState& state : loaded.value())
            {
                if (!isBoundedText(state.day, kScheduleMaximumToken, false)
                    || !isBoundedText(state.startTime, kScheduleMaximumToken, false)
                    || !isBoundedText(state.state, kScheduleMaximumToken, false))
                {
                    return failure<ScheduleSlotStateReadSnapshot>(
                        Domain::ErrorCode::Validation,
                        "Stored schedule slot state text exceeds the supported limit.",
                        false);
                }
                snapshot.rows.push_back({
                    .identity = ScheduleSlotStateReadIdentity(
                        state.day.toStdU16String(), state.startTime.toStdU16String()),
                    .state = state.state.toStdU16String()
                });
            }
            return ScheduleSlotStateReadResult::success(std::move(snapshot));
        }
        catch (...)
        {
            return failure<ScheduleSlotStateReadSnapshot>(
                Domain::ErrorCode::Technical,
                "Schedule slot states could not be loaded.", false);
        }
    }

    [[nodiscard]] ScheduleTestingAssignmentReadResult readTestingAssignments(
        const ScheduleTestingAssignmentReadQuery&
        ) const override
    {
        TestingBlockRepository* const repository =
            m_store && m_store->isOpen() ? m_store->testingBlockRepository() : nullptr;
        if (!repository)
        {
            return unavailable<ScheduleTestingAssignmentReadSnapshot>();
        }
        try
        {
            const auto loaded = repository->loadTestingAssignmentDisplayRecords();
            if (!loaded)
            {
                return ScheduleTestingAssignmentReadResult::failure(
                    repositoryError(loaded.error()));
            }
            if (static_cast<std::size_t>(loaded->size())
                > kScheduleAuxiliaryMaximumRows)
            {
                return failure<ScheduleTestingAssignmentReadSnapshot>(
                    Domain::ErrorCode::Validation,
                    "Stored testing assignments exceed the supported 32,768 row limit.",
                    false);
            }

            ScheduleTestingAssignmentReadSnapshot snapshot;
            snapshot.rows.reserve(static_cast<std::size_t>(loaded->size()));
            for (const TestingAssignmentDisplayRecord& row : loaded.value())
            {
                if (!isBoundedText(row.day, kScheduleMaximumToken, false)
                    || !isBoundedText(row.startTime, kScheduleMaximumToken, false)
                    || !isBoundedText(row.room, kScheduleMaximumShortText)
                    || !isBoundedText(row.className, kScheduleMaximumShortText)
                    || !isBoundedText(row.teacherKoreanName, kScheduleMaximumShortText)
                    || !isBoundedText(row.teacherEnglishName, kScheduleMaximumShortText)
                    || !isBoundedText(row.teacherPreferredName, kScheduleMaximumShortText)
                    || !isBoundedText(row.testingClassRoom, kScheduleMaximumShortText)
                    || !isBoundedText(row.grade, kScheduleMaximumToken)
                    || !isBoundedText(row.level, kScheduleMaximumShortText)
                    || !isBoundedText(row.classColor, 64)
                    || !isBoundedText(row.fontColor, 64))
                {
                    return failure<ScheduleTestingAssignmentReadSnapshot>(
                        Domain::ErrorCode::Validation,
                        "Testing assignment text exceeds the supported display limit.",
                        false);
                }
                const auto classId = Domain::ClassId::fromString(
                    std::to_string(row.classId));
                if (!classId)
                {
                    return failure<ScheduleTestingAssignmentReadSnapshot>(
                        Domain::ErrorCode::Validation,
                        "A testing assignment contains an invalid class ID.", false);
                }
                ScheduleTestingAssignmentReadRow output{
                    .assignmentKey = ScheduleTestingAssignmentKey(
                        row.day.toStdU16String(), row.startTime.toStdU16String()),
                    .room = row.room.toStdU16String(),
                    .classId = *classId
                };
                if (row.classId > 0 && row.hasSpecialClass)
                {
                    output.specialClass = ScheduleTestingAssignmentSpecialClass{
                        .name = row.className.toStdU16String(),
                        .teacherKoreanName = row.teacherKoreanName.toStdU16String(),
                        .teacherEnglishName = row.teacherEnglishName.toStdU16String(),
                        .teacherPreferredName = row.teacherPreferredName.toStdU16String(),
                        .room = row.testingClassRoom.toStdU16String(),
                        .grade = row.grade.toStdU16String(),
                        .level = row.level.toStdU16String(),
                        .classColor = row.classColor.toStdU16String(),
                        .fontColor = row.fontColor.toStdU16String()
                    };
                }
                snapshot.rows.push_back(std::move(output));
            }
            return ScheduleTestingAssignmentReadResult::success(std::move(snapshot));
        }
        catch (...)
        {
            return failure<ScheduleTestingAssignmentReadSnapshot>(
                Domain::ErrorCode::Technical,
                "Testing assignments could not be loaded.", false);
        }
    }

private:
    template <typename Value>
    [[nodiscard]] static Domain::Result<Value> failure(
        const Domain::ErrorCode code,
        std::string message,
        const bool recoverable
        )
    {
        return Domain::Result<Value>::failure(
            operationError(code, std::move(message), recoverable));
    }

    template <typename Value>
    [[nodiscard]] Domain::Result<Value> unavailable() const
    {
        return failure<Value>(
            Domain::ErrorCode::NotFound,
            "The selected workspace is unavailable for Schedule.", true);
    }

    WorkspaceStore* m_store = nullptr;
};

class WorkspaceStoreScheduleTestingAssignmentSavePort final
    : public ScheduleTestingAssignmentSavePort
{
public:
    explicit WorkspaceStoreScheduleTestingAssignmentSavePort(
        WorkspaceStore& store
        ) noexcept
        : m_store(&store)
    {
    }

    [[nodiscard]] ScheduleTestingAssignmentSaveResult saveTestingAssignment(
        const ScheduleTestingAssignmentSaveRequest& request
        ) const override
    {
        if (!m_store || !m_store->isOpen())
        {
            return ScheduleTestingAssignmentSaveResult::failure(operationError(
                Domain::ErrorCode::NotFound,
                "The selected workspace is unavailable for Schedule.", true));
        }
        TestingBlockRepository* const repository =
            m_store->testingBlockRepository();
        if (!repository)
        {
            return ScheduleTestingAssignmentSaveResult::failure(operationError(
                Domain::ErrorCode::NotFound,
                "The selected workspace has no testing assignment repository.",
                true));
        }

        const QString day = QString::fromStdU16String(request.day);
        const QString startTime = QString::fromStdU16String(request.startTime);
        RepositoryStatus saved;
        try
        {
            switch (request.mutation)
            {
            case ScheduleTestingAssignmentMutation::RemoveAssignment:
                saved = repository->deleteTestingAssignment(day, startTime);
                break;
            case ScheduleTestingAssignmentMutation::SavePlainTesting:
                saved = repository->saveTestingBlock(
                    day, startTime, QString::fromStdU16String(request.room),
                    request.replaceExisting);
                break;
            case ScheduleTestingAssignmentMutation::AssignTestingClass:
            {
                const auto parsedId = request.classId
                    ? parseCanonicalPositiveDecimalId(request.classId->value())
                    : std::nullopt;
                if (!parsedId)
                {
                    return ScheduleTestingAssignmentSaveResult::failure(
                        operationError(Domain::ErrorCode::InvalidInput,
                            "A valid testing class is required.", true));
                }
                saved = repository->assignTestingClass(
                    day, startTime, *parsedId, request.replaceExisting);
                break;
            }
            }
        }
        catch (const std::exception&)
        {
            return ScheduleTestingAssignmentSaveResult::failure(
                operationError(Domain::ErrorCode::Technical,
                    "The testing assignment could not be saved.", false));
        }
        catch (...)
        {
            return ScheduleTestingAssignmentSaveResult::failure(
                operationError(Domain::ErrorCode::Technical,
                    "The testing assignment could not be saved.", false));
        }
        return saved
            ? ScheduleTestingAssignmentSaveResult::success()
            : ScheduleTestingAssignmentSaveResult::failure(
                repositoryError(saved.error()));
    }

private:
    WorkspaceStore* m_store = nullptr;
};

[[nodiscard]] bool weekdayCaseInsensitiveEquals(
    const std::u16string_view left,
    const std::u16string_view right
    ) noexcept
{
    if (left.size() != right.size())
    {
        return false;
    }
    for (std::size_t index = 0; index < left.size(); ++index)
    {
        const auto lower = [](const char16_t value) {
            return value >= u'A' && value <= u'Z'
                ? static_cast<char16_t>(value + (u'a' - u'A'))
                : value;
        };
        if (lower(left[index]) != lower(right[index]))
        {
            return false;
        }
    }
    return true;
}

[[nodiscard]] Domain::Result<void> saveSettings(
    WorkspaceStore& store,
    const QVariantMap& settings
    )
{
    SettingsRepository* const repository = store.settingsRepository();
    if (!repository)
    {
        return Domain::Result<void>::failure(operationError(
            Domain::ErrorCode::NotFound,
            "Schedule settings are unavailable.", true));
    }
    const RepositoryStatus saved = repository->saveSettings(settings);
    if (!saved)
    {
        return Domain::Result<void>::failure(repositoryError(saved.error()));
    }
    return Domain::Result<void>::success();
}

} // namespace

SchedulePageReadResult readSchedulePageFromStore(WorkspaceStore& store)
{
    const WorkspaceStoreSchedulePageReadPort port(store);
    return SchedulePageReadUseCase::execute(port);
}

SchedulePageReadResult readSchedulePageAtPath(const QString& databasePath)
{
    WorkspaceStore store;
    const RepositoryStatus opened = store.openExisting(databasePath);
    if (!opened)
    {
        return openFailure<SchedulePageReadResult>(opened.error());
    }
    return readSchedulePageFromStore(store);
}

ScheduleDisplayPreferencesSaveResult saveScheduleDisplayPreferencesAtPath(
    const QString& databasePath,
    const ScheduleDisplayPreferencesSaveRequest& request
    )
{
    WorkspaceStore store;
    const RepositoryStatus opened = store.openExisting(databasePath);
    if (!opened)
    {
        return openFailure<ScheduleDisplayPreferencesSaveResult>(opened.error());
    }
    const QVariantMap settings{
        {QStringLiteral("schedule_use_24h"), request.use24HourTime},
        {QStringLiteral("schedule_show_korean_teacher_english_names"), request.showEnglishNames},
        {QStringLiteral("schedule_show_weekends"), request.showWeekends},
        {QStringLiteral("schedule_show_all_hours_v2"), request.showAllIntensiveHours},
        {QStringLiteral("schedule_testing_affects_m1"), request.testingAffectsM1}
    };
    const auto saved = saveSettings(store, settings);
    return saved
        ? ScheduleDisplayPreferencesSaveResult::success()
        : ScheduleDisplayPreferencesSaveResult::failure(saved.error());
}

ScheduleDisplayModePreferencesSaveResult saveScheduleDisplayModeAtPath(
    const QString& databasePath,
    const ScheduleDisplayMode mode
    )
{
    const QString value = modeSettingValue(mode);
    if (value.isEmpty())
    {
        return ScheduleDisplayModePreferencesSaveResult::failure(operationError(
            Domain::ErrorCode::InvalidInput,
            "Schedule display mode is invalid.", false));
    }
    WorkspaceStore store;
    const RepositoryStatus opened = store.openExisting(databasePath);
    if (!opened)
    {
        return openFailure<ScheduleDisplayModePreferencesSaveResult>(opened.error());
    }
    const auto saved = saveSettings(store, {
        {QStringLiteral("schedule_display_mode"), value}
    });
    return saved
        ? ScheduleDisplayModePreferencesSaveResult::success()
        : ScheduleDisplayModePreferencesSaveResult::failure(saved.error());
}

ScheduleSlotStateSaveResult saveScheduleSlotStateAtPath(
    const QString& databasePath,
    const ScheduleSlotStateSaveRequest& request
    )
{
    const Domain::Result<void> validation = request.validate();
    if (!validation)
    {
        return ScheduleSlotStateSaveResult::failure(validation.error());
    }
    WorkspaceStore store;
    const RepositoryStatus opened = store.openExisting(databasePath);
    if (!opened)
    {
        return openFailure<ScheduleSlotStateSaveResult>(opened.error());
    }
    IntensiveSlotStateRepository* const repository =
        store.intensiveSlotStateRepository();
    if (!repository)
    {
        return ScheduleSlotStateSaveResult::failure(operationError(
            Domain::ErrorCode::NotFound,
            "The selected workspace has no schedule slot-state repository.", true));
    }

    const auto weekdayText = [](const ScheduleWeekday weekday) -> QString
    {
        switch (weekday)
        {
        case ScheduleWeekday::Monday: return QStringLiteral("Monday");
        case ScheduleWeekday::Tuesday: return QStringLiteral("Tuesday");
        case ScheduleWeekday::Wednesday: return QStringLiteral("Wednesday");
        case ScheduleWeekday::Thursday: return QStringLiteral("Thursday");
        case ScheduleWeekday::Friday: return QStringLiteral("Friday");
        case ScheduleWeekday::Saturday: return QStringLiteral("Saturday");
        case ScheduleWeekday::Sunday: return QStringLiteral("Sunday");
        }
        return {};
    };
    const auto stateText = [](const ScheduleSlotState state) -> QString
    {
        switch (state)
        {
        case ScheduleSlotState::Empty: return QStringLiteral("empty");
        case ScheduleSlotState::Essay: return QStringLiteral("essay");
        case ScheduleSlotState::Lunch: return QStringLiteral("lunch");
        }
        return {};
    };
    const int minute = request.startMinute.value();
    const QString time = QStringLiteral("%1:%2")
        .arg(minute / 60, 2, 10, QLatin1Char('0'))
        .arg(minute % 60, 2, 10, QLatin1Char('0'));
    const RepositoryStatus saved = repository->saveIntensiveSlotState(
        weekdayText(request.weekday),
        time,
        stateText(request.selectedState),
        stateText(request.defaultState));
    return saved
        ? ScheduleSlotStateSaveResult::success()
        : ScheduleSlotStateSaveResult::failure(repositoryError(saved.error()));
}

Application::ScheduleTestingAssignmentSaveResult
saveScheduleTestingAssignmentAtPath(
    const QString& databasePath,
    const Application::ScheduleTestingAssignmentSaveRequest& request
    )
{
    WorkspaceStore store;
    const RepositoryStatus opened = store.openExisting(databasePath);
    if (!opened)
    {
        return openFailure<
            Application::ScheduleTestingAssignmentSaveResult>(opened.error());
    }
    const WorkspaceStoreScheduleTestingAssignmentSavePort port(store);
    return Application::ScheduleTestingAssignmentSaveUseCase::execute(
        request, port, weekdayCaseInsensitiveEquals);
}

Application::PersonalDisplayNamePreferencesReadResult
readScheduleOutputProfileNameAtPath(const QString& databasePath)
{
    using Result = Application::PersonalDisplayNamePreferencesReadResult;
    WorkspaceStore store;
    const RepositoryStatus opened = store.openExisting(databasePath);
    if (!opened)
    {
        return openFailure<Result>(opened.error());
    }
    SettingsRepository* const repository = store.settingsRepository();
    if (!repository)
    {
        return Result::failure(operationError(
            Domain::ErrorCode::NotFound,
            "The selected workspace has no profile settings repository.", true));
    }
    try
    {
        const auto loaded = repository->loadSetting(
            QStringLiteral("myInfo/name"));
        if (!loaded)
        {
            return Result::failure(repositoryError(loaded.error()));
        }
        if (!loaded->isValid())
        {
            return Result::success({});
        }
        const QByteArray name = loaded->toString().toUtf8();
        return Result::success(std::string(
            name.constData(), static_cast<std::size_t>(name.size())));
    }
    catch (const std::exception&)
    {
        return Result::failure(operationError(
            Domain::ErrorCode::Technical,
            "The profile name could not be loaded for Schedule output.", false));
    }
    catch (...)
    {
        return Result::failure(operationError(
            Domain::ErrorCode::Technical,
            "The profile name could not be loaded for Schedule output.", false));
    }
}

} // namespace ClassMngr::Next::Platform
