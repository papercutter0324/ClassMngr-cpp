#include "next/application/campus_dashboard_campus_save_use_case.h"
#include "next/application/campus_legacy_import_use_case.h"
#include "next/bootstrap/campus_edit_operation_actions.h"
#include "next/application/campus_dashboard_selected_campus_read_query.h"
#include "next/application/campus_directory_read_query.h"
#include "next/bootstrap/campus_edit_capability_arguments.h"
#include "next/platform/campus_directory_repository_adapter.h"
#include "next/resources/resource_catalog.h"
#include "next/resources/resource_loader.h"
#include "next/resources/resource_scope.h"
#include "next/ui/campus_directory_page.h"
#include "next/ui/campus_directory_route_factories.h"
#include "next/ui/page_host.h"
#include "preferences_test_ports.h"

#include <QApplication>
#include <QComboBox>
#include <QCryptographicHash>
#include <QCheckBox>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTemporaryDir>
#include <QTest>

#include <algorithm>
#include <optional>
#include <string>
#include <vector>

using namespace ClassMngr::Next;

namespace
{

Domain::CampusId campusId(const char* value)
{
    return *Domain::CampusId::fromString(value);
}

Application::CampusDashboardCampusSnapshot bundangSnapshot()
{
    return Application::CampusDashboardCampusSnapshot{
        .id = campusId("bundang"),
        .campusName = "Bundang",
        .campusCode = "BDG",
        .buildingName = "XY Building",
        .buildingNameKr = "XY빌딩",
        .address = "Baekhyun-ro 101beon-gil 30",
        .phoneNumber = "031-716-2945",
        .officeNumber = "418",
        .directionsAddressEnJson = "{\"line1\":\"Baekhyun-ro 101beon-gil 30\"}",
        .directionsAddressKrJson = "{\"line1\":\"백현로101번길30\"}",
        .directionsNote = "Use the east entrance.",
        .transitSteps = {"Take the Suin-Bundang line.", "Exit via exit 2."},
        .arrivalInfo = "Sign in at reception.",
        .mapImagePaths = {
            ":/assets/campuses/bundang/bundang_map.png",
            ":/assets/campuses/bundang/bundang_building.png"
        },
        .naverMapUrl = "https://map.naver.com/p/entry/place/37093029",
        .kakaoMapUrl = "https://place.map.kakao.com/12327306",
        .officeWifi = "Native Room_5G",
        .officeWifiPassword = "wifi-secret",
        .printerName = "Bundang Office",
        .printerSteps = "Install the driver.",
        .printerDriverUrl = "",
        .printerDriverUrlUnavailable = true,
        .photocopierCode = "N/A",
        .housingLocationsJson = "[]"
    };
}

QString testResourcePath(const char* macroPath)
{
    return QString::fromUtf8(macroPath);
}

std::optional<Resources::ResourceCatalog> loadCatalog()
{
    const auto loaded = Resources::ResourceCatalog::load(
        testResourcePath(CLASSMNGR_NEXT_TEST_RESOURCE_CATALOG),
        testResourcePath(CLASSMNGR_NEXT_TEST_RESOURCE_ROOT)
        );
    if (!loaded) return std::nullopt;
    return *loaded;
}

QJsonObject legacyRecord(const QString& id, const QString& name)
{
    return QJsonObject{
        {QStringLiteral("id"), id},
        {QStringLiteral("campus_name"), name},
        {QStringLiteral("campus_code"), QStringLiteral("OLD")},
        {QStringLiteral("building_name"), QStringLiteral("Legacy Hall")},
        {QStringLiteral("address"), QStringLiteral("Legacy Street")},
        {QStringLiteral("directions"), QJsonObject{
             {QStringLiteral("en"), QJsonObject{
                  {QStringLiteral("building_name"), QStringLiteral("Legacy Hall")},
                  {QStringLiteral("line1"), QStringLiteral("Legacy Street")}
              }},
             {QStringLiteral("kr"), QJsonObject{
                  {QStringLiteral("building_name"), QStringLiteral("구관")},
                  {QStringLiteral("line1"), QStringLiteral("옛길")}
              }}
         }},
        {QStringLiteral("map"), QJsonObject{
             {QStringLiteral("images"), QJsonArray{}},
             {QStringLiteral("links"), QJsonObject{}}
         }},
        {QStringLiteral("housing_locations"), QJsonArray{}}
    };
}

QString overrideFilePath(const QString& directory, const QString& campusIdValue)
{
    const QByteArray digest = QCryptographicHash::hash(
        campusIdValue.toUtf8(),
        QCryptographicHash::Sha256
        ).toHex();
    return QDir(directory).filePath(
        QStringLiteral("campus-") + QString::fromLatin1(digest)
            + QStringLiteral(".json")
        );
}

Ui::CampusDirectoryPageActions actionsFor(
    Application::CampusEditCapability capability,
    const std::function<Domain::Result<void>(
        const Application::CampusDashboardCampusSnapshot&,
        Application::CampusDashboardCampusSaveIntent)>& save = {},
    const std::function<void(const Resources::ResourceDiagnostic&)>& diagnostic = {}
    )
{
    const auto snapshot = bundangSnapshot();
    Ui::CampusDirectoryPageActions actions;
    actions.editCapability = capability;
    actions.loadCampuses = [snapshot] {
        return Application::CampusDirectoryReadResult::success({
            Application::CampusDirectoryEntry{
                .id = snapshot.id,
                .displayName = snapshot.campusName
            }
        });
    };
    actions.loadCampus = [snapshot](const Domain::CampusId& id) {
        if (id != snapshot.id)
        {
            return Application::CampusDashboardSelectedCampusReadResult::success(
                std::nullopt
                );
        }
        return Application::CampusDashboardSelectedCampusReadResult::success(
            snapshot
            );
    };
    actions.saveCampus = save
        ? save
        : [](const Application::CampusDashboardCampusSnapshot&,
             Application::CampusDashboardCampusSaveIntent)
        {
            return Domain::Result<void>::success();
        };
    actions.readLastSelectedCampus = [] { return std::optional<Domain::CampusId>{}; };
    actions.saveLastSelectedCampus = [](const std::optional<Domain::CampusId>&) {
        return Domain::Result<void>::success();
    };
    actions.importLegacyCampuses = [] {
        return Domain::Result<std::size_t>::success(0);
    };
    actions.mapResourcesFor = [snapshot](
                                  const Application::CampusDashboardCampusSnapshot& campus
                                  ) {
        if (campus.id != snapshot.id)
        {
            return Domain::Result<std::vector<Resources::ResourceId>>::success({});
        }
        std::vector<Resources::ResourceId> resources;
        for (const QString& value : {
                 QStringLiteral("resource:assets/campuses/bundang/bundang_map.png"),
                 QStringLiteral("resource:assets/campuses/bundang/bundang_building.png")
             })
        {
            const auto id = Resources::ResourceId::fromString(value);
            if (id) resources.push_back(*id);
        }
        return Domain::Result<std::vector<Resources::ResourceId>>::success(
            std::move(resources)
            );
    };
    static_cast<void>(diagnostic);
    return actions;
}

class CampusDirectoryTests final : public QObject
{
    Q_OBJECT

private slots:
    void packagedDirectoryLoadsAndIdOverrideWinsAfterReload();
    void explicitMigrationReadsOnlyCurrentWorkingDataCampuses();
    void productionAdminArgumentMapsToCampusEditCapabilityByPresence();
    void composedCampusEditOperationsEnforceCapabilityAndAuthorizedParity();
    void realQSaveFileFailureRetainsDirtyDraftAndRetrySucceeds();
    void packagedMapReferencesAreTypedAndImagesStayWithinBound();
    void savedCampusSelectionAndFirstAvailableFallback();
    void allFiveRoutesRenderAndReadOnlyDoesNotExposeEditing();
    void failedSaveRetainsDirtyDraftForRetry();
    void mapsLoadOnlyOnMapRouteReleaseOnLeaveAndReopen();
    void koreanTranslationAndApplicationPaletteReachRoutes();

private:
    QTemporaryDir m_temporaryDirectory;
};

class CampusEditOperationProbe final
    : public Application::CampusDashboardCampusSavePort
    , public Application::CampusLegacyImportPort
{
public:
    explicit CampusEditOperationProbe(
        Application::CampusDashboardCampusSnapshot initial
        )
        : campuses{std::move(initial)}
    {
    }

    [[nodiscard]] Domain::Result<void> saveCampus(
        const Application::CampusDashboardCampusSnapshot& campus,
        const Application::CampusDashboardCampusSaveIntent intent,
        const Application::CampusDashboardCampusNameOrigin nameOrigin =
            Application::CampusDashboardCampusNameOrigin::UserEntered
        ) const override
    {
        ++saveCalls;
        if (failSave)
        {
            return Domain::Result<void>::failure({
                .code = Domain::ErrorCode::Technical,
                .message = "injected save failure",
                .recoverable = true
            });
        }
        saveIntents.push_back(intent);
        nameOrigins.push_back(nameOrigin);
        const auto found = std::find_if(
            campuses.begin(),
            campuses.end(),
            [&campus](const auto& item) { return item.id == campus.id; }
            );
        if (found == campuses.end())
        {
            campuses.push_back(campus);
        }
        else
        {
            *found = campus;
        }
        return Domain::Result<void>::success();
    }

    [[nodiscard]] Domain::Result<std::size_t> importLegacyCampuses() const override
    {
        ++importCalls;
        if (failImport)
        {
            return Domain::Result<std::size_t>::failure({
                .code = Domain::ErrorCode::Technical,
                .message = "injected import failure",
                .recoverable = true
            });
        }
        campuses.push_back(importedCampus);
        return Domain::Result<std::size_t>::success(1);
    }

    mutable std::vector<Application::CampusDashboardCampusSnapshot> campuses;
    mutable std::vector<Application::CampusDashboardCampusSaveIntent> saveIntents;
    mutable std::vector<Application::CampusDashboardCampusNameOrigin> nameOrigins;
    mutable int saveCalls = 0;
    mutable int importCalls = 0;
    bool failSave = false;
    bool failImport = false;
    Application::CampusDashboardCampusSnapshot importedCampus = [] {
        auto campus = bundangSnapshot();
        campus.id = campusId("imported-campus");
        campus.campusName = "Imported Campus";
        return campus;
    }();
};

void CampusDirectoryTests::packagedDirectoryLoadsAndIdOverrideWinsAfterReload()
{
    QVERIFY(m_temporaryDirectory.isValid());
    auto catalog = loadCatalog();
    QVERIFY(catalog.has_value());
    Resources::ResourceLoader loader(std::move(*catalog));
    auto repository = std::make_shared<Platform::CampusDirectoryRepositoryAdapter>(
        loader,
        QDir(m_temporaryDirectory.path()).filePath(QStringLiteral("app-data/campuses"))
        );

    const auto directory = Application::CampusDirectoryReadQuery::execute(*repository);
    QVERIFY(directory);
    const auto packaged = std::find_if(
        directory.value().cbegin(),
        directory.value().cend(),
        [](const Application::CampusDirectoryEntry& entry)
        {
            return entry.id.value() == "bundang";
        }
        );
    QVERIFY(packaged != directory.value().cend());
    QCOMPARE(packaged->displayName, std::string("Bundang"));

    const auto selected = Application::CampusDashboardSelectedCampusReadQuery::execute(
        Application::CampusDashboardSelectedCampusReadRequest{campusId("bundang")},
        *repository
        );
    QVERIFY(selected);
    QVERIFY(selected.value().has_value());
    auto edited = *selected.value();
    edited.campusName = "Bundang Override";
    const Bootstrap::CampusSaveAction saveAction = Bootstrap::campusSaveAction(
        Application::CampusEditCapability::enabled(),
        repository
        );
    const auto saved = saveAction(
        edited,
        Application::CampusDashboardCampusSaveIntent::Update
        );
    QVERIFY(saved);

    const auto reloadedDirectory = Application::CampusDirectoryReadQuery::execute(*repository);
    QVERIFY(reloadedDirectory);
    const auto overrideEntry = std::find_if(
        reloadedDirectory.value().cbegin(),
        reloadedDirectory.value().cend(),
        [](const Application::CampusDirectoryEntry& entry)
        {
            return entry.id.value() == "bundang";
        }
        );
    QVERIFY(overrideEntry != reloadedDirectory.value().cend());
    QCOMPARE(overrideEntry->displayName, std::string("Bundang Override"));
    const auto reloadedSnapshot = Application::CampusDashboardSelectedCampusReadQuery::execute(
        Application::CampusDashboardSelectedCampusReadRequest{campusId("bundang")},
        *repository
        );
    QVERIFY(reloadedSnapshot);
    QCOMPARE(reloadedSnapshot.value()->campusName, std::string("Bundang Override"));

    auto createdCampus = bundangSnapshot();
    createdCampus.id = campusId("alpha");
    createdCampus.campusName = "Alpha Campus";
    const auto created = saveAction(
        createdCampus,
        Application::CampusDashboardCampusSaveIntent::Create
        );
    QVERIFY(created);
    const auto reloadedCreatedCampus =
        Application::CampusDashboardSelectedCampusReadQuery::execute(
            Application::CampusDashboardSelectedCampusReadRequest{campusId("alpha")},
            *repository
            );
    QVERIFY(reloadedCreatedCampus);
    QVERIFY(reloadedCreatedCampus.value().has_value());
    QCOMPARE(reloadedCreatedCampus.value()->campusName, std::string("Alpha Campus"));

    const QStringList files = QDir(
        QDir(m_temporaryDirectory.path()).filePath(QStringLiteral("app-data/campuses"))
        ).entryList({QStringLiteral("campus-*.json")}, QDir::Files);
    QCOMPARE(files.size(), 2);
}

void CampusDirectoryTests::explicitMigrationReadsOnlyCurrentWorkingDataCampuses()
{
    QTemporaryDir migrationTemporaryDirectory;
    QVERIFY(migrationTemporaryDirectory.isValid());
    auto catalog = loadCatalog();
    QVERIFY(catalog.has_value());
    Resources::ResourceLoader loader(std::move(*catalog));
    const QString overrideDirectory = QDir(migrationTemporaryDirectory.path()).filePath(
        QStringLiteral("app-data/campuses")
        );
    auto repository = std::make_shared<Platform::CampusDirectoryRepositoryAdapter>(
        loader,
        overrideDirectory
        );

    const QString priorWorkingDirectory = QDir::currentPath();
    const QString migrationRoot = QDir(migrationTemporaryDirectory.path()).filePath(
        QStringLiteral("migration-root")
        );
    QVERIFY(QDir().mkpath(QDir(migrationRoot).filePath(QStringLiteral("data/campuses"))));
    QVERIFY(QDir().mkpath(QDir(migrationRoot).filePath(QStringLiteral("elsewhere/campuses"))));
    QFile knownInput(QDir(migrationRoot).filePath(QStringLiteral("data/campuses/legacy.json")));
    QVERIFY(knownInput.open(QIODevice::WriteOnly));
    QVERIFY(knownInput.write(QJsonDocument(legacyRecord(
        QStringLiteral("legacy-campus"), QStringLiteral("Legacy Campus")
        )).toJson()) > 0);
    knownInput.close();
    QFile ignoredInput(QDir(migrationRoot).filePath(QStringLiteral("elsewhere/campuses/ignored.json")));
    QVERIFY(ignoredInput.open(QIODevice::WriteOnly));
    QVERIFY(ignoredInput.write(QJsonDocument(legacyRecord(
        QStringLiteral("ignored-campus"), QStringLiteral("Ignored Campus")
        )).toJson()) > 0);
    ignoredInput.close();

    const auto deniedImportAction = Bootstrap::campusLegacyImportAction(
        Application::CampusEditCapability::readOnly(),
        repository
        );
    QVERIFY(QDir::setCurrent(migrationRoot));
    const auto deniedImport = deniedImportAction();
    QVERIFY(QDir::setCurrent(priorWorkingDirectory));
    QVERIFY(!deniedImport);
    QCOMPARE(deniedImport.error().code, Domain::ErrorCode::InvalidInput);
    QVERIFY(!deniedImport.error().recoverable);
    QVERIFY(!QFileInfo::exists(overrideDirectory));

    const auto authorizedImportAction = Bootstrap::campusLegacyImportAction(
        Application::CampusEditCapability::enabled(),
        repository
        );
    QVERIFY(QDir::setCurrent(migrationRoot));
    const auto imported = authorizedImportAction();
    QVERIFY(QDir::setCurrent(priorWorkingDirectory));
    QVERIFY(imported);
    QCOMPARE(imported.value(), std::size_t(1));

    const auto legacy = Application::CampusDashboardSelectedCampusReadQuery::execute(
        Application::CampusDashboardSelectedCampusReadRequest{campusId("legacy-campus")},
        *repository
        );
    QVERIFY(legacy);
    QVERIFY(legacy.value().has_value());
    QCOMPARE(legacy.value()->campusName, std::string("Legacy Campus"));

    const auto ignored = Application::CampusDashboardSelectedCampusReadQuery::execute(
        Application::CampusDashboardSelectedCampusReadRequest{campusId("ignored-campus")},
        *repository
        );
    QVERIFY(ignored);
    QVERIFY(!ignored.value().has_value());

    QTemporaryDir invalidMigrationRoot;
    QVERIFY(invalidMigrationRoot.isValid());
    const QString invalidImportDirectory = QDir(invalidMigrationRoot.path()).filePath(
        QStringLiteral("data/campuses")
        );
    QVERIFY(QDir().mkpath(invalidImportDirectory));
    QFile invalidInput(QDir(invalidImportDirectory).filePath(
        QStringLiteral("invalid.json")
        ));
    QVERIFY(invalidInput.open(QIODevice::WriteOnly));
    QVERIFY(invalidInput.write("not valid JSON") > 0);
    invalidInput.close();

    const QString failedOverrideDirectory = QDir(invalidMigrationRoot.path()).filePath(
        QStringLiteral("app-data/campuses")
        );
    auto failedRepository =
        std::make_shared<Platform::CampusDirectoryRepositoryAdapter>(
            loader,
            failedOverrideDirectory
            );
    const auto failedImportAction = Bootstrap::campusLegacyImportAction(
        Application::CampusEditCapability::enabled(),
        failedRepository
        );
    QVERIFY(QDir::setCurrent(invalidMigrationRoot.path()));
    const auto failedImport = failedImportAction();
    QVERIFY(QDir::setCurrent(priorWorkingDirectory));
    QVERIFY(!failedImport);
    QVERIFY(!QFileInfo::exists(failedOverrideDirectory));
    const auto failedImportedCampus =
        Application::CampusDashboardSelectedCampusReadQuery::execute(
            Application::CampusDashboardSelectedCampusReadRequest{
                campusId("legacy-campus")
            },
            *failedRepository
            );
    QVERIFY(failedImportedCampus);
    QVERIFY(!failedImportedCampus.value().has_value());
}

void CampusDirectoryTests::productionAdminArgumentMapsToCampusEditCapabilityByPresence()
{
    const Application::CampusEditCapability absent =
        Bootstrap::campusEditCapabilityForArguments({QStringLiteral("ClassMngrNext")});
    QVERIFY(!absent.canEdit());

    const Application::CampusEditCapability present =
        Bootstrap::campusEditCapabilityForArguments({
            QStringLiteral("ClassMngrNext"),
            QString::fromLatin1(AppSettings::AdminModeArgument)
        });
    QVERIFY(present.canEdit());
}

void CampusDirectoryTests::composedCampusEditOperationsEnforceCapabilityAndAuthorizedParity()
{
    auto port = std::make_shared<CampusEditOperationProbe>(bundangSnapshot());
    const auto originalCampuses = port->campuses;
    const auto readOnlyCapability = Application::CampusEditCapability::readOnly();
    const Bootstrap::CampusSaveAction readOnlySave =
        Bootstrap::campusSaveAction(readOnlyCapability, port);
    const Bootstrap::CampusLegacyImportAction readOnlyImport =
        Bootstrap::campusLegacyImportAction(readOnlyCapability, port);

    auto newCampus = bundangSnapshot();
    newCampus.id = campusId("new-campus");
    newCampus.campusName = "New Campus";
    const auto deniedCreate = readOnlySave(
        newCampus,
        Application::CampusDashboardCampusSaveIntent::Create
        );
    QVERIFY(!deniedCreate);
    QCOMPARE(deniedCreate.error().code, Domain::ErrorCode::InvalidInput);
    QVERIFY(!deniedCreate.error().recoverable);
    QCOMPARE(port->saveCalls, 0);
    QVERIFY(port->saveIntents.empty());
    QCOMPARE(port->campuses, originalCampuses);

    auto editedBundang = bundangSnapshot();
    editedBundang.campusName = "Unauthorized Update";
    const auto deniedUpdate = readOnlySave(
        editedBundang,
        Application::CampusDashboardCampusSaveIntent::Update
        );
    QVERIFY(!deniedUpdate);
    QCOMPARE(deniedUpdate.error().code, Domain::ErrorCode::InvalidInput);
    QVERIFY(!deniedUpdate.error().recoverable);
    QCOMPARE(port->saveCalls, 0);
    QVERIFY(port->saveIntents.empty());
    QCOMPARE(port->campuses, originalCampuses);

    const auto deniedImport = readOnlyImport();
    QVERIFY(!deniedImport);
    QCOMPARE(deniedImport.error().code, Domain::ErrorCode::InvalidInput);
    QVERIFY(!deniedImport.error().recoverable);
    QCOMPARE(port->importCalls, 0);
    QCOMPARE(port->campuses, originalCampuses);

    const Bootstrap::CampusSaveAction adminSave = Bootstrap::campusSaveAction(
        Application::CampusEditCapability::enabled(),
        port
        );
    const Bootstrap::CampusLegacyImportAction adminImport =
        Bootstrap::campusLegacyImportAction(
            Application::CampusEditCapability::enabled(),
            port
            );
    const auto created = adminSave(
        newCampus,
        Application::CampusDashboardCampusSaveIntent::Create
        );
    QVERIFY(created);
    QCOMPARE(port->saveCalls, 1);
    QCOMPARE(port->saveIntents.back(), Application::CampusDashboardCampusSaveIntent::Create);
    QCOMPARE(
        port->nameOrigins.back(),
        Application::CampusDashboardCampusNameOrigin::UserEntered
        );
    QCOMPARE(port->campuses.size(), std::size_t(2));
    QCOMPARE(port->campuses.back(), newCampus);

    auto updatedCampus = newCampus;
    updatedCampus.campusName = "Updated Campus";
    const auto updated = adminSave(
        updatedCampus,
        Application::CampusDashboardCampusSaveIntent::Update
        );
    QVERIFY(updated);
    QCOMPARE(port->saveCalls, 2);
    QCOMPARE(port->saveIntents.back(), Application::CampusDashboardCampusSaveIntent::Update);
    QCOMPARE(
        port->nameOrigins.back(),
        Application::CampusDashboardCampusNameOrigin::UserEntered
        );
    QCOMPARE(port->campuses.back(), updatedCampus);

    const auto beforeFailedSave = port->campuses;
    port->failSave = true;
    auto failedDraft = updatedCampus;
    failedDraft.campusName = "Failed Save";
    const auto failedSave = adminSave(
        failedDraft,
        Application::CampusDashboardCampusSaveIntent::Update
        );
    QVERIFY(!failedSave);
    QCOMPARE(port->saveCalls, 3);
    QCOMPARE(port->campuses, beforeFailedSave);
    port->failSave = false;

    const auto beforeFailedImport = port->campuses;
    port->failImport = true;
    const auto failedImport = adminImport();
    QVERIFY(!failedImport);
    QCOMPARE(port->importCalls, 1);
    QCOMPARE(port->campuses, beforeFailedImport);
    port->failImport = false;

    const auto imported = adminImport();
    QVERIFY(imported);
    QCOMPARE(imported.value(), std::size_t(1));
    QCOMPARE(port->importCalls, 2);
    QCOMPARE(port->campuses.size(), std::size_t(3));
    QCOMPARE(port->campuses.back(), port->importedCampus);
}

void CampusDirectoryTests::realQSaveFileFailureRetainsDirtyDraftAndRetrySucceeds()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    auto catalog = loadCatalog();
    QVERIFY(catalog.has_value());
    Resources::ResourceLoader loader(std::move(*catalog));
    const QString overrideDirectory = QDir(temporaryDirectory.path()).filePath(
        QStringLiteral("app-data/campuses")
        );
    Platform::CampusDirectoryRepositoryAdapter repository(loader, overrideDirectory);

    auto priorCampus = bundangSnapshot();
    priorCampus.id = campusId("alpha");
    priorCampus.campusName = "Alpha Campus";
    const auto savedPriorCampus = Application::CampusDashboardCampusSaveUseCase::execute(
        Application::CampusEditCapability::enabled(),
        priorCampus,
        Application::CampusDashboardCampusSaveIntent::Create,
        repository
        );
    QVERIFY(savedPriorCampus);
    QFile priorOverride(overrideFilePath(overrideDirectory, QStringLiteral("alpha")));
    QVERIFY(priorOverride.open(QIODevice::ReadOnly));
    const QByteArray priorOverrideBytes = priorOverride.readAll();
    priorOverride.close();
    QVERIFY(!priorOverrideBytes.isEmpty());

    const QString blockedBundangPath = overrideFilePath(
        overrideDirectory,
        QStringLiteral("bundang")
        );
    QVERIFY(QDir().mkpath(blockedBundangPath));
    QVERIFY(QFileInfo(blockedBundangPath).isDir());

    Test::MemoryPreferences preferences;
    Ui::PageHost host(loader, preferences.model);
    const auto descriptors = Ui::campusDirectoryPageDescriptors([&repository] {
        Ui::CampusDirectoryPageActions actions;
        actions.editCapability = Application::CampusEditCapability::enabled();
        actions.loadCampuses = [&repository] {
            return Application::CampusDirectoryReadQuery::execute(repository);
        };
        actions.loadCampus = [&repository](const Domain::CampusId& id) {
            return Application::CampusDashboardSelectedCampusReadQuery::execute(
                Application::CampusDashboardSelectedCampusReadRequest{id},
                repository
                );
        };
        actions.saveCampus = [&repository](
                                 const Application::CampusDashboardCampusSnapshot& campus,
                                 const Application::CampusDashboardCampusSaveIntent intent) {
            return Application::CampusDashboardCampusSaveUseCase::execute(
                Application::CampusEditCapability::enabled(),
                campus,
                intent,
                repository
                );
        };
        actions.readLastSelectedCampus = [] {
            return std::optional<Domain::CampusId>{campusId("bundang")};
        };
        actions.saveLastSelectedCampus = [](const std::optional<Domain::CampusId>&) {
            return Domain::Result<void>::success();
        };
        actions.importLegacyCampuses = [] {
            return Domain::Result<std::size_t>::success(0);
        };
        actions.mapResourcesFor = [&repository](
                                      const Application::CampusDashboardCampusSnapshot& campus) {
            return repository.mapResourcesFor(campus);
        };
        return actions;
    });
    QVERIFY(host.registerPage(descriptors.front()));
    QVERIFY(host.activatePage(QStringLiteral("campus_information")));
    auto* page = dynamic_cast<Ui::CampusDirectoryPage*>(
        host.pageInstance(QStringLiteral("campus_information"))
        );
    QVERIFY(page != nullptr);
    QVERIFY(page->currentCampus().has_value());
    QCOMPARE(page->currentCampus()->id.value(), std::string("bundang"));

    auto* office = page->findChild<QLineEdit*>(QStringLiteral("campusField_officeNumber"));
    QVERIFY(office != nullptr);
    office->clear();
    QTest::keyClicks(office, QStringLiteral("Room 903"));
    QVERIFY(page->isDirty());

    const auto failedSave = page->saveChanges();
    QVERIFY(!failedSave);
    QVERIFY(failedSave.error().message.contains(QStringLiteral("Campus override could not be")));
    QVERIFY(page->isDirty());
    QCOMPARE(office->text(), QStringLiteral("Room 903"));

    QFile unchangedPriorOverride(overrideFilePath(
        overrideDirectory,
        QStringLiteral("alpha")
        ));
    QVERIFY(unchangedPriorOverride.open(QIODevice::ReadOnly));
    QCOMPARE(unchangedPriorOverride.readAll(), priorOverrideBytes);
    unchangedPriorOverride.close();
    const auto unchangedPackagedCampus = Application::CampusDashboardSelectedCampusReadQuery::execute(
        Application::CampusDashboardSelectedCampusReadRequest{campusId("bundang")},
        repository
        );
    QVERIFY(unchangedPackagedCampus);
    QCOMPARE(unchangedPackagedCampus.value()->officeNumber, std::string("418"));

    QVERIFY(QDir().rmdir(blockedBundangPath));
    const auto retriedSave = page->saveChanges();
    QVERIFY(retriedSave);
    QVERIFY(!page->isDirty());
    QCOMPARE(office->text(), QStringLiteral("Room 903"));

    const auto reloadedCampus = Application::CampusDashboardSelectedCampusReadQuery::execute(
        Application::CampusDashboardSelectedCampusReadRequest{campusId("bundang")},
        repository
        );
    QVERIFY(reloadedCampus);
    QCOMPARE(reloadedCampus.value()->officeNumber, std::string("Room 903"));
    QFile stillUnchangedPriorOverride(overrideFilePath(
        overrideDirectory,
        QStringLiteral("alpha")
        ));
    QVERIFY(stillUnchangedPriorOverride.open(QIODevice::ReadOnly));
    QCOMPARE(stillUnchangedPriorOverride.readAll(), priorOverrideBytes);
}

void CampusDirectoryTests::packagedMapReferencesAreTypedAndImagesStayWithinBound()
{
    auto catalog = loadCatalog();
    QVERIFY(catalog.has_value());
    Resources::ResourceLoader loader(std::move(*catalog));
    Platform::CampusDirectoryRepositoryAdapter repository(loader, QString());
    const auto resources = repository.mapResourcesFor(bundangSnapshot());
    QVERIFY(resources);
    QCOMPARE(resources.value().size(), std::size_t(2));

    const auto scope = Resources::ResourceScope::create(
        Resources::ResourceScopeKind::Page,
        QStringLiteral("campus-map-test")
        );
    QVERIFY(scope);
    for (const Resources::ResourceId& id : resources.value())
    {
        const auto request = Resources::ResourceRequest::create(id, *scope);
        QVERIFY(request);
        const auto image = loader.loadBoundedImage(*request, 1520);
        QVERIFY(image);
        QVERIFY(image->get().width() <= 1520);
        QVERIFY(image->get().height() <= 1520);
    }
}

void CampusDirectoryTests::savedCampusSelectionAndFirstAvailableFallback()
{
    auto catalog = loadCatalog();
    QVERIFY(catalog.has_value());
    Resources::ResourceLoader loader(std::move(*catalog));
    auto alpha = bundangSnapshot();
    alpha.id = campusId("alpha");
    alpha.campusName = "Alpha Campus";
    alpha.mapImagePaths.clear();
    const auto bundang = bundangSnapshot();

    const auto makeActions = [&alpha, &bundang](
                                 const std::optional<Domain::CampusId>& saved,
                                 std::optional<Domain::CampusId>* persisted) {
        Ui::CampusDirectoryPageActions actions;
        actions.editCapability = Application::CampusEditCapability::readOnly();
        actions.loadCampuses = [&alpha, &bundang] {
            return Application::CampusDirectoryReadResult::success({
                {bundang.id, bundang.campusName},
                {alpha.id, alpha.campusName}
            });
        };
        actions.loadCampus = [&alpha, &bundang](const Domain::CampusId& id) {
            if (id == alpha.id)
            {
                return Application::CampusDashboardSelectedCampusReadResult::success(alpha);
            }
            if (id == bundang.id)
            {
                return Application::CampusDashboardSelectedCampusReadResult::success(bundang);
            }
            return Application::CampusDashboardSelectedCampusReadResult::success(std::nullopt);
        };
        actions.readLastSelectedCampus = [saved] { return saved; };
        actions.saveLastSelectedCampus = [persisted](
                                              const std::optional<Domain::CampusId>& id) {
            if (persisted) *persisted = id;
            return Domain::Result<void>::success();
        };
        actions.saveCampus = [](const Application::CampusDashboardCampusSnapshot&,
                                Application::CampusDashboardCampusSaveIntent) {
            return Domain::Result<void>::success();
        };
        actions.importLegacyCampuses = [] {
            return Domain::Result<std::size_t>::success(0);
        };
        actions.mapResourcesFor = [](const Application::CampusDashboardCampusSnapshot&) {
            return Domain::Result<std::vector<Resources::ResourceId>>::success({});
        };
        return actions;
    };

    Test::MemoryPreferences savedPreferences;
    Ui::PageHost savedHost(loader, savedPreferences.model);
    const auto savedDescriptors = Ui::campusDirectoryPageDescriptors([&] {
        return makeActions(campusId("bundang"), nullptr);
    });
    QVERIFY(savedHost.registerPage(savedDescriptors.front()));
    QVERIFY(savedHost.activatePage(QStringLiteral("campus_information")));
    const auto* savedPage = dynamic_cast<const Ui::CampusDirectoryPage*>(
        savedHost.pageInstance(QStringLiteral("campus_information"))
        );
    QVERIFY(savedPage != nullptr);
    QVERIFY(savedPage->currentCampus().has_value());
    QCOMPARE(savedPage->currentCampus()->id.value(), std::string("bundang"));

    std::optional<Domain::CampusId> persisted;
    Test::MemoryPreferences fallbackPreferences;
    Ui::PageHost fallbackHost(loader, fallbackPreferences.model);
    const auto fallbackDescriptors = Ui::campusDirectoryPageDescriptors([&] {
        return makeActions(campusId("missing-campus"), &persisted);
    });
    QVERIFY(fallbackHost.registerPage(fallbackDescriptors.front()));
    QVERIFY(fallbackHost.activatePage(QStringLiteral("campus_information")));
    const auto* fallbackPage = dynamic_cast<const Ui::CampusDirectoryPage*>(
        fallbackHost.pageInstance(QStringLiteral("campus_information"))
        );
    QVERIFY(fallbackPage != nullptr);
    QVERIFY(fallbackPage->currentCampus().has_value());
    QCOMPARE(fallbackPage->currentCampus()->id.value(), std::string("alpha"));
    QVERIFY(persisted.has_value());
    QCOMPARE(persisted->value(), std::string("alpha"));
}

void CampusDirectoryTests::allFiveRoutesRenderAndReadOnlyDoesNotExposeEditing()
{
    auto catalog = loadCatalog();
    QVERIFY(catalog.has_value());
    Resources::ResourceLoader loader(std::move(*catalog));
    Test::MemoryPreferences preferences;
    Ui::PageHost host(loader, preferences.model);
    const auto descriptors = Ui::campusDirectoryPageDescriptors([] {
        return actionsFor(Application::CampusEditCapability::readOnly());
    });
    QCOMPARE(descriptors.size(), std::size_t(5));
    std::vector<QString> ids;
    for (const auto& descriptor : descriptors)
    {
        ids.push_back(descriptor.id.value());
        QVERIFY(descriptor.requiredPermissions.isEmpty());
        QVERIFY(host.registerPage(descriptor));
    }
    const std::vector<QString> expected{
        QStringLiteral("campus_information"),
        QStringLiteral("campus_directions"),
        QStringLiteral("campus_address"),
        QStringLiteral("campus_housing"),
        QStringLiteral("campus_map")
    };
    for (const QString& id : expected)
    {
        QVERIFY(ids.end() != std::find(ids.cbegin(), ids.cend(), id));
        QVERIFY(host.activatePage(id));
        auto* page = dynamic_cast<Ui::CampusDirectoryPage*>(host.pageInstance(id));
        QVERIFY(page != nullptr);
        QCOMPARE(page->section(), [&id] {
            if (id == QStringLiteral("campus_information")) return Ui::CampusDirectorySection::Information;
            if (id == QStringLiteral("campus_directions")) return Ui::CampusDirectorySection::Directions;
            if (id == QStringLiteral("campus_address")) return Ui::CampusDirectorySection::Address;
            if (id == QStringLiteral("campus_housing")) return Ui::CampusDirectorySection::Housing;
            return Ui::CampusDirectorySection::Maps;
        }());
        auto* readOnlyField = page->findChild<QLineEdit*>(QStringLiteral("campusField_campusName"));
        if (readOnlyField)
        {
            QVERIFY(readOnlyField->isReadOnly());
        }
        if (id == QStringLiteral("campus_information"))
        {
            auto* unavailable = page->findChild<QCheckBox*>(
                QStringLiteral("campusPrinterDriverUnavailable")
                );
            QVERIFY(unavailable != nullptr);
            QVERIFY(unavailable->isChecked());
            QVERIFY(!unavailable->isEnabled());
        }
        auto* save = page->findChild<QPushButton*>(QStringLiteral("campusDirectorySave"));
        QVERIFY(save != nullptr);
        QVERIFY(save->isHidden());
        auto* import = page->findChild<QPushButton*>(QStringLiteral("campusDirectoryImportLegacy"));
        QVERIFY(import != nullptr);
        QVERIFY(import->isHidden());
        if (id == QStringLiteral("campus_map"))
        {
            auto* openNaver = page->findChild<QPushButton*>(
                QStringLiteral("campusMapNaver")
                );
            auto* openKakao = page->findChild<QPushButton*>(
                QStringLiteral("campusMapKakao")
                );
            QVERIFY(openNaver != nullptr);
            QVERIFY(openKakao != nullptr);
            QVERIFY(openNaver->isEnabled());
            QVERIFY(openKakao->isEnabled());
        }
    }
}

void CampusDirectoryTests::failedSaveRetainsDirtyDraftForRetry()
{
    auto catalog = loadCatalog();
    QVERIFY(catalog.has_value());
    Resources::ResourceLoader loader(std::move(*catalog));
    Test::MemoryPreferences preferences;
    Ui::PageHost host(loader, preferences.model);
    bool failSave = true;
    int saveCount = 0;
    std::optional<Application::CampusDashboardCampusSnapshot> lastSaved;
    auto descriptor = Ui::campusDirectoryPageDescriptors([&] {
        return actionsFor(
            Application::CampusEditCapability::enabled(),
            [&](const Application::CampusDashboardCampusSnapshot& campus,
                Application::CampusDashboardCampusSaveIntent)
            {
                ++saveCount;
                if (failSave)
                {
                    return Domain::Result<void>::failure({
                        .code = Domain::ErrorCode::Technical,
                        .message = "injected save failure",
                        .recoverable = true
                    });
                }
                lastSaved = campus;
                return Domain::Result<void>::success();
            }
            );
    });
    QVERIFY(host.registerPage(std::move(descriptor.front())));
    QVERIFY(host.activatePage(QStringLiteral("campus_information")));
    auto* page = dynamic_cast<Ui::CampusDirectoryPage*>(
        host.pageInstance(QStringLiteral("campus_information"))
        );
    QVERIFY(page != nullptr);
    auto* office = page->findChild<QLineEdit*>(QStringLiteral("campusField_officeNumber"));
    auto* driverUnavailable = page->findChild<QCheckBox*>(
        QStringLiteral("campusPrinterDriverUnavailable")
        );
    QVERIFY(office != nullptr);
    QVERIFY(driverUnavailable != nullptr);
    QVERIFY(driverUnavailable->isChecked());
    QVERIFY(driverUnavailable->isEnabled());
    office->clear();
    QTest::keyClicks(office, QStringLiteral("Room 902"));
    driverUnavailable->setChecked(false);
    QVERIFY(page->isDirty());
    QCOMPARE(office->text(), QStringLiteral("Room 902"));
    QVERIFY(!driverUnavailable->isChecked());

    const auto failed = page->saveChanges();
    QVERIFY(!failed);
    QVERIFY(page->isDirty());
    QCOMPARE(office->text(), QStringLiteral("Room 902"));
    QCOMPARE(saveCount, 1);

    failSave = false;
    const auto retried = page->saveChanges();
    QVERIFY(retried);
    QVERIFY(!page->isDirty());
    QCOMPARE(saveCount, 2);
    QVERIFY(lastSaved.has_value());
    QCOMPARE(lastSaved->officeNumber, std::string("Room 902"));
    QVERIFY(!lastSaved->printerDriverUrlUnavailable);
}

void CampusDirectoryTests::mapsLoadOnlyOnMapRouteReleaseOnLeaveAndReopen()
{
    auto catalog = loadCatalog();
    QVERIFY(catalog.has_value());
    std::vector<Resources::ResourceDiagnostic> diagnostics;
    Resources::ResourceLoader loader(
        std::move(*catalog),
        [&](const Resources::ResourceDiagnostic& diagnostic)
        {
            diagnostics.push_back(diagnostic);
        }
        );
    Test::MemoryPreferences preferences;
    Ui::PageHost host(loader, preferences.model);
    const auto descriptors = Ui::campusDirectoryPageDescriptors([] {
        return actionsFor(Application::CampusEditCapability::readOnly());
    });
    for (const auto& descriptor : descriptors)
    {
        QVERIFY(host.registerPage(descriptor));
    }

    QVERIFY(host.activatePage(QStringLiteral("campus_information")));
    const auto mapLoads = [&diagnostics]
    {
        return std::count_if(
            diagnostics.cbegin(),
            diagnostics.cend(),
            [](const Resources::ResourceDiagnostic& item)
            {
                return item.resourceId.value().endsWith(QStringLiteral(".png"))
                    && item.resourceId.value().contains(QStringLiteral("/campuses/"));
            }
            );
    };
    QCOMPARE(mapLoads(), 0);

    QVERIFY(host.activatePage(QStringLiteral("campus_map")));
    QCOMPARE(mapLoads(), 2);
    auto* mapPage = dynamic_cast<Ui::CampusDirectoryPage*>(
        host.pageInstance(QStringLiteral("campus_map"))
        );
    QVERIFY(mapPage != nullptr);
    QCOMPARE(mapPage->findChildren<QLabel*>(QStringLiteral("campusMapImage")).size(), 2);

    QVERIFY(host.activatePage(QStringLiteral("campus_information")));
    QCOMPARE(mapLoads(), 2);
    QCOMPARE(mapPage->findChildren<QLabel*>(QStringLiteral("campusMapImage")).size(), 0);

    QVERIFY(host.activatePage(QStringLiteral("campus_map")));
    QCOMPARE(mapLoads(), 4);
    QCOMPARE(mapPage->findChildren<QLabel*>(QStringLiteral("campusMapImage")).size(), 2);
    QVERIFY(host.releasePage(QStringLiteral("campus_map")));
    QVERIFY(!host.hasPageInstance(QStringLiteral("campus_map")));
}

void CampusDirectoryTests::koreanTranslationAndApplicationPaletteReachRoutes()
{
    QTemporaryDir translationRoot;
    QVERIFY(translationRoot.isValid());
    const QString campusDirectory = QDir(translationRoot.path()).filePath(
        QStringLiteral("assets/campuses")
        );
    const QString translationDirectory = QDir(translationRoot.path()).filePath(
        QStringLiteral("assets/translations")
        );
    QVERIFY(QDir().mkpath(campusDirectory));
    QVERIFY(QDir().mkpath(translationDirectory));
    QVERIFY(QFile::copy(
        QDir(testResourcePath(CLASSMNGR_NEXT_TEST_RESOURCE_ROOT)).filePath(
            QStringLiteral("assets/campuses/bundang.json")
            ),
        QDir(campusDirectory).filePath(QStringLiteral("bundang.json"))
        ));
    QVERIFY(QFile::copy(
        QDir(testResourcePath(CLASSMNGR_NEXT_TEST_TRANSLATION_DIRECTORY)).filePath(
            QStringLiteral("ClassMngr_ko_KR.qm")
            ),
        QDir(translationDirectory).filePath(QStringLiteral("ClassMngr_ko_KR.qm"))
        ));
    const auto catalogResult = Resources::ResourceCatalog::load(
        testResourcePath(CLASSMNGR_NEXT_TEST_RESOURCE_CATALOG),
        translationRoot.path()
        );
    QVERIFY(catalogResult);
    auto catalog = std::optional<Resources::ResourceCatalog>(*catalogResult);
    Resources::ResourceLoader loader(std::move(*catalog));
    const auto translationId = Resources::ResourceId::fromString(
        QStringLiteral("resource:assets/translations/ClassMngr_ko_KR.ts")
        );
    QVERIFY(translationId.has_value());
    const auto scope = Resources::ResourceScope::create(
        Resources::ResourceScopeKind::Feature,
        QStringLiteral("campus-locale-test")
        );
    QVERIFY(scope);
    const auto request = Resources::ResourceRequest::create(*translationId, *scope);
    QVERIFY(request);
    auto translation = loader.loadTranslation(*request, *QCoreApplication::instance());
    QVERIFY(translation);
    QVERIFY(translation->get().isInstalled());

    const QPalette original = QApplication::palette();
    QPalette light = original;
    light.setColor(QPalette::Window, QColor(QStringLiteral("#ffffff")));
    light.setColor(QPalette::WindowText, QColor(QStringLiteral("#202124")));
    QApplication::setPalette(light);

    Test::MemoryPreferences preferences;
    Ui::PageHost host(loader, preferences.model);
    const auto descriptors = Ui::campusDirectoryPageDescriptors([] {
        return actionsFor(Application::CampusEditCapability::readOnly());
    });
    QVERIFY(host.registerPage(descriptors.front()));
    QVERIFY(host.activatePage(QStringLiteral("campus_information")));
    auto* page = dynamic_cast<Ui::CampusDirectoryPage*>(
        host.pageInstance(QStringLiteral("campus_information"))
        );
    QVERIFY(page != nullptr);
    auto* title = page->findChild<QLabel*>(QStringLiteral("campusDirectoryTitle"));
    QVERIFY(title != nullptr);
    QCOMPARE(title->text(), QStringLiteral("사무실 정보"));
    auto* driverUnavailable = page->findChild<QCheckBox*>(
        QStringLiteral("campusPrinterDriverUnavailable")
        );
    QVERIFY(driverUnavailable != nullptr);
    QCOMPARE(
        driverUnavailable->text(),
        QStringLiteral("프린터 드라이버 링크 없음")
        );

    QVERIFY(page->palette().color(QPalette::Window) == QColor(QStringLiteral("#ffffff")));
    QPalette dark = original;
    dark.setColor(QPalette::Window, QColor(QStringLiteral("#202124")));
    dark.setColor(QPalette::WindowText, QColor(QStringLiteral("#f5f5f5")));
    QApplication::setPalette(dark);
    Test::MemoryPreferences darkPreferences;
    Ui::PageHost darkHost(loader, darkPreferences.model);
    const auto darkDescriptors = Ui::campusDirectoryPageDescriptors([] {
        return actionsFor(Application::CampusEditCapability::readOnly());
    });
    QVERIFY(darkHost.registerPage(darkDescriptors.front()));
    QVERIFY(darkHost.activatePage(QStringLiteral("campus_information")));
    const auto* darkPage = darkHost.pageInstance(QStringLiteral("campus_information"));
    QVERIFY(darkPage != nullptr);
    QVERIFY(darkPage->palette().color(QPalette::Window) == QColor(QStringLiteral("#202124")));
    QApplication::setPalette(original);
}

}

QTEST_MAIN(CampusDirectoryTests)
#include "campus_directory_tests.moc"
