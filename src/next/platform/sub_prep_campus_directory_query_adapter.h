#pragma once

#include "core/resource_paths.h"
#include "features/campus/data/campus_json_repository.h"
#include "next/application/sub_prep_campus_directory_query_port.h"

#include <QList>
#include <QString>

#include <cstddef>
#include <utility>
#include <vector>

namespace ClassMngr::Next::Platform
{

// Qt-backed adapter for Sub Prep's campus selector and office details. The
// repository retains legacy campus-name ordering and omission behavior.
class SubPrepCampusDirectoryQueryAdapter final
    : public Application::SubPrepCampusDirectoryQueryPort
{
public:
    SubPrepCampusDirectoryQueryAdapter()
        : SubPrepCampusDirectoryQueryAdapter(
              ResourcePaths::Campuses::directory()
              )
    {
    }

    explicit SubPrepCampusDirectoryQueryAdapter(
        QString directoryPath
        )
        : m_repository(std::move(directoryPath))
    {
    }

    SubPrepCampusDirectoryQueryAdapter(
        const SubPrepCampusDirectoryQueryAdapter&
        ) = delete;
    SubPrepCampusDirectoryQueryAdapter& operator=(
        const SubPrepCampusDirectoryQueryAdapter&
        ) = delete;
    SubPrepCampusDirectoryQueryAdapter(
        SubPrepCampusDirectoryQueryAdapter&&
        ) = delete;
    SubPrepCampusDirectoryQueryAdapter& operator=(
        SubPrepCampusDirectoryQueryAdapter&&
        ) = delete;

    [[nodiscard]] std::vector<Application::SubPrepCampusMetadata>
    loadCampuses() const override
    {
        std::vector<Application::SubPrepCampusMetadata> campuses;

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

            campuses.push_back(
                Application::SubPrepCampusMetadata{
                    campus.id.toUtf8().toStdString(),
                    displayName.toUtf8().toStdString(),
                    campus.officeNumber.toUtf8().toStdString(),
                    campus.officeWifi.toUtf8().toStdString(),
                    campus.officeWifiPassword.toUtf8().toStdString(),
                    campus.photocopierCode.toUtf8().toStdString()
                }
                );
        }

        return campuses;
    }

private:
    CampusJsonRepository m_repository;
};

} // namespace ClassMngr::Next::Platform
