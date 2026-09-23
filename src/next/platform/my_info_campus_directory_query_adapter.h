#pragma once

#include "core/resource_paths.h"
#include "features/campus/data/campus_json_repository.h"
#include "next/application/my_info_campus_directory_query_port.h"

#include <QList>
#include <QString>

#include <cstddef>
#include <utility>
#include <vector>

namespace ClassMngr::Next::Platform
{

// Qt-backed adapter for the minimal campus chooser metadata used by My
// Information. The repository supplies the legacy ordering and omission of
// default or unreadable records.
class MyInfoCampusDirectoryQueryAdapter final
    : public Application::MyInfoCampusDirectoryQueryPort
{
public:
    MyInfoCampusDirectoryQueryAdapter()
        : MyInfoCampusDirectoryQueryAdapter(
              ResourcePaths::Campuses::directory()
              )
    {
    }

    explicit MyInfoCampusDirectoryQueryAdapter(
        QString directoryPath
        )
        : m_repository(std::move(directoryPath))
    {
    }

    MyInfoCampusDirectoryQueryAdapter(
        const MyInfoCampusDirectoryQueryAdapter&
        ) = delete;
    MyInfoCampusDirectoryQueryAdapter& operator=(
        const MyInfoCampusDirectoryQueryAdapter&
        ) = delete;
    MyInfoCampusDirectoryQueryAdapter(
        MyInfoCampusDirectoryQueryAdapter&&
        ) = delete;
    MyInfoCampusDirectoryQueryAdapter& operator=(
        MyInfoCampusDirectoryQueryAdapter&&
        ) = delete;

    [[nodiscard]] std::vector<Application::MyInfoCampusMetadata>
    loadCampuses() const override
    {
        std::vector<Application::MyInfoCampusMetadata> campuses;

        const QList<CampusInfo> repositoryCampuses =
            m_repository.loadCampuses();
        campuses.reserve(
            static_cast<std::size_t>(repositoryCampuses.size())
            );

        for (const CampusInfo& campus : repositoryCampuses)
        {
            const QString displayName =
                campus.campusName.trimmed().isEmpty()
                    ? campus.id.trimmed()
                    : campus.campusName.trimmed();

            if (displayName.isEmpty())
            {
                continue;
            }

            campuses.push_back(
                Application::MyInfoCampusMetadata{
                    campus.id.toUtf8().toStdString(),
                    displayName.toUtf8().toStdString()
                }
                );
        }

        return campuses;
    }

private:
    CampusJsonRepository m_repository;
};

} // namespace ClassMngr::Next::Platform
