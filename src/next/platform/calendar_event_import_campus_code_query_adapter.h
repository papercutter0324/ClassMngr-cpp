#pragma once

#include "core/resource_paths.h"
#include "features/campus/data/campus_json_repository.h"
#include "next/application/calendar_event_import_campus_code_query_port.h"

#include <QString>

#include <string>
#include <utility>
#include <vector>

namespace ClassMngr::Next::Platform
{

// Qt-backed adapter for the legacy campus-code lookup used by calendar import.
// CampusJsonRepository supplies the established campus-name order and skips
// default, malformed, unreadable, and unavailable records.
class CalendarEventImportCampusCodeQueryAdapter final
    : public Application::CalendarEventImportCampusCodeQueryPort
{
public:
    CalendarEventImportCampusCodeQueryAdapter()
        : CalendarEventImportCampusCodeQueryAdapter(
              ResourcePaths::Campuses::directory()
              )
    {
    }

    explicit CalendarEventImportCampusCodeQueryAdapter(
        QString directoryPath
        )
        : m_repository(std::move(directoryPath))
    {
    }

    CalendarEventImportCampusCodeQueryAdapter(
        const CalendarEventImportCampusCodeQueryAdapter&
        ) = delete;
    CalendarEventImportCampusCodeQueryAdapter& operator=(
        const CalendarEventImportCampusCodeQueryAdapter&
        ) = delete;
    CalendarEventImportCampusCodeQueryAdapter(
        CalendarEventImportCampusCodeQueryAdapter&&
        ) = delete;
    CalendarEventImportCampusCodeQueryAdapter& operator=(
        CalendarEventImportCampusCodeQueryAdapter&&
        ) = delete;

    [[nodiscard]] std::vector<Application::CampusCode>
    loadCampusCodes() const override
    {
        using CampusCode = Application::CampusCode;
        std::vector<CampusCode> codes;

        for (const CampusInfo& campus : m_repository.loadCampuses())
        {
            const QString code = campus.campusCode.trimmed();
            if (code.isEmpty())
            {
                continue;
            }

            const std::string utf8Code = code.toUtf8().toStdString();
            bool duplicate = false;
            for (const CampusCode& existingCode : codes)
            {
                if (existingCode.value() == utf8Code)
                {
                    duplicate = true;
                    break;
                }
            }

            if (!duplicate)
            {
                codes.emplace_back(utf8Code);
            }
        }

        return codes;
    }

private:
    CampusJsonRepository m_repository;
};

} // namespace ClassMngr::Next::Platform
