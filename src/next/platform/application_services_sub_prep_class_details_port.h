#pragma once

#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "next/application/sub_prep_class_details_query.h"

#include <QByteArray>
#include <QString>

#include <charconv>
#include <cstddef>
#include <exception>
#include <optional>
#include <string>
#include <system_error>
#include <utility>

namespace ClassMngr::Next::Platform
{

// Reads one selected Sub Prep class detail record through the active session
// and copies its bounded fields into the application-owned value. The adapter
// retains only ApplicationServices and does not expose legacy record objects.
class ApplicationServicesSubPrepClassDetailsPort final
    : public Application::SubPrepClassDetailsReadPort
{
public:
    explicit ApplicationServicesSubPrepClassDetailsPort(
        ApplicationServices& services
        ) noexcept
        : m_services(services)
    {
    }

    ApplicationServicesSubPrepClassDetailsPort(
        const ApplicationServicesSubPrepClassDetailsPort&
        ) = delete;
    ApplicationServicesSubPrepClassDetailsPort& operator=(
        const ApplicationServicesSubPrepClassDetailsPort&
        ) = delete;
    ApplicationServicesSubPrepClassDetailsPort(
        ApplicationServicesSubPrepClassDetailsPort&&
        ) = delete;
    ApplicationServicesSubPrepClassDetailsPort& operator=(
        ApplicationServicesSubPrepClassDetailsPort&&
        ) = delete;

    [[nodiscard]] Application::SubPrepClassDetailsReadResult loadDetails(
        const Domain::ClassId& classId
        ) override
    {
        if (!Application::ClassSummaryProjectionDetail::isValidId(classId))
        {
            return failure(
                Domain::ErrorCode::InvalidInput,
                "Selected class identifier must be non-blank and bounded."
                );
        }

        const std::optional<int> legacyClassId = legacyId(classId.value());
        if (!legacyClassId)
        {
            return failure(
                Domain::ErrorCode::InvalidInput,
                "Selected class identifier must be a canonical positive integer."
                );
        }

        try
        {
            ClassService* classService = m_services.classService();
            if (!classService || !classService->isAvailable())
            {
                return failure(
                    Domain::ErrorCode::NotFound,
                    "The Sub Prep class details service is unavailable."
                    );
            }

            const ::Result<SubPrepClassDetailsRecord> source =
                classService->subPrepClassDetails(*legacyClassId);
            if (!source)
            {
                if (!classService->isAvailable())
                {
                    return failure(
                        Domain::ErrorCode::NotFound,
                        "The Sub Prep class details service became unavailable while loading details."
                        );
                }
                return legacyFailure(source.error());
            }

            const SubPrepClassDetailsRecord& record = source.value();
            if (record.classId != *legacyClassId)
            {
                return failure(
                    Domain::ErrorCode::Validation,
                    "Selected class details do not match the requested class."
                    );
            }

            const auto classNotes = boundedUtf8(
                record.classNotes,
                Application::kSelectedClassDetailsMaxClassNotesLength
                );
            if (!classNotes)
            {
                return failure(
                    Domain::ErrorCode::Validation,
                    "Selected class notes exceed the bounded details limit."
                    );
            }

            Application::SubPrepClassDetails details{
                .classId = classId,
                .teacherId = std::nullopt,
                .classNotes = *classNotes,
                .teacherDisplayName = {},
                .teacherFacilities = {},
                .teacherNotes = {}
            };

            if (record.teacherId > 0)
            {
                const auto displayName = boundedUtf8(
                    preferredDisplayName(record),
                    Application::kSelectedClassDetailsMaxTeacherDisplayNameLength
                    );
                const auto room = boundedUtf8(
                    record.roomNumber,
                    Application::kSelectedClassDetailsMaxTeacherRoomLength
                    );
                const auto wifiName = boundedUtf8(
                    record.wifiName,
                    Application::kSelectedClassDetailsMaxTeacherWifiNameLength
                    );
                const auto wifiPassword = boundedUtf8(
                    record.wifiPassword,
                    Application::kSelectedClassDetailsMaxTeacherWifiPasswordLength
                    );
                const auto internetType = boundedUtf8(
                    record.internetType,
                    Application::kSelectedClassDetailsMaxTeacherInternetTypeLength
                    );
                const auto zoomId = boundedUtf8(
                    record.zoomId,
                    Application::kSelectedClassDetailsMaxTeacherZoomIdLength
                    );
                const auto zoomPassword = boundedUtf8(
                    record.zoomPassword,
                    Application::kSelectedClassDetailsMaxTeacherZoomPasswordLength
                    );
                const auto projectionType = boundedUtf8(
                    record.projectionType,
                    Application::kSelectedClassDetailsMaxTeacherProjectionTypeLength
                    );
                const auto teacherNotes = boundedUtf8(
                    record.teacherNotes,
                    Application::kSelectedClassDetailsMaxTeacherNotesLength
                    );
                if (!displayName || !room || !wifiName || !wifiPassword
                    || !internetType || !zoomId || !zoomPassword
                    || !projectionType || !teacherNotes)
                {
                    return failure(
                        Domain::ErrorCode::Validation,
                        "Selected teacher details exceed a bounded field limit."
                        );
                }

                details.teacherId = teacherId(record.teacherId);
                details.teacherDisplayName = *displayName;
                details.teacherFacilities = {
                    .room = *room,
                    .wifiName = *wifiName,
                    .wifiPassword = *wifiPassword,
                    .internetType = *internetType,
                    .zoomId = *zoomId,
                    .zoomPassword = *zoomPassword,
                    .projectionType = *projectionType
                };
                details.teacherNotes = *teacherNotes;
            }

            return Application::SubPrepClassDetailsReadResult::success(
                std::move(details)
                );
        }
        catch (const std::exception&)
        {
            return failure(
                Domain::ErrorCode::Technical,
                "Selected Sub Prep class details could not be loaded."
                );
        }
        catch (...)
        {
            return failure(
                Domain::ErrorCode::Technical,
                "Selected Sub Prep class details could not be loaded."
                );
        }
    }

private:
    [[nodiscard]] static std::optional<int> legacyId(
        const std::string& value
        )
    {
        if (value.empty())
        {
            return std::nullopt;
        }

        int parsed = 0;
        const auto [end, error] = std::from_chars(
            value.data(),
            value.data() + value.size(),
            parsed
            );
        if (error != std::errc{} || end != value.data() + value.size()
            || parsed <= 0 || std::to_string(parsed) != value)
        {
            return std::nullopt;
        }
        return parsed;
    }

    [[nodiscard]] static Domain::TeacherId teacherId(
        const int legacyTeacherId
        )
    {
        return *Domain::TeacherId::fromString(
            std::to_string(legacyTeacherId)
            );
    }

    [[nodiscard]] static QString preferredDisplayName(
        const SubPrepClassDetailsRecord& record
        )
    {
        const QString selected = record.teacherPreferredName.trimmed();
        if (!selected.isEmpty())
        {
            return selected;
        }

        const QString english = record.teacherEn.trimmed();
        if (!english.isEmpty())
        {
            return english;
        }

        const QString romanized = record.teacherPreferredRomanization.trimmed();
        if (!romanized.isEmpty())
        {
            return romanized;
        }

        return record.teacherKr.trimmed();
    }

    [[nodiscard]] static std::optional<std::string> boundedUtf8(
        const QString& value,
        const std::size_t maxLength
        )
    {
        // UTF-8 takes at least one byte per UTF-16 code unit. Reject obviously
        // oversized source text before allocating its encoded copy.
        if (static_cast<std::size_t>(value.size()) > maxLength)
        {
            return std::nullopt;
        }

        const QByteArray bytes = value.toUtf8();
        if (static_cast<std::size_t>(bytes.size()) > maxLength)
        {
            return std::nullopt;
        }
        return bytes.toStdString();
    }

    [[nodiscard]] static Application::SubPrepClassDetailsReadResult
    legacyFailure(
        const QString& legacyError
        )
    {
        const QString normalized = legacyError.toLower();
        if (normalized.contains(QStringLiteral("service is available"))
            || normalized.contains(QStringLiteral("service unavailable")))
        {
            return failure(
                Domain::ErrorCode::NotFound,
                errorText(legacyError,
                    "The Sub Prep class details service is unavailable.")
                );
        }
        if (normalized.contains(QStringLiteral("no matching record"))
            || normalized.contains(QStringLiteral("not found"))
            || normalized.contains(QStringLiteral("does not exist")))
        {
            return failure(
                Domain::ErrorCode::NotFound,
                errorText(legacyError,
                    "The selected Sub Prep class details were not found.")
                );
        }
        if (normalized.contains(QStringLiteral("invalid")))
        {
            return failure(
                Domain::ErrorCode::InvalidInput,
                errorText(legacyError,
                    "The selected Sub Prep class identifier is invalid.")
                );
        }
        if (normalized.contains(QStringLiteral("validation")))
        {
            return failure(
                Domain::ErrorCode::Validation,
                errorText(legacyError,
                    "The selected Sub Prep class details failed validation.")
                );
        }

        return failure(
            Domain::ErrorCode::Technical,
            errorText(legacyError,
                "Selected Sub Prep class details could not be loaded.")
            );
    }

    [[nodiscard]] static std::string errorText(
        const QString& legacyError,
        const char* fallback
        )
    {
        const QByteArray bytes = legacyError.toUtf8();
        return bytes.isEmpty() ? fallback : bytes.toStdString();
    }

    [[nodiscard]] static Application::SubPrepClassDetailsReadResult failure(
        const Domain::ErrorCode code,
        std::string message
        )
    {
        return Application::SubPrepClassDetailsReadResult::failure({
            .code = code,
            .message = std::move(message),
            .recoverable = false
        });
    }

    ApplicationServices& m_services;
};

} // namespace ClassMngr::Next::Platform
