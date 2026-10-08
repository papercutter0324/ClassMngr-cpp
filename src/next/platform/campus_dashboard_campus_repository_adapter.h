#pragma once

#include "features/campus/data/campus_json_repository.h"
#include "next/application/campus_dashboard_campus_save_port.h"

#include <QByteArray>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>
#include <QStringList>

#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace ClassMngr::Next::Platform
{

class CampusDashboardCampusRepositoryAdapter final
    : public Application::CampusDashboardCampusSavePort
{
public:
    explicit CampusDashboardCampusRepositoryAdapter(
        QString campusDirectory
        )
        : m_repository(std::move(campusDirectory))
    {
    }

    CampusDashboardCampusRepositoryAdapter(
        const CampusDashboardCampusRepositoryAdapter&
        ) = delete;
    CampusDashboardCampusRepositoryAdapter& operator=(
        const CampusDashboardCampusRepositoryAdapter&
        ) = delete;
    CampusDashboardCampusRepositoryAdapter(
        CampusDashboardCampusRepositoryAdapter&&
        ) = delete;
    CampusDashboardCampusRepositoryAdapter& operator=(
        CampusDashboardCampusRepositoryAdapter&&
        ) = delete;

    [[nodiscard]] Domain::Result<void> saveCampus(
        const Application::CampusDashboardCampusSnapshot& snapshot
        ) const override
    {
        const Status saved = m_repository.saveCampus(
            campusInfoFromSnapshot(snapshot)
            );
        if (!saved)
        {
            return Domain::Result<void>::failure({
                .code = Domain::ErrorCode::Technical,
                .message = utf8(saved.error()),
                .recoverable = false
            });
        }

        return Domain::Result<void>::success();
    }

    [[nodiscard]] static std::optional<
        Application::CampusDashboardCampusSnapshot
        > snapshotFromCampusInfo(const CampusInfo& campus)
    {
        const auto id = Domain::CampusId::fromString(utf8(campus.id));
        if (!id.has_value())
        {
            return std::nullopt;
        }

        return Application::CampusDashboardCampusSnapshot{
            .id = id.value(),
            .campusName = utf8(campus.campusName),
            .campusCode = utf8(campus.campusCode),
            .buildingName = utf8(campus.buildingName),
            .buildingNameKr = utf8(campus.buildingNameKr),
            .address = utf8(campus.address),
            .phoneNumber = utf8(campus.phoneNumber),
            .officeNumber = utf8(campus.officeNumber),
            .directionsAddressEnJson = compactJson(
                campus.directionsAddressEn
                ),
            .directionsAddressKrJson = compactJson(
                campus.directionsAddressKr
                ),
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
            .printerDriverUrlUnavailable =
                campus.printerDriverUrlUnavailable,
            .photocopierCode = utf8(campus.photocopierCode),
            .housingLocationsJson = compactJson(campus.housingLocations)
        };
    }

    [[nodiscard]] static CampusInfo campusInfoFromSnapshot(
        const Application::CampusDashboardCampusSnapshot& snapshot
        )
    {
        CampusInfo campus;
        campus.id = fromUtf8(snapshot.id.value());
        campus.campusName = fromUtf8(snapshot.campusName);
        campus.campusCode = fromUtf8(snapshot.campusCode);
        campus.buildingName = fromUtf8(snapshot.buildingName);
        campus.buildingNameKr = fromUtf8(snapshot.buildingNameKr);
        campus.address = fromUtf8(snapshot.address);
        campus.phoneNumber = fromUtf8(snapshot.phoneNumber);
        campus.officeNumber = fromUtf8(snapshot.officeNumber);
        campus.directionsAddressEn = QJsonDocument::fromJson(
            QByteArray::fromStdString(snapshot.directionsAddressEnJson)
            ).object();
        campus.directionsAddressKr = QJsonDocument::fromJson(
            QByteArray::fromStdString(snapshot.directionsAddressKrJson)
            ).object();
        campus.directionsNote = fromUtf8(snapshot.directionsNote);
        campus.transitSteps = fromUtf8List(snapshot.transitSteps);
        campus.arrivalInfo = fromUtf8(snapshot.arrivalInfo);
        campus.imageMain = fromUtf8(snapshot.imageMain);
        campus.mapImagePaths = fromUtf8List(snapshot.mapImagePaths);
        campus.naverMapUrl = fromUtf8(snapshot.naverMapUrl);
        campus.kakaoMapUrl = fromUtf8(snapshot.kakaoMapUrl);
        campus.officeWifi = fromUtf8(snapshot.officeWifi);
        campus.officeWifiPassword = fromUtf8(snapshot.officeWifiPassword);
        campus.printerName = fromUtf8(snapshot.printerName);
        campus.printerSteps = fromUtf8(snapshot.printerSteps);
        campus.printerDriverUrl = fromUtf8(snapshot.printerDriverUrl);
        campus.printerDriverUrlUnavailable =
            snapshot.printerDriverUrlUnavailable;
        campus.photocopierCode = fromUtf8(snapshot.photocopierCode);
        campus.housingLocations = QJsonDocument::fromJson(
            QByteArray::fromStdString(snapshot.housingLocationsJson)
            ).array();
        return campus;
    }

private:
    [[nodiscard]] static std::string utf8(const QString& value)
    {
        const QByteArray bytes = value.toUtf8();
        return {bytes.constData(), static_cast<std::size_t>(bytes.size())};
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
        const QByteArray bytes = QJsonDocument(object)
            .toJson(QJsonDocument::Compact);
        return {bytes.constData(), static_cast<std::size_t>(bytes.size())};
    }

    [[nodiscard]] static std::string compactJson(
        const QJsonArray& array
        )
    {
        const QByteArray bytes = QJsonDocument(array)
            .toJson(QJsonDocument::Compact);
        return {bytes.constData(), static_cast<std::size_t>(bytes.size())};
    }

    [[nodiscard]] static QString fromUtf8(const std::string& value)
    {
        return QString::fromUtf8(
            value.data(),
            static_cast<qsizetype>(value.size())
            );
    }

    [[nodiscard]] static QStringList fromUtf8List(
        const std::vector<std::string>& values
        )
    {
        QStringList result;
        result.reserve(static_cast<qsizetype>(values.size()));
        for (const std::string& value : values)
        {
            result.push_back(fromUtf8(value));
        }
        return result;
    }

    CampusJsonRepository m_repository;
};

} // namespace ClassMngr::Next::Platform
