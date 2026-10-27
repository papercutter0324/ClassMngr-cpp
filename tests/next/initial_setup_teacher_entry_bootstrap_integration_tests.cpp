#include "core/settingsmanager.h"
#include "next/bootstrap/application_bootstrap.h"
#include "next/bootstrap/startup_update_dispatcher.h"
#include "next/resources/resource_root_resolver.h"
#include "next/ui/application_shell.h"
#include "next/ui/initial_page.h"
#include "next/ui/page_host.h"
#include "next/application/file_dialog_port.h"

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QUuid>
#include <QtTest/QtTest>

#include <memory>
#include <optional>
#include <string>
#include <vector>

using namespace ClassMngr::Next;

namespace
{

class SilentUpdateDispatcher final : public Bootstrap::StartupUpdateDispatcher
{
public:
    void dispatch() override {}
};

class SelectedProfileFileDialog final : public Application::FileDialogPort
{
public:
    explicit SelectedProfileFileDialog(QString selectedPath)
        : m_selectedPath(Application::FileDialogPath(
              selectedPath.toUtf8().toStdString()
              ))
    {
    }

    [[nodiscard]] Domain::Result<
        Application::FileDialogResponse<Application::FileDialogPath>>
    openOneFile(const Application::OpenFileDialogRequest&) override
    {
        return unexpectedCall<
            Application::FileDialogResponse<Application::FileDialogPath>>();
    }

    [[nodiscard]] Domain::Result<
        Application::FileDialogResponse<std::vector<Application::FileDialogPath>>>
    openManyFiles(const Application::OpenFileDialogRequest&) override
    {
        return unexpectedCall<
            Application::FileDialogResponse<std::vector<Application::FileDialogPath>>>();
    }

    [[nodiscard]] Domain::Result<
        Application::FileDialogResponse<Application::SavedFileDialogSelection>>
    saveFile(const Application::SaveFileDialogRequest&) override
    {
        return Domain::Result<
            Application::FileDialogResponse<
                Application::SavedFileDialogSelection>>::success({
            .selection = Application::SavedFileDialogSelection{
                .path = m_selectedPath,
                .openAfterSaving = false
            }
        });
    }

    [[nodiscard]] Domain::Result<
        Application::FileDialogResponse<Application::FileDialogPath>>
    selectDirectory(const Application::SelectDirectoryDialogRequest&) override
    {
        return unexpectedCall<
            Application::FileDialogResponse<Application::FileDialogPath>>();
    }

private:
    template <typename Response>
    [[nodiscard]] static Domain::Result<Response> unexpectedCall()
    {
        return Domain::Result<Response>::failure({
            .code = Domain::ErrorCode::Technical,
            .message = "Unexpected file dialog request.",
            .recoverable = false
        });
    }

    Application::FileDialogPath m_selectedPath;
};

class SettingsRootEnvironmentGuard final
{
public:
    SettingsRootEnvironmentGuard()
        : m_wasSet(qEnvironmentVariableIsSet("CLASSMNGR_SETTINGS_ROOT"))
        , m_previousValue(qgetenv("CLASSMNGR_SETTINGS_ROOT"))
    {
    }

    ~SettingsRootEnvironmentGuard()
    {
        if (m_wasSet)
        {
            qputenv("CLASSMNGR_SETTINGS_ROOT", m_previousValue);
        }
        else
        {
            qunsetenv("CLASSMNGR_SETTINGS_ROOT");
        }
    }

private:
    bool m_wasSet;
    QByteArray m_previousValue;
};

class SettingsPreferenceGuard final
{
public:
    SettingsPreferenceGuard()
        : m_languageKey(QStringLiteral("options/language"))
        , m_themeKey(QStringLiteral("options/theme"))
        , m_previousLanguage(SettingsManager::instance().get(m_languageKey))
        , m_previousTheme(SettingsManager::instance().get(m_themeKey))
    {
        SettingsManager::instance().set(m_languageKey, 1);
        SettingsManager::instance().set(m_themeKey, 1);
    }

    ~SettingsPreferenceGuard()
    {
        restore(m_languageKey, m_previousLanguage);
        restore(m_themeKey, m_previousTheme);
    }

private:
    QString m_languageKey;
    QString m_themeKey;
    QVariant m_previousLanguage;
    QVariant m_previousTheme;

    static void restore(const QString& key, const QVariant& value)
    {
        if (value.isValid())
        {
            SettingsManager::instance().set(key, value);
        }
        else
        {
            SettingsManager::instance().remove(key);
        }
    }
};

class ApplicationStyleSheetGuard final
{
public:
    explicit ApplicationStyleSheetGuard(QApplication& application)
        : m_application(application)
        , m_previousStyleSheet(application.styleSheet())
    {
    }

    ~ApplicationStyleSheetGuard()
    {
        m_application.setStyleSheet(m_previousStyleSheet);
    }

private:
    QApplication& m_application;
    QString m_previousStyleSheet;
};

struct RecentWorkspaceHistoryProbe final
{
    int loadCount = 0;
    int saveCount = 0;
    std::vector<Application::RecentWorkspaceHistory> savedHistories;
};

class RecordingRecentWorkspaceHistoryPort final
    : public Application::RecentWorkspaceHistoryPort
{
public:
    explicit RecordingRecentWorkspaceHistoryPort(
        std::shared_ptr<RecentWorkspaceHistoryProbe> probe
        )
        : m_probe(std::move(probe))
    {
    }

    [[nodiscard]] Application::RecentWorkspaceHistory load() const override
    {
        ++m_probe->loadCount;
        return {};
    }

    [[nodiscard]] Domain::Result<void> save(
        const Application::RecentWorkspaceHistory& history
        ) const override
    {
        ++m_probe->saveCount;
        m_probe->savedHistories.push_back(history);
        return Domain::Result<void>::success();
    }

    [[nodiscard]] Domain::Result<void> clear() const override
    {
        return Domain::Result<void>::success();
    }

private:
    std::shared_ptr<RecentWorkspaceHistoryProbe> m_probe;
};

QString builtResourceRoot()
{
    return QStringLiteral(CLASSMNGR_NEXT_TEST_RESOURCE_ROOT);
}

QString builtResourceCatalogPath()
{
    return QStringLiteral(CLASSMNGR_NEXT_TEST_RESOURCE_CATALOG);
}

class StartupResourceFixture final
{
public:
    StartupResourceFixture()
        : m_root(QDir(m_directory.path()).filePath(QStringLiteral("resources")))
    {
    }

    [[nodiscard]] bool isValid() const
    {
        return m_directory.isValid() && QDir().mkpath(m_root);
    }

    [[nodiscard]] const QString& root() const noexcept
    {
        return m_root;
    }

    [[nodiscard]] bool addResource(const QString& relativePath) const
    {
        return copyFile(
            QDir(builtResourceRoot()).filePath(relativePath),
            QDir(m_root).filePath(relativePath)
            );
    }

    [[nodiscard]] bool addTranslation(const QString& locale) const
    {
        const QString relativePath = QStringLiteral(
            "assets/translations/ClassMngr_%1.qm"
            ).arg(locale);
        const QString sourcePath = QDir(
            QFileInfo(builtResourceCatalogPath()).absolutePath()
            ).filePath(relativePath);
        return copyFile(sourcePath, QDir(m_root).filePath(relativePath));
    }

    [[nodiscard]] bool addStartupResources(
        const QString& locale,
        const QString& theme
        ) const
    {
        return addTranslation(locale)
            && addResource(QStringLiteral("assets/styles/%1.qss").arg(theme))
            && addResource(QStringLiteral("assets/fonts/Inter.ttc"))
            && addResource(QStringLiteral("assets/fonts/PretendardVariable.ttf"))
            && addResource(QStringLiteral("assets/documents/documents.json"));
    }

private:
    [[nodiscard]] static bool copyFile(
        const QString& source,
        const QString& destination
        )
    {
        if (!QDir().mkpath(QFileInfo(destination).absolutePath()))
        {
            return false;
        }
        return QFile::copy(source, destination);
    }

    QTemporaryDir m_directory;
    QString m_root;
};

Bootstrap::ApplicationBootstrap::ResourcePackagePathsResolver
resolverForRoot(QString resourceRoot)
{
    return [resourceRoot = std::move(resourceRoot)]()
        -> Resources::ResourceResult<Resources::ResourcePackagePaths> {
        return Resources::ResourcePackagePaths{
            resourceRoot,
            builtResourceCatalogPath()
        };
    };
}

struct TeacherRecord final
{
    QString englishName;
    QString koreanName;
    QString preferredSpelling;
    QString preferredName;
    QString roomNumber;
    QString wifiName;
    QString wifiPassword;
    QString internetType;
    QString zoomId;
    QString zoomPassword;
    QString projectionType;
};

[[nodiscard]] std::optional<TeacherRecord> readTeacherRecord(const QString& path)
{
    const QString connectionName = QStringLiteral("teacher-entry-bootstrap-%1")
        .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    std::optional<TeacherRecord> record;
    {
        QSqlDatabase database = QSqlDatabase::addDatabase(
            QStringLiteral("QSQLITE"), connectionName
            );
        database.setDatabaseName(path);
        if (database.open())
        {
            QSqlQuery query(database);
            if (query.exec(QStringLiteral(
                    "SELECT teacher_en, teacher_kr, preferred_romanization, "
                    "preferred_name, room_number, wifi_name, wifi_password, "
                    "internet_type, zoom_id, zoom_password, projection_type "
                    "FROM teachers ORDER BY id LIMIT 1"
                    ))
                && query.next())
            {
                record = TeacherRecord{
                    .englishName = query.value(0).toString(),
                    .koreanName = query.value(1).toString(),
                    .preferredSpelling = query.value(2).toString(),
                    .preferredName = query.value(3).toString(),
                    .roomNumber = query.value(4).toString(),
                    .wifiName = query.value(5).toString(),
                    .wifiPassword = query.value(6).toString(),
                    .internetType = query.value(7).toString(),
                    .zoomId = query.value(8).toString(),
                    .zoomPassword = query.value(9).toString(),
                    .projectionType = query.value(10).toString()
                };
            }
            database.close();
        }
    }
    QSqlDatabase::removeDatabase(connectionName);
    return record;
}

} // namespace

class InitialSetupTeacherEntryBootstrapIntegrationTests final : public QObject
{
    Q_OBJECT

private slots:
    void initialSetupPageCreatesTeacherThroughWorkspaceStore();
};

void InitialSetupTeacherEntryBootstrapIntegrationTests::
initialSetupPageCreatesTeacherThroughWorkspaceStore()
{
    QTemporaryDir settingsRoot;
    QVERIFY(settingsRoot.isValid());
    SettingsRootEnvironmentGuard settingsRootGuard;
    qputenv("CLASSMNGR_SETTINGS_ROOT", settingsRoot.path().toUtf8());
    SettingsPreferenceGuard preferences;
    StartupResourceFixture resources;
    QVERIFY(resources.isValid());
    QVERIFY(resources.addStartupResources(
        QStringLiteral("en_US"), QStringLiteral("light")
        ));
    QTemporaryDir workspaceRoot;
    QVERIFY(workspaceRoot.isValid());
    const QString profilePath = workspaceRoot.filePath(
        QStringLiteral("teacher-entry-integration.tps")
        );
    const QString recoveryJournalPath = workspaceRoot.filePath(
        QStringLiteral("initial-setup-recovery.json")
        );

    QApplication& application = *qApp;
    ApplicationStyleSheetGuard styleSheetGuard(application);
    auto recentHistoryProbe = std::make_shared<RecentWorkspaceHistoryProbe>();

    Bootstrap::ApplicationBootstrap::StartupWorkspaceSeams seams;
    seams.commandLineArguments = QStringList{QStringLiteral("teacher-entry-test")};
    seams.recentAutoOpenEnabled = false;
    seams.initialSetupRecoveryJournalPath = recoveryJournalPath;
    seams.recentWorkspaceHistoryPortFactory = [recentHistoryProbe] {
        return std::make_unique<RecordingRecentWorkspaceHistoryPort>(
            recentHistoryProbe
            );
    };
    seams.initialSetupFileDialogPortFactory = [profilePath] {
        return std::make_unique<SelectedProfileFileDialog>(profilePath);
    };

    Bootstrap::ApplicationBootstrap bootstrap(
        application,
        {},
        {},
        resolverForRoot(resources.root()),
        QStringList{QStringLiteral("en-US")},
        std::move(seams),
        [] { return std::make_unique<SilentUpdateDispatcher>(); }
        );
    const Application::StartupResult started = bootstrap.start();
    QVERIFY2(started.succeeded(), "ApplicationBootstrap failed to start.");

    auto* const page = dynamic_cast<Ui::InitialPage*>(
        bootstrap.shell()->pageHost().pageInstance(QStringLiteral("initial_setup"))
        );
    QVERIFY(page);
    auto* const createProfile = page->findChild<QPushButton*>(
        QStringLiteral("classMngrNextInitialSetupCreateProfile")
        );
    QVERIFY(createProfile);
    createProfile->click();
    QVERIFY(QFileInfo::exists(profilePath));
    QVERIFY(QFileInfo::exists(recoveryJournalPath));
    QCOMPARE(recentHistoryProbe->saveCount, 0);

    auto* const englishName = page->findChild<QLineEdit*>(
        QStringLiteral("classMngrNextInitialSetupTeacherEnglishName")
        );
    auto* const koreanName = page->findChild<QLineEdit*>(
        QStringLiteral("classMngrNextInitialSetupTeacherKoreanName")
        );
    auto* const preferredSpelling = page->findChild<QLineEdit*>(
        QStringLiteral("classMngrNextInitialSetupTeacherPreferredSpelling")
        );
    auto* const room = page->findChild<QLineEdit*>(
        QStringLiteral("classMngrNextInitialSetupTeacherRoom")
        );
    auto* const wifiName = page->findChild<QLineEdit*>(
        QStringLiteral("classMngrNextInitialSetupTeacherWifiName")
        );
    auto* const wifiPassword = page->findChild<QLineEdit*>(
        QStringLiteral("classMngrNextInitialSetupTeacherWifiPassword")
        );
    auto* const continueButton = page->findChild<QPushButton*>(
        QStringLiteral("classMngrNextInitialSetupContinueTeacherEntry")
        );
    auto* const teacherStatus = page->findChild<QLabel*>(
        QStringLiteral("classMngrNextInitialSetupTeacherEntryStatus")
        );
    auto* const pageStatus = page->findChild<QLabel*>(
        QStringLiteral("classMngrNextInitialSetupStatus")
        );
    QVERIFY(englishName && koreanName && preferredSpelling && room);
    QVERIFY(wifiName && wifiPassword && continueButton && teacherStatus && pageStatus);

    englishName->setText(QStringLiteral("Alex Kim"));
    koreanName->setText(QString::fromUtf8("김선생님"));
    preferredSpelling->setText(QStringLiteral("Kim Seonsaeng"));
    room->setText(QStringLiteral("Room 12"));
    wifiName->setText(QStringLiteral("Teacher WiFi"));
    wifiPassword->setText(QStringLiteral("wifi secret"));
    continueButton->click();

    QCOMPARE(
        teacherStatus->text(),
        QStringLiteral("Teacher saved. Setup remains pending.")
        );
    QVERIFY(!continueButton->isEnabled());
    QVERIFY(pageStatus->text().contains(QStringLiteral("Setup is pending")));
    QVERIFY(bootstrap.workspaceStateSnapshot().session().has_value());
    QVERIFY(!bootstrap.shutdownRequested());
    QVERIFY(QFileInfo::exists(recoveryJournalPath));
    QCOMPARE(recentHistoryProbe->saveCount, 0);
    QVERIFY(recentHistoryProbe->savedHistories.empty());

    const Application::ShutdownResult stopped = bootstrap.shutdown();
    QVERIFY(stopped.shutdownRequested);
    QVERIFY(QFileInfo::exists(recoveryJournalPath));
    QCOMPARE(recentHistoryProbe->saveCount, 0);
    const std::optional<TeacherRecord> persisted = readTeacherRecord(profilePath);
    QVERIFY2(persisted.has_value(), "The teacher row was not stored in the .tps file.");
    QCOMPARE(persisted->englishName, QStringLiteral("Alex Kim"));
    QCOMPARE(persisted->koreanName, QString::fromUtf8("김선생님"));
    QCOMPARE(persisted->preferredSpelling, QStringLiteral("Kim Seonsaeng"));
    QCOMPARE(persisted->preferredName, QStringLiteral("Alex Kim"));
    QCOMPARE(persisted->roomNumber, QStringLiteral("Room 12"));
    QCOMPARE(persisted->wifiName, QStringLiteral("Teacher WiFi"));
    QCOMPARE(persisted->wifiPassword, QStringLiteral("wifi secret"));
    QCOMPARE(persisted->internetType, QStringLiteral("WiFi"));
    QCOMPARE(persisted->projectionType, QStringLiteral("HDMI"));

}

QTEST_MAIN(InitialSetupTeacherEntryBootstrapIntegrationTests)

#include "initial_setup_teacher_entry_bootstrap_integration_tests.moc"
