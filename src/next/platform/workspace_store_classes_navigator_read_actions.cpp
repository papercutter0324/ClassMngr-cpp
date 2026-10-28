#include "next/platform/workspace_store_classes_navigator_read_actions.h"

#include "data/database/workspace_store.h"
#include "data/repositories/class_info_repository.h"
#include "data/repositories/class_repository.h"
#include "data/repositories/settings_repository.h"
#include "next/application/classes_list_read_query.h"
#include "next/application/positive_decimal_id_validation.h"
#include "next/platform/repository_error_code.h"

#include <QByteArray>
#include <QString>
#include <QVariant>

#include <optional>
#include <string>
#include <utility>

namespace ClassMngr::Next::Platform
{
namespace
{

[[nodiscard]] Domain::OperationError repositoryError(
    const RepositoryError& error,
    const char* fallback
    )
{
    const QByteArray diagnostic = error.diagnostic.toUtf8();
    return {
        .code = repositoryDomainErrorCode(error.code),
        .message = diagnostic.isEmpty()
            ? std::string(fallback)
            : std::string(diagnostic.constData(),
                static_cast<std::size_t>(diagnostic.size())),
        .recoverable = error.code != RepositoryErrorCode::InvalidInput
    };
}

template <typename Result>
[[nodiscard]] Result openFailure(const RepositoryStatus& opened)
{
    return Result::failure(repositoryError(
        opened.error(),
        "The active workspace could not be opened for the Classes read."
        ));
}

[[nodiscard]] Domain::OperationError unavailableError()
{
    return {
        .code = Domain::ErrorCode::NotFound,
        .message = "The active workspace store is unavailable.",
        .recoverable = true
    };
}

template <typename Result>
[[nodiscard]] Result validationFailure(const char* message)
{
    return Result::failure({
        .code = Domain::ErrorCode::Validation,
        .message = message,
        .recoverable = false
    });
}

[[nodiscard]] std::optional<Application::ScheduleDisplayMode>
scheduleModeFromValue(const QVariant& value)
{
    const QString normalized = value.toString().trimmed().toLower();
    if (normalized == QStringLiteral("regular"))
    {
        return Application::ScheduleDisplayMode::Regular;
    }
    if (normalized == QStringLiteral("intensive"))
    {
        return Application::ScheduleDisplayMode::Intensive;
    }
    if (normalized == QStringLiteral("testing"))
    {
        return Application::ScheduleDisplayMode::Testing;
    }
    return std::nullopt;
}

[[nodiscard]] bool settingToBool(const QVariant& value)
{
    if (!value.isValid())
    {
        return false;
    }
    const QString normalized = value.toString().trimmed().toLower();
    if (normalized == QStringLiteral("true")
        || normalized == QStringLiteral("1"))
    {
        return true;
    }
    if (normalized == QStringLiteral("false")
        || normalized == QStringLiteral("0"))
    {
        return false;
    }
    return value.toBool();
}

[[nodiscard]] QString scheduleModeValue(
    const Application::ScheduleDisplayMode mode
    )
{
    switch (mode)
    {
    case Application::ScheduleDisplayMode::Regular:
        return QStringLiteral("regular");
    case Application::ScheduleDisplayMode::Intensive:
        return QStringLiteral("intensive");
    case Application::ScheduleDisplayMode::Testing:
        return QStringLiteral("testing");
    }
    return QStringLiteral("regular");
}

[[nodiscard]] QString visibilityScopeValue(
    const Application::ClassVisibilityScope scope
    )
{
    return scope == Application::ClassVisibilityScope::AllClasses
        ? QStringLiteral("all_classes")
        : QStringLiteral("active_schedule");
}

[[nodiscard]] Application::ScheduleDisplayModePreferencesSaveResult
preferenceSaveFailure(const RepositoryStatus& status, const char* fallback)
{
    return Application::ScheduleDisplayModePreferencesSaveResult::failure(
        repositoryError(status.error(), fallback)
        );
}

[[nodiscard]] Application::ClassVisibilityPreferencesSaveResult
visibilitySaveFailure(const RepositoryStatus& status, const char* fallback)
{
    return Application::ClassVisibilityPreferencesSaveResult::failure(
        repositoryError(status.error(), fallback)
        );
}

class WorkspaceStoreClassesNavigatorReadPort final
    : public Application::ClassesListReadPort,
      public Application::ClassesNavigationSnapshotReadPort
{
public:
    explicit WorkspaceStoreClassesNavigatorReadPort(WorkspaceStore& store)
        : m_store(&store)
    {
    }

    [[nodiscard]] Application::ClassesListReadResult readClassesList()
        const override
    {
        if (!m_store || !m_store->isOpen())
        {
            return Application::ClassesListReadResult::failure(unavailableError());
        }

        ClassRepository* const repository = m_store->classRepository();
        if (!repository)
        {
            return Application::ClassesListReadResult::failure(
                unavailableError()
                );
        }

        const auto loaded = repository->getBoundedClassesForNavigation(
            static_cast<int>(Application::kClassesNavigationMaximumClasses),
            static_cast<int>(Application::kClassesNavigationMaximumTextLength)
            );
        if (!loaded)
        {
            return Application::ClassesListReadResult::failure(
                repositoryError(loaded.error(),
                    "The class list could not be loaded.")
                );
        }

        Application::ClassesListSnapshot snapshot;
        snapshot.classes.reserve(static_cast<std::size_t>(loaded->size()));
        for (const Classroom& classroom : *loaded)
        {
            const auto classId = Domain::ClassId::fromString(
                std::to_string(classroom.id)
                );
            if (!classId)
            {
                return validationFailure<Application::ClassesListReadResult>(
                    "The class list contains an invalid identifier."
                    );
            }
            snapshot.classes.push_back({
                .classId = *classId,
                .className = classroom.name.toStdU16String()
            });
        }

        return Application::ClassesListReadResult::success(std::move(snapshot));
    }

    [[nodiscard]] Application::ClassesNavigationSnapshotResult readClasses(
        const Application::ClassesNavigationSnapshotQuery& requested
        ) const override
    {
        if (!m_store || !m_store->isOpen())
        {
            return Application::ClassesNavigationSnapshotResult::failure(
                unavailableError()
                );
        }
        if (requested.classes.empty())
        {
            return Application::ClassesNavigationSnapshotResult::success({});
        }
        if (requested.classes.size()
            > Application::kClassesNavigationMaximumClasses)
        {
            return validationFailure<
                Application::ClassesNavigationSnapshotResult>(
                    "The class navigation request exceeds its bounded class limit."
                    );
        }

        ClassInfoRepository* const repository = m_store->classInfoRepository();
        if (!repository)
        {
            return Application::ClassesNavigationSnapshotResult::failure(
                unavailableError()
                );
        }

        QList<ClassNavigationBoundedReadRequestEntry> repositoryRequest;
        repositoryRequest.reserve(static_cast<qsizetype>(requested.classes.size()));
        for (std::size_t index = 0; index < requested.classes.size(); ++index)
        {
            const auto parsedId = Application::parseCanonicalPositiveDecimalId(
                requested.classes[index].classId.value()
                );
            if (!parsedId)
            {
                return validationFailure<
                    Application::ClassesNavigationSnapshotResult>(
                        "The class navigation request contains an invalid identifier."
                        );
            }
            repositoryRequest.append({
                .classId = *parsedId,
                .className = QString::fromStdU16String(
                    requested.classes[index].className)
            });
        }

        const ClassNavigationBoundedReadLimits limits{
            .maximumClassCount = static_cast<int>(
                Application::kClassesNavigationMaximumClasses),
            .maximumTextLength = static_cast<int>(
                Application::kClassesNavigationMaximumTextLength),
            .maximumScheduleRowsPerSourceAndClass = static_cast<int>(
                Application::kClassesNavigationMaximumScheduleRows),
            .maximumScheduleTokenLength = static_cast<int>(
                Application::kClassesNavigationMaximumScheduleTokenLength)
        };
        const auto loaded = repository->loadBoundedClassesNavigationRecords(
            repositoryRequest,
            limits
            );
        if (!loaded)
        {
            return Application::ClassesNavigationSnapshotResult::failure(
                repositoryError(loaded.error(),
                    "Class navigation metadata could not be loaded.")
                );
        }

        if (loaded->size() != repositoryRequest.size())
        {
            return validationFailure<
                Application::ClassesNavigationSnapshotResult>(
                    "The repository returned a different number of navigation records."
                    );
        }

        Application::ClassesNavigationSnapshot snapshot;
        snapshot.classes.reserve(loaded->size());
        for (std::size_t index = 0; index < loaded->size(); ++index)
        {
            const ClassNavigationBoundedReadRecord& source = (*loaded)[
                static_cast<qsizetype>(index)];
            const auto classId = Domain::ClassId::fromString(
                std::to_string(source.details.classId)
                );
            if (!classId
                || requested.classes[index].classId != *classId
                || requested.classes[index].className
                    != source.className.toStdU16String())
            {
                return validationFailure<
                    Application::ClassesNavigationSnapshotResult>(
                        "The repository changed class identity or order."
                        );
            }

            Application::ClassesNavigationDetails details;
            details.grade = source.details.grade.toStdU16String();
            details.level = source.details.level.toStdU16String();
            details.teacherEnglishName =
                source.details.teacherEnglishName.toStdU16String();
            details.teacherKoreanName =
                source.details.teacherKoreanName.toStdU16String();
            for (const ClassTime& time : source.details.regularTimes)
            {
                details.regularSchedule.push_back({
                    .day = time.day.toStdU16String(),
                    .startTime = time.startTime.toStdU16String(),
                    .endTime = time.endTime.toStdU16String()
                });
            }
            for (const ClassTime& time : source.details.intensiveTimes)
            {
                details.intensiveSchedule.push_back({
                    .day = time.day.toStdU16String(),
                    .startTime = time.startTime.toStdU16String(),
                    .endTime = time.endTime.toStdU16String()
                });
            }
            snapshot.classes.push_back({
                .classId = *classId,
                .className = source.className.toStdU16String(),
                .details = Application::ClassesNavigationDetailsResult::success(
                    std::move(details))
            });
        }

        return Application::ClassesNavigationSnapshotResult::success(
            std::move(snapshot)
            );
    }

private:
    WorkspaceStore* m_store = nullptr;
};

[[nodiscard]] std::optional<QVariant> loadSetting(
    SettingsRepository& repository,
    const QString& key
    )
{
    const auto loaded = repository.loadSetting(key);
    return loaded
        ? std::optional<QVariant>(*loaded)
        : std::nullopt;
}

void materializeSetting(
    SettingsRepository& repository,
    const QString& key,
    const QVariant& value
    )
{
    static_cast<void>(repository.saveSetting(key, value));
}

[[nodiscard]] Application::ClassesNavigatorPreferences readPreferences(
    WorkspaceStore& store
    )
{
    Application::ClassesNavigatorPreferences result;
    if (!store.isOpen())
    {
        return result;
    }
    SettingsRepository* const repository = store.settingsRepository();
    if (!repository)
    {
        return result;
    }

    constexpr auto regular = Application::ScheduleDisplayMode::Regular;
    const auto storedScheduleMode = loadSetting(
        *repository,
        QStringLiteral("schedule_display_mode")
        );
    if (!storedScheduleMode)
    {
        return result;
    }
    result.scheduleDisplayMode = scheduleModeFromValue(*storedScheduleMode);
    if (!result.scheduleDisplayMode)
    {
        const auto legacyIntensive = loadSetting(
            *repository,
            QStringLiteral("schedule_show_intensive")
            );
        const Application::ScheduleDisplayMode fallback =
            legacyIntensive && settingToBool(*legacyIntensive)
            ? Application::ScheduleDisplayMode::Intensive
            : regular;
        result.scheduleDisplayMode = fallback;
        if (!storedScheduleMode->isValid())
        {
            materializeSetting(
                *repository,
                QStringLiteral("schedule_display_mode"),
                scheduleModeValue(fallback)
                );
        }
    }

    const auto storedVisibility = loadSetting(
        *repository,
        QStringLiteral("classes_navigation_visibility_scope")
        );
    if (storedVisibility)
    {
        result.visibilityScope = storedVisibility->isValid()
            && storedVisibility->toString().trimmed().toLower()
                == QStringLiteral("all_classes")
            ? Application::ClassVisibilityScope::AllClasses
            : Application::ClassVisibilityScope::ActiveSchedule;
        if (!storedVisibility->isValid())
        {
            materializeSetting(
                *repository,
                QStringLiteral("classes_navigation_visibility_scope"),
                visibilityScopeValue(*result.visibilityScope)
                );
        }
    }

    const auto loadResetPolicy = [repository](
        const QString& key
        ) -> std::optional<bool> {
        const auto stored = loadSetting(*repository, key);
        if (!stored)
        {
            return std::nullopt;
        }
        if (!stored->isValid())
        {
            materializeSetting(
                *repository,
                key,
                QStringLiteral("on_application_close")
                );
            return false;
        }
        return stored->toString().trimmed().toLower()
            == QStringLiteral("on_page_leave");
    };
    if (const auto reset = loadResetPolicy(QStringLiteral(
            "classes_navigation_day_filter_reset_policy")))
    {
        result.dayFilterResetPolicy = *reset
            ? Application::ClassDayFilterResetPolicy::OnPageLeave
            : Application::ClassDayFilterResetPolicy::OnApplicationClose;
    }
    if (const auto reset = loadResetPolicy(QStringLiteral(
            "classes_navigation_class_selection_reset_policy")))
    {
        result.classSelectionResetPolicy = *reset
            ? Application::ClassSelectionResetPolicy::OnPageLeave
            : Application::ClassSelectionResetPolicy::OnApplicationClose;
    }
    return result;
}

} // namespace

Application::ClassesListReadResult readClassesNavigatorListAtPath(
    const QString& databasePath
    )
{
    WorkspaceStore store;
    const RepositoryStatus opened = store.openExisting(databasePath);
    if (!opened)
    {
        return openFailure<Application::ClassesListReadResult>(opened);
    }
    const WorkspaceStoreClassesNavigatorReadPort port(store);
    return Application::ClassesListReadQuery(port).execute();
}

Application::ClassesNavigationSnapshotResult
readClassesNavigatorNavigationAtPath(
    const QString& databasePath,
    const Application::ClassesNavigationSnapshotQuery& query
    )
{
    WorkspaceStore store;
    const RepositoryStatus opened = store.openExisting(databasePath);
    if (!opened)
    {
        return openFailure<Application::ClassesNavigationSnapshotResult>(opened);
    }
    const WorkspaceStoreClassesNavigatorReadPort port(store);
    return Application::ClassesNavigationSnapshotQueryHandler::execute(
        query,
        port
        );
}

Application::ClassesNavigatorPreferences
readClassesNavigatorPreferencesAtPath(const QString& databasePath)
{
    WorkspaceStore store;
    if (!store.openExisting(databasePath))
    {
        return {};
    }
    return readPreferences(store);
}

Application::ScheduleDisplayModePreferencesSaveResult
saveClassesNavigatorScheduleModeAtPath(
    const QString& databasePath,
    const Application::ScheduleDisplayMode mode
    )
{
    if (mode != Application::ScheduleDisplayMode::Regular
        && mode != Application::ScheduleDisplayMode::Intensive
        && mode != Application::ScheduleDisplayMode::Testing)
    {
        return Application::ScheduleDisplayModePreferencesSaveResult::failure({
            .code = Domain::ErrorCode::InvalidInput,
            .message = "The Classes schedule mode is invalid.",
            .recoverable = false
        });
    }
    WorkspaceStore store;
    const RepositoryStatus opened = store.openExisting(databasePath);
    if (!opened)
    {
        return Application::ScheduleDisplayModePreferencesSaveResult::failure(
            repositoryError(opened.error(),
                "The schedule source could not be saved.")
            );
    }
    SettingsRepository* const repository = store.settingsRepository();
    if (!repository)
    {
        return Application::ScheduleDisplayModePreferencesSaveResult::failure(
            unavailableError()
            );
    }
    const RepositoryStatus saved = repository->saveSetting(
        QStringLiteral("schedule_display_mode"),
        scheduleModeValue(mode)
        );
    return saved
        ? Application::ScheduleDisplayModePreferencesSaveResult::success()
        : preferenceSaveFailure(saved, "The schedule source could not be saved.");
}

Application::ClassVisibilityPreferencesSaveResult
saveClassesNavigatorVisibilityAtPath(
    const QString& databasePath,
    const Application::ClassVisibilityScope scope
    )
{
    if (scope != Application::ClassVisibilityScope::ActiveSchedule
        && scope != Application::ClassVisibilityScope::AllClasses)
    {
        return Application::ClassVisibilityPreferencesSaveResult::failure({
            .code = Domain::ErrorCode::InvalidInput,
            .message = "The Classes visibility scope is invalid.",
            .recoverable = false
        });
    }
    WorkspaceStore store;
    const RepositoryStatus opened = store.openExisting(databasePath);
    if (!opened)
    {
        return Application::ClassVisibilityPreferencesSaveResult::failure(
            repositoryError(opened.error(),
                "The Classes visibility preference could not be saved.")
            );
    }
    SettingsRepository* const repository = store.settingsRepository();
    if (!repository)
    {
        return Application::ClassVisibilityPreferencesSaveResult::failure(
            unavailableError()
            );
    }
    const RepositoryStatus saved = repository->saveSetting(
        QStringLiteral("classes_navigation_visibility_scope"),
        visibilityScopeValue(scope)
        );
    return saved
        ? Application::ClassVisibilityPreferencesSaveResult::success()
        : visibilitySaveFailure(
            saved,
            "The Classes visibility preference could not be saved."
            );
}

} // namespace ClassMngr::Next::Platform
