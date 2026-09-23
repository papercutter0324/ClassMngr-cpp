#pragma once

#include "core/resource_paths.h"
#include "features/campus/data/campus_json_repository.h"
#include "next/application/calendar_page_campus_directory_query_port.h"

#include <QList>
#include <QString>

#include <cstddef>
#include <utility>
#include <vector>

namespace ClassMngr::Next::Platform
{

// Qt-backed adapter for CalendarPage's campus filter metadata. The repository
// retains the legacy name ordering and omission of default or unreadable files.
class CalendarPageCampusDirectoryQueryAdapter final
    : public Application::CalendarPageCampusDirectoryQueryPort
{
public:
    CalendarPageCampusDirectoryQueryAdapter()
        : CalendarPageCampusDirectoryQueryAdapter(
              ResourcePaths::Campuses::directory()
              )
    {
    }

    explicit CalendarPageCampusDirectoryQueryAdapter(
        QString directoryPath
        )
        : m_repository(std::move(directoryPath))
    {
    }

    CalendarPageCampusDirectoryQueryAdapter(
        const CalendarPageCampusDirectoryQueryAdapter&
        ) = delete;
    CalendarPageCampusDirectoryQueryAdapter& operator=(
        const CalendarPageCampusDirectoryQueryAdapter&
        ) = delete;
    CalendarPageCampusDirectoryQueryAdapter(
        CalendarPageCampusDirectoryQueryAdapter&&
        ) = delete;
    CalendarPageCampusDirectoryQueryAdapter& operator=(
        CalendarPageCampusDirectoryQueryAdapter&&
        ) = delete;

    [[nodiscard]] std::vector<Application::CalendarPageCampusMetadata>
    loadCampuses() const override
    {
        std::vector<Application::CalendarPageCampusMetadata> campuses;

        const QList<CampusInfo> repositoryCampuses =
            m_repository.loadCampuses();
        campuses.reserve(
            static_cast<std::size_t>(repositoryCampuses.size())
            );

        for (const CampusInfo& campus : repositoryCampuses)
        {
            Application::CalendarPageCampusMetadata metadata;
            metadata.id = campus.id.toUtf8().toStdString();
            metadata.campusName = campus.campusName.toUtf8().toStdString();

            if (!campus.campusCode.isEmpty())
            {
                metadata.campusCode =
                    campus.campusCode.toUtf8().toStdString();
            }

            campuses.push_back(std::move(metadata));
        }

        return campuses;
    }

private:
    CampusJsonRepository m_repository;
};

} // namespace ClassMngr::Next::Platform
