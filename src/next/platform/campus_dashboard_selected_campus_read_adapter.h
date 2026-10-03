#pragma once

#include "features/campus/data/campus_json_repository.h"
#include "next/application/campus_dashboard_selected_campus_read_port.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>
#include <QStringList>

#include <optional>
#include <utility>
#include <vector>

namespace ClassMngr::Next::Platform
{

class CampusDashboardSelectedCampusReadAdapter final
    : public Application::CampusDashboardSelectedCampusReadPort
{
public:
    explicit CampusDashboardSelectedCampusReadAdapter(
        QString campusDirectory
        )
        : m_repository(std::move(campusDirectory))
    {
    }

    CampusDashboardSelectedCampusReadAdapter(
        const CampusDashboardSelectedCampusReadAdapter&
        ) = delete;
    CampusDashboardSelectedCampusReadAdapter& operator=(
        const CampusDashboardSelectedCampusReadAdapter&
        ) = delete;
    CampusDashboardSelectedCampusReadAdapter(
        CampusDashboardSelectedCampusReadAdapter&&
        ) = delete;
    CampusDashboardSelectedCampusReadAdapter& operator=(
        CampusDashboardSelectedCampusReadAdapter&&
        ) = delete;

    [[nodiscard]] Application::CampusDashboardSelectedCampusReadResult
    loadCampus(const Domain::CampusId& campusId) const override
    {
        const std::optional<CampusInfo> loaded = m_repository.loadCampus(
            QString::fromUtf8(campusId.value())
            );
        if (!loaded.has_value())
        {
            return Application::CampusDashboardSelectedCampusReadResult::success(
                std::nullopt
                );
        }

        const CampusInfo& campus = loaded.value();
        const std::optional<Domain::CampusId> returnedId =
            Domain::CampusId::fromString(campus.id.toUtf8().toStdString());
        if (!returnedId.has_value())
        {
            return Application::CampusDashboardSelectedCampusReadResult::failure(
                Domain::OperationError{
                    .code = Domain::ErrorCode::Technical,
                    .message = "Campus record has no usable identifier.",
                    .recoverable = false
                }
                );
        }

        Application::CampusDashboardSelectedCampusSnapshot snapshot{
            .id = returnedId.value(),
            .campusName = utf8(campus.campusName),
            .campusCode = utf8(campus.campusCode),
            .buildingName = utf8(campus.buildingName),
            .buildingNameKr = utf8(campus.buildingNameKr),
            .address = utf8(campus.address),
            .phoneNumber = utf8(campus.phoneNumber),
            .officeNumber = utf8(campus.officeNumber),
            .directionsAddressEnJson = compactJson(campus.directionsAddressEn),
            .directionsAddressKrJson = compactJson(campus.directionsAddressKr),
            .directionsNote = utf8(campus.directionsNote),
            .transitSteps = utf8List(campus.transitSteps),
            .arrivalInfo = utf8(campus.arrivalInfo),
            .imageMain = utf8(campus.imageMain),
            .mapImagePaths = utf8List(campus.mapImagePaths),
            .naverMapUrl = utf8(campus.naverMapUrl),
            .kakaoMapUrl = utf8(campus.kakaoMapUrl),
            .officeWifi = utf8(campus.officeWifi),
            .officeWifiPassword = utf8(campus.officeWifiPassword),
            .printerName = utf8(campus.printerName),
            .printerSteps = utf8(campus.printerSteps),
            .printerDriverUrl = utf8(campus.printerDriverUrl),
            .printerDriverUrlUnavailable = campus.printerDriverUrlUnavailable,
            .photocopierCode = utf8(campus.photocopierCode),
            .housingLocationsJson = compactJson(campus.housingLocations)
        };

        return Application::CampusDashboardSelectedCampusReadResult::success(
            std::move(snapshot)
            );
    }

private:
    [[nodiscard]] static std::string utf8(const QString& value)
    {
        return value.toUtf8().toStdString();
    }

    [[nodiscard]] static std::vector<std::string> utf8List(
        const QStringList& values
        )
    {
        std::vector<std::string> result;
        result.reserve(static_cast<std::size_t>(values.size()));
        for (const QString& value : values)
        {
            result.push_back(utf8(value));
        }
        return result;
    }

    [[nodiscard]] static std::string compactJson(
        const QJsonObject& object
        )
    {
        return QJsonDocument(object)
            .toJson(QJsonDocument::Compact)
            .toStdString();
    }

    [[nodiscard]] static std::string compactJson(
        const QJsonArray& array
        )
    {
        return QJsonDocument(array)
            .toJson(QJsonDocument::Compact)
            .toStdString();
    }

    CampusJsonRepository m_repository;
};

} // namespace ClassMngr::Next::Platform
