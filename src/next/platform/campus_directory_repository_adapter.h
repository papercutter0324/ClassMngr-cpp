#pragma once

#include "next/application/campus_dashboard_campus_save_port.h"
#include "next/application/campus_dashboard_campus_identity_save_policy.h"
#include "next/application/campus_dashboard_selected_campus_read_port.h"
#include "next/application/campus_directory_read_port.h"
#include "next/resources/resource_loader.h"

#include <QDir>
#include <QCryptographicHash>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QSaveFile>
#include <QString>

#include <algorithm>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace ClassMngr::Next::Platform
{

// Reads the packaged campus records through ResourceLoader and applies a
// writable app-data directory as an ID-keyed overlay. Legacy files are read
// only when an explicit migration call is made.
class CampusDirectoryRepositoryAdapter final
    : public Application::CampusDirectoryReadPort
    , public Application::CampusDashboardSelectedCampusReadPort
    , public Application::CampusDashboardCampusSavePort
{
public:
    CampusDirectoryRepositoryAdapter(
        Resources::ResourceLoader& resourceLoader,
        QString overrideDirectory
        )
        : m_resourceLoader(&resourceLoader)
        , m_overrideDirectory(std::move(overrideDirectory))
    {
    }

    CampusDirectoryRepositoryAdapter(const CampusDirectoryRepositoryAdapter&) = delete;
    CampusDirectoryRepositoryAdapter& operator=(const CampusDirectoryRepositoryAdapter&) = delete;
    CampusDirectoryRepositoryAdapter(CampusDirectoryRepositoryAdapter&&) = delete;
    CampusDirectoryRepositoryAdapter& operator=(CampusDirectoryRepositoryAdapter&&) = delete;

    [[nodiscard]] Application::CampusDirectoryReadResult loadCampuses() const override
    {
        const auto records = loadEffectiveRecords();
        if (!records)
        {
            return Application::CampusDirectoryReadResult::failure(records.error());
        }

        std::vector<Application::CampusDirectoryEntry> campuses;
        campuses.reserve(records.value().size());
        for (const auto& [id, object] : records.value())
        {
            const auto snapshot = snapshotFromJson(object);
            if (!snapshot)
            {
                return Application::CampusDirectoryReadResult::failure(
                    snapshot.error()
                    );
            }
            if (isReservedDefault(snapshot.value()))
            {
                continue;
            }
            const auto campusId = Domain::CampusId::fromString(
                id.toUtf8().toStdString()
                );
            if (!campusId)
            {
                return Application::CampusDirectoryReadResult::failure({
                    .code = Domain::ErrorCode::Validation,
                    .message = "Campus record has an invalid identifier.",
                    .recoverable = false
                });
            }
            campuses.push_back({
                .id = *campusId,
                .displayName = utf8(object.value(QStringLiteral("campus_name")).toString())
            });
        }

        return Application::CampusDirectoryReadResult::success(std::move(campuses));
    }

    [[nodiscard]] Application::CampusDashboardSelectedCampusReadResult loadCampus(
        const Domain::CampusId& campusId
        ) const override
    {
        const QString id = QString::fromUtf8(
            campusId.value().data(),
            static_cast<qsizetype>(campusId.value().size())
            );
        const auto records = loadEffectiveRecords();
        if (!records)
        {
            return Application::CampusDashboardSelectedCampusReadResult::failure(
                records.error()
                );
        }
        const auto found = records.value().find(id);
        if (found == records.value().end() || isReservedDefaultObject(found->second))
        {
            return Application::CampusDashboardSelectedCampusReadResult::success(
                std::nullopt
                );
        }
        const auto snapshot = snapshotFromJson(found->second);
        if (!snapshot)
        {
            return Application::CampusDashboardSelectedCampusReadResult::failure(
                snapshot.error()
                );
        }
        return Application::CampusDashboardSelectedCampusReadResult::success(
            snapshot.value()
            );
    }

    [[nodiscard]] Domain::Result<void> saveCampus(
        const Application::CampusDashboardCampusSnapshot& campus,
        const Application::CampusDashboardCampusSaveIntent intent,
        const Application::CampusDashboardCampusNameOrigin nameOrigin =
            Application::CampusDashboardCampusNameOrigin::UserEntered
        ) const override
    {
        const Domain::Result<void> identityValidation =
            Application::CampusDashboardCampusIdentitySavePolicy::validate(
                campus,
                intent,
                nameOrigin
                );
        if (!identityValidation)
        {
            return identityValidation;
        }
        if (!validSnapshotJson(campus))
        {
            return Domain::Result<void>::failure({
                .code = Domain::ErrorCode::InvalidInput,
                .message = "Campus address and housing data must contain valid JSON values.",
                .recoverable = false
            });
        }

        const auto current = loadEffectiveRecords();
        if (!current)
        {
            return Domain::Result<void>::failure(current.error());
        }
        const QString id = QString::fromUtf8(
            campus.id.value().data(),
            static_cast<qsizetype>(campus.id.value().size())
            );
        const bool exists = current.value().contains(id);
        if (intent == Application::CampusDashboardCampusSaveIntent::Create && exists)
        {
            return Domain::Result<void>::failure({
                .code = Domain::ErrorCode::Conflict,
                .message = "A campus with this identifier already exists.",
                .recoverable = true
            });
        }
        if (intent == Application::CampusDashboardCampusSaveIntent::Update && !exists)
        {
            return Domain::Result<void>::failure({
                .code = Domain::ErrorCode::NotFound,
                .message = "This campus no longer exists. Reload it before saving.",
                .recoverable = true
            });
        }

        QJsonObject object = exists ? current.value().at(id) : QJsonObject{};
        updateJsonFromSnapshot(object, campus);
        return writeOverride(id, object);
    }

    // This explicit action reads only <process cwd>/data/campuses. It does not
    // probe other roots, and every imported record is written as an atomic
    // app-data override keyed by its embedded campus ID.
    [[nodiscard]] Domain::Result<std::size_t> importLegacyOverridesFromProcessWorkingDirectory() const
    {
        const QString sourceDirectory = QDir::current().filePath(
            QStringLiteral("data/campuses")
            );
        const QDir directory(sourceDirectory);
        if (!directory.exists())
        {
            return Domain::Result<std::size_t>::failure({
                .code = Domain::ErrorCode::NotFound,
                .message = "The historical data/campuses directory is not present in the current working directory.",
                .recoverable = true
            });
        }

        const QStringList files = directory.entryList(
            {QStringLiteral("*.json")},
            QDir::Files,
            QDir::Name
            );
        std::size_t imported = 0;
        for (const QString& fileName : files)
        {
            const QString path = directory.filePath(fileName);
            QJsonObject object;
            const Domain::Result<QJsonObject> loaded = readJsonObject(path);
            if (!loaded)
            {
                return Domain::Result<std::size_t>::failure(loaded.error());
            }
            object = loaded.value();
            const auto snapshot = snapshotFromJson(object);
            if (!snapshot)
            {
                return Domain::Result<std::size_t>::failure(snapshot.error());
            }
            if (isReservedDefault(snapshot.value()))
            {
                continue;
            }

            const QString id = QString::fromUtf8(
                snapshot.value().id.value().data(),
                static_cast<qsizetype>(snapshot.value().id.value().size())
                );
            const Domain::Result<void> saved = writeOverride(id, object);
            if (!saved)
            {
                return Domain::Result<std::size_t>::failure(saved.error());
            }
            ++imported;
        }
        return Domain::Result<std::size_t>::success(imported);
    }

    [[nodiscard]] Domain::Result<std::vector<Resources::ResourceId>> mapResourcesFor(
        const Application::CampusDashboardCampusSnapshot& campus
        ) const
    {
        std::vector<Resources::ResourceId> resources;
        std::vector<std::string> references = campus.mapImagePaths;
        if (references.empty() && !campus.imageMain.empty())
        {
            references.push_back(campus.imageMain);
        }
        for (const std::string& reference : references)
        {
            const QString path = QString::fromUtf8(
                reference.data(),
                static_cast<qsizetype>(reference.size())
                );
            QString logicalId;
            if (path.startsWith(QStringLiteral(":/assets/")))
            {
                logicalId = QStringLiteral("resource:") + path.mid(2);
            }
            else if (path.startsWith(QStringLiteral("resource:assets/")))
            {
                logicalId = path;
            }
            else
            {
                return Domain::Result<std::vector<Resources::ResourceId>>::failure({
                    .code = Domain::ErrorCode::Validation,
                    .message = "Campus map reference is not a packaged campus resource.",
                    .recoverable = true
                });
            }

            const QString campusResourcePrefix = QStringLiteral("resource:assets/campuses/%1/")
                .arg(QString::fromUtf8(
                    campus.id.value().data(),
                    static_cast<qsizetype>(campus.id.value().size())
                    ));
            if (!logicalId.startsWith(campusResourcePrefix))
            {
                return Domain::Result<std::vector<Resources::ResourceId>>::failure({
                    .code = Domain::ErrorCode::Validation,
                    .message = "Campus map reference must belong to the selected campus package.",
                    .recoverable = true
                });
            }

            const auto id = Resources::ResourceId::fromString(logicalId);
            if (!id)
            {
                return Domain::Result<std::vector<Resources::ResourceId>>::failure({
                    .code = Domain::ErrorCode::Validation,
                    .message = "Campus map reference has an invalid resource identifier.",
                    .recoverable = true
                });
            }
            const Resources::ResourceDescriptor* descriptor =
                m_resourceLoader->catalog().find(*id);
            if (!descriptor || descriptor->expectedType() != Resources::ResourceType::Png)
            {
                return Domain::Result<std::vector<Resources::ResourceId>>::failure({
                    .code = Domain::ErrorCode::NotFound,
                    .message = "Campus map image is missing from the packaged resource catalog.",
                    .recoverable = true
                });
            }
            if (std::find(resources.cbegin(), resources.cend(), *id) == resources.cend())
            {
                resources.push_back(*id);
            }
        }
        return Domain::Result<std::vector<Resources::ResourceId>>::success(
            std::move(resources)
            );
    }

private:
    using RawCampusRecords = std::map<QString, QJsonObject>;

    [[nodiscard]] static std::string utf8(const QString& value)
    {
        const QByteArray bytes = value.toUtf8();
        return std::string(bytes.constData(), static_cast<std::size_t>(bytes.size()));
    }

    [[nodiscard]] static QString fromUtf8(const std::string& value)
    {
        return QString::fromUtf8(value.data(), static_cast<qsizetype>(value.size()));
    }

    [[nodiscard]] Domain::Result<RawCampusRecords> loadEffectiveRecords() const
    {
        const auto scopeRequest = Resources::ResourceScope::create(
            Resources::ResourceScopeKind::Feature,
            QStringLiteral("campus-directory")
            );
        if (!scopeRequest)
        {
            return Domain::Result<RawCampusRecords>::failure({
                .code = Domain::ErrorCode::Technical,
                .message = scopeRequest.error().message.toStdString(),
                .recoverable = false
            });
        }

        RawCampusRecords records;
        for (const Resources::ResourceDescriptor& descriptor :
             m_resourceLoader->catalog().records())
        {
            if (descriptor.resourceKind() != QStringLiteral("campus_data")
                || descriptor.expectedType() != Resources::ResourceType::Json)
            {
                continue;
            }
            const auto request = Resources::ResourceRequest::create(
                descriptor.id(),
                *scopeRequest
                );
            if (!request)
            {
                return Domain::Result<RawCampusRecords>::failure({
                    .code = Domain::ErrorCode::Technical,
                    .message = request.error().message.toStdString(),
                    .recoverable = false
                });
            }
            const auto loaded = m_resourceLoader->loadJson(*request);
            if (!loaded)
            {
                return Domain::Result<RawCampusRecords>::failure({
                    .code = Domain::ErrorCode::Technical,
                    .message = loaded.error().message.toStdString(),
                    .recoverable = true
                });
            }
            const QJsonObject object = loaded->get().object();
            const auto snapshot = snapshotFromJson(object);
            if (!snapshot)
            {
                return Domain::Result<RawCampusRecords>::failure(snapshot.error());
            }
            const QString id = QString::fromUtf8(
                snapshot.value().id.value().data(),
                static_cast<qsizetype>(snapshot.value().id.value().size())
                );
            if (records.contains(id))
            {
                return Domain::Result<RawCampusRecords>::failure({
                    .code = Domain::ErrorCode::Validation,
                    .message = "Packaged campus catalog contains duplicate identifiers.",
                    .recoverable = false
                });
            }
            records.emplace(id, object);
        }

        const QDir overrideDirectory(m_overrideDirectory);
        if (!overrideDirectory.exists())
        {
            return Domain::Result<RawCampusRecords>::success(std::move(records));
        }
        const QStringList overrideFiles = overrideDirectory.entryList(
            {QStringLiteral("campus-*.json")},
            QDir::Files,
            QDir::Name
            );
        for (const QString& fileName : overrideFiles)
        {
            const auto loaded = readJsonObject(overrideDirectory.filePath(fileName));
            if (!loaded)
            {
                return Domain::Result<RawCampusRecords>::failure(loaded.error());
            }
            const auto snapshot = snapshotFromJson(loaded.value());
            if (!snapshot)
            {
                return Domain::Result<RawCampusRecords>::failure(snapshot.error());
            }
            const QString id = QString::fromUtf8(
                snapshot.value().id.value().data(),
                static_cast<qsizetype>(snapshot.value().id.value().size())
                );
            records.insert_or_assign(id, loaded.value());
        }

        return Domain::Result<RawCampusRecords>::success(std::move(records));
    }

    [[nodiscard]] static Domain::Result<QJsonObject> readJsonObject(
        const QString& path
        )
    {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly))
        {
            return Domain::Result<QJsonObject>::failure({
                .code = Domain::ErrorCode::Technical,
                .message = "Campus record could not be opened: " + file.errorString().toStdString(),
                .recoverable = true
            });
        }
        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
        if (parseError.error != QJsonParseError::NoError || !document.isObject())
        {
            return Domain::Result<QJsonObject>::failure({
                .code = Domain::ErrorCode::Validation,
                .message = "Campus record is not a valid JSON object.",
                .recoverable = true
            });
        }
        return Domain::Result<QJsonObject>::success(document.object());
    }

    [[nodiscard]] static Domain::Result<Application::CampusDashboardCampusSnapshot>
    snapshotFromJson(const QJsonObject& object)
    {
        const auto id = Domain::CampusId::fromString(
            utf8(object.value(QStringLiteral("id")).toString()).c_str()
            );
        const QString campusName = object.value(QStringLiteral("campus_name")).toString();
        if (!id || campusName.trimmed().isEmpty())
        {
            return Domain::Result<Application::CampusDashboardCampusSnapshot>::failure({
                .code = Domain::ErrorCode::Validation,
                .message = "Campus record requires an identifier and campus name.",
                .recoverable = true
            });
        }

        const QJsonObject directions = object.value(QStringLiteral("directions")).toObject();
        const QJsonObject englishAddress = directions.value(QStringLiteral("en")).toObject();
        const QJsonObject koreanAddress = directions.value(QStringLiteral("kr")).toObject();
        const QJsonObject map = object.value(QStringLiteral("map")).toObject();
        const QJsonObject links = map.value(QStringLiteral("links")).toObject();
        std::vector<std::string> images;
        const QJsonArray imageArray = map.value(QStringLiteral("images")).toArray();
        images.reserve(static_cast<std::size_t>(imageArray.size()));
        for (const QJsonValue& image : imageArray)
        {
            if (image.isString() && !image.toString().trimmed().isEmpty())
            {
                images.push_back(utf8(image.toString()));
            }
        }
        if (images.empty())
        {
            const QString mainImage = object.value(QStringLiteral("image_main")).toString();
            if (!mainImage.trimmed().isEmpty())
            {
                images.push_back(utf8(mainImage));
            }
        }
        std::vector<std::string> transitSteps;
        const QJsonArray transit = object.value(QStringLiteral("transit_steps")).toArray();
        transitSteps.reserve(static_cast<std::size_t>(transit.size()));
        for (const QJsonValue& step : transit)
        {
            if (step.isString()) transitSteps.push_back(utf8(step.toString()));
        }
        const QJsonDocument housing(object.value(QStringLiteral("housing_locations")).toArray());
        return Domain::Result<Application::CampusDashboardCampusSnapshot>::success({
            .id = *id,
            .campusName = utf8(campusName),
            .campusCode = utf8(object.value(QStringLiteral("campus_code")).toString()),
            .buildingName = utf8(
                englishAddress.value(QStringLiteral("building_name")).toString(
                    object.value(QStringLiteral("building_name")).toString()
                    )
                ),
            .buildingNameKr = utf8(koreanAddress.value(QStringLiteral("building_name")).toString()),
            .address = utf8(object.value(QStringLiteral("address")).toString()),
            .phoneNumber = utf8(object.value(QStringLiteral("phone_number")).toString()),
            .officeNumber = utf8(object.value(QStringLiteral("office_number")).toString()),
            .directionsAddressEnJson = utf8(QJsonDocument(englishAddress).toJson(QJsonDocument::Compact)),
            .directionsAddressKrJson = utf8(QJsonDocument(koreanAddress).toJson(QJsonDocument::Compact)),
            .directionsNote = utf8(directions.value(QStringLiteral("addr_note")).toString()),
            .transitSteps = std::move(transitSteps),
            .arrivalInfo = utf8(object.value(QStringLiteral("arrival_info")).toString()),
            .imageMain = utf8(object.value(QStringLiteral("image_main")).toString()),
            .mapImagePaths = std::move(images),
            .naverMapUrl = utf8(links.value(QStringLiteral("naver")).toString()),
            .kakaoMapUrl = utf8(links.value(QStringLiteral("kakao")).toString()),
            .officeWifi = utf8(object.value(QStringLiteral("office_wifi")).toString()),
            .officeWifiPassword = utf8(object.value(QStringLiteral("office_wifi_password")).toString()),
            .printerName = utf8(object.value(QStringLiteral("printer_name")).toString()),
            .printerSteps = utf8(object.value(QStringLiteral("printer_steps")).toString()),
            .printerDriverUrl = utf8(object.value(QStringLiteral("printer_driver_url")).toString()),
            .printerDriverUrlUnavailable = object.value(QStringLiteral("printer_driver_url_unavailable")).toBool(true),
            .photocopierCode = utf8(object.value(QStringLiteral("photocopier_code")).toString()),
            .housingLocationsJson = utf8(housing.toJson(QJsonDocument::Compact))
        });
    }

    [[nodiscard]] static bool validSnapshotJson(
        const Application::CampusDashboardCampusSnapshot& campus
        )
    {
        QJsonParseError addressError;
        const QJsonDocument en = QJsonDocument::fromJson(
            QByteArray::fromStdString(campus.directionsAddressEnJson),
            &addressError
            );
        if (addressError.error != QJsonParseError::NoError || !en.isObject()) return false;
        const QJsonDocument kr = QJsonDocument::fromJson(
            QByteArray::fromStdString(campus.directionsAddressKrJson),
            &addressError
            );
        if (addressError.error != QJsonParseError::NoError || !kr.isObject()) return false;
        const QJsonDocument housing = QJsonDocument::fromJson(
            QByteArray::fromStdString(campus.housingLocationsJson),
            &addressError
            );
        return addressError.error == QJsonParseError::NoError && housing.isArray();
    }

    static void updateJsonFromSnapshot(
        QJsonObject& object,
        const Application::CampusDashboardCampusSnapshot& campus
        )
    {
        const QJsonObject english = QJsonDocument::fromJson(
            QByteArray::fromStdString(campus.directionsAddressEnJson)
            ).object();
        const QJsonObject korean = QJsonDocument::fromJson(
            QByteArray::fromStdString(campus.directionsAddressKrJson)
            ).object();
        const QJsonArray images = [&campus]
        {
            QJsonArray array;
            for (const std::string& image : campus.mapImagePaths)
            {
                array.append(fromUtf8(image));
            }
            return array;
        }();
        const QJsonArray transit = [&campus]
        {
            QJsonArray array;
            for (const std::string& step : campus.transitSteps)
            {
                array.append(fromUtf8(step));
            }
            return array;
        }();

        object.insert(QStringLiteral("id"), fromUtf8(campus.id.value()));
        object.insert(QStringLiteral("campus_name"), fromUtf8(campus.campusName));
        object.insert(QStringLiteral("campus_code"), fromUtf8(campus.campusCode));
        object.insert(QStringLiteral("building_name"), fromUtf8(campus.buildingName));
        object.insert(QStringLiteral("address"), fromUtf8(campus.address));
        object.insert(QStringLiteral("phone_number"), fromUtf8(campus.phoneNumber));
        object.insert(QStringLiteral("office_number"), fromUtf8(campus.officeNumber));
        object.insert(QStringLiteral("arrival_info"), fromUtf8(campus.arrivalInfo));
        object.insert(QStringLiteral("office_wifi"), fromUtf8(campus.officeWifi));
        object.insert(QStringLiteral("office_wifi_password"), fromUtf8(campus.officeWifiPassword));
        object.insert(QStringLiteral("printer_name"), fromUtf8(campus.printerName));
        object.insert(QStringLiteral("printer_steps"), fromUtf8(campus.printerSteps));
        object.insert(QStringLiteral("printer_driver_url"), fromUtf8(campus.printerDriverUrl));
        object.insert(QStringLiteral("printer_driver_url_unavailable"), campus.printerDriverUrlUnavailable);
        object.insert(QStringLiteral("photocopier_code"), fromUtf8(campus.photocopierCode));
        object.insert(QStringLiteral("transit_steps"), transit);
        object.insert(QStringLiteral("housing_locations"), QJsonDocument::fromJson(
            QByteArray::fromStdString(campus.housingLocationsJson)
            ).array());

        QJsonObject directions = object.value(QStringLiteral("directions")).toObject();
        QJsonObject englishWithName = english;
        englishWithName.insert(QStringLiteral("building_name"), fromUtf8(campus.buildingName));
        QJsonObject koreanWithName = korean;
        koreanWithName.insert(QStringLiteral("building_name"), fromUtf8(campus.buildingNameKr));
        directions.insert(QStringLiteral("en"), englishWithName);
        directions.insert(QStringLiteral("kr"), koreanWithName);
        directions.insert(QStringLiteral("addr_note"), fromUtf8(campus.directionsNote));
        object.insert(QStringLiteral("directions"), directions);

        QJsonObject map = object.value(QStringLiteral("map")).toObject();
        map.insert(QStringLiteral("images"), images);
        QJsonObject links = map.value(QStringLiteral("links")).toObject();
        links.insert(QStringLiteral("naver"), fromUtf8(campus.naverMapUrl));
        links.insert(QStringLiteral("kakao"), fromUtf8(campus.kakaoMapUrl));
        map.insert(QStringLiteral("links"), links);
        object.insert(QStringLiteral("map"), map);
        object.insert(
            QStringLiteral("image_main"),
            campus.mapImagePaths.empty()
                ? fromUtf8(campus.imageMain)
                : fromUtf8(campus.mapImagePaths.front())
            );
    }

    [[nodiscard]] static bool isReservedDefault(
        const Application::CampusDashboardCampusSnapshot& campus
        )
    {
        return QString::fromUtf8(campus.id.value().c_str()).compare(
                   QStringLiteral("default"), Qt::CaseInsensitive
                   ) == 0
            || QString::fromUtf8(campus.campusName.data(),
                   static_cast<qsizetype>(campus.campusName.size())).compare(
                   QStringLiteral("Default"), Qt::CaseInsensitive
                   ) == 0;
    }

    [[nodiscard]] static bool isReservedDefaultObject(const QJsonObject& object)
    {
        return object.value(QStringLiteral("id")).toString().compare(
                   QStringLiteral("default"), Qt::CaseInsensitive
                   ) == 0
            || object.value(QStringLiteral("campus_name")).toString().compare(
                   QStringLiteral("Default"), Qt::CaseInsensitive
                   ) == 0;
    }

    [[nodiscard]] QString overrideFilePath(const QString& id) const
    {
        const QByteArray encoded = id.toUtf8();
        const QByteArray digest = QCryptographicHash::hash(
            encoded,
            QCryptographicHash::Sha256
            ).toHex();
        return QDir(m_overrideDirectory).filePath(
            QStringLiteral("campus-") + QString::fromLatin1(digest)
                + QStringLiteral(".json")
            );
    }

    [[nodiscard]] Domain::Result<void> writeOverride(
        const QString& id,
        const QJsonObject& object
        ) const
    {
        if (id.trimmed().isEmpty() || id.toUtf8().size() > 256)
        {
            return Domain::Result<void>::failure({
                .code = Domain::ErrorCode::InvalidInput,
                .message = "Campus identifier must be non-blank and bounded.",
                .recoverable = false
            });
        }
        if (!QDir().mkpath(m_overrideDirectory))
        {
            return Domain::Result<void>::failure({
                .code = Domain::ErrorCode::Technical,
                .message = "Campus app-data directory could not be created.",
                .recoverable = true
            });
        }
        QSaveFile file(overrideFilePath(id));
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        {
            return Domain::Result<void>::failure({
                .code = Domain::ErrorCode::Technical,
                .message = "Campus override could not be opened for writing: "
                    + file.errorString().toStdString(),
                .recoverable = true
            });
        }
        const QByteArray json = QJsonDocument(object).toJson(QJsonDocument::Indented);
        if (file.write(json) != json.size() || !file.commit())
        {
            file.cancelWriting();
            return Domain::Result<void>::failure({
                .code = Domain::ErrorCode::Technical,
                .message = "Campus override could not be committed atomically: "
                    + file.errorString().toStdString(),
                .recoverable = true
            });
        }
        return Domain::Result<void>::success();
    }

    Resources::ResourceLoader* m_resourceLoader;
    QString m_overrideDirectory;
};

} // namespace ClassMngr::Next::Platform
