#include "core/settingsmanager.h"
#include "data/database/workspace_store.h"
#include "next/bootstrap/application_bootstrap.h"
#include "next/bootstrap/startup_update_dispatcher.h"
#include "next/resources/resource_root_resolver.h"
#include "next/ui/application_shell.h"
#include "next/ui/initial_page.h"
#include "next/ui/page_host.h"
#include "next/application/file_dialog_port.h"

#include <QApplication>
#include <QCheckBox>
#include <QColor>
#include <QColorDialog>
#include <QComboBox>
#include <QDir>
#include <QDialog>
#include <QFile>
#include <QFileInfo>
#include <QGroupBox>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QTimer>
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
    explicit SelectedProfileFileDialog(
        QString selectedPath,
        QString openPath = {}
        )
        : m_selectedPath(Application::FileDialogPath(
              selectedPath.toUtf8().toStdString()
              ))
        , m_openPath(openPath.isEmpty()
              ? std::nullopt
              : std::optional<Application::FileDialogPath>{
                    Application::FileDialogPath(
                        openPath.toUtf8().toStdString())
                })
    {
    }

    [[nodiscard]] Domain::Result<
        Application::FileDialogResponse<Application::FileDialogPath>>
    openOneFile(const Application::OpenFileDialogRequest&) override
    {
        if (m_openPath.has_value())
        {
            return Domain::Result<
                Application::FileDialogResponse<Application::FileDialogPath>>::success({
                    .selection = m_openPath
                });
        }
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
    std::optional<Application::FileDialogPath> m_openPath;
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

[[nodiscard]] std::optional<QString> readTeacherKoreanName(
    const QString& path,
    const QString& teacherId
    )
{
    const QString connectionName = QStringLiteral("setup-imported-teacher-%1")
        .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    std::optional<QString> koreanName;
    {
        QSqlDatabase database = QSqlDatabase::addDatabase(
            QStringLiteral("QSQLITE"), connectionName
            );
        database.setDatabaseName(path);
        if (database.open())
        {
            QSqlQuery query(database);
            query.prepare(QStringLiteral(
                "SELECT teacher_kr FROM teachers WHERE id=?"
                ));
            query.addBindValue(teacherId.toLongLong());
            if (query.exec() && query.next())
            {
                koreanName = query.value(0).toString();
            }
            database.close();
        }
    }
    QSqlDatabase::removeDatabase(connectionName);
    return koreanName;
}

[[nodiscard]] std::optional<int> tableRowCount(
    const QString& path,
    const QString& tableName
    )
{
    const QString connectionName = QStringLiteral("setup-count-%1").arg(
        QUuid::createUuid().toString(QUuid::WithoutBraces)
        );
    std::optional<int> count;
    {
        QSqlDatabase database = QSqlDatabase::addDatabase(
            QStringLiteral("QSQLITE"), connectionName
            );
        database.setDatabaseName(path);
        if (database.open())
        {
            QSqlQuery query(database);
            if (query.exec(QStringLiteral("SELECT COUNT(*) FROM %1").arg(tableName))
                && query.next())
            {
                count = query.value(0).toInt();
            }
            database.close();
        }
    }
    QSqlDatabase::removeDatabase(connectionName);
    return count;
}

[[nodiscard]] bool executeWorkspaceFixture(
    const QString& profilePath,
    const QString& fixtureName,
    QString* errorMessage
    )
{
    const QString fixturePath = QDir(QStringLiteral(CLASSMNGR_SOURCE_DIR))
        .filePath(QStringLiteral("tests/fixtures/workspaces/") + fixtureName);
    QFile fixture(fixturePath);
    if (!fixture.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        if (errorMessage)
        {
            *errorMessage = fixture.errorString();
        }
        return false;
    }
    const QStringList statements = QString::fromUtf8(fixture.readAll())
        .split(QLatin1Char(';'), Qt::SkipEmptyParts);

    const QString connectionName = QStringLiteral("setup-original-fixture-%1")
        .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    bool executed = false;
    {
        QSqlDatabase database = QSqlDatabase::addDatabase(
            QStringLiteral("QSQLITE"), connectionName
            );
        database.setDatabaseName(profilePath);
        if (!database.open())
        {
            if (errorMessage)
            {
                *errorMessage = database.lastError().text();
            }
        }
        else
        {
            executed = true;
            {
                QSqlQuery query(database);
                for (const QString& rawStatement : statements)
                {
                    const QString statement = rawStatement.trimmed();
                    if (statement.isEmpty())
                    {
                        continue;
                    }
                    if (!query.exec(statement))
                    {
                        if (errorMessage)
                        {
                            *errorMessage = QStringLiteral("%1: %2")
                                .arg(statement.simplified().left(160),
                                     query.lastError().text());
                        }
                        executed = false;
                        break;
                    }
                }
            }
            database.close();
        }
    }
    QSqlDatabase::removeDatabase(connectionName);
    return executed;
}

[[nodiscard]] bool createWorkspaceWithFixture(
    const QString& profilePath,
    const QString& fixtureName,
    QString* errorMessage
    )
{
    {
        WorkspaceStore store;
        const RepositoryStatus opened = store.open(profilePath);
        if (!opened)
        {
            if (errorMessage)
            {
                *errorMessage = opened.error().diagnostic;
            }
            return false;
        }
        store.close();
    }
    return executeWorkspaceFixture(profilePath, fixtureName, errorMessage);
}

void chooseColorInNextModalDialog(const QColor& color, bool* didChoose)
{
    QTimer::singleShot(0, [color, didChoose] {
        auto* const dialog = qobject_cast<QColorDialog*>(
            QApplication::activeModalWidget()
            );
        if (!dialog)
        {
            return;
        }
        dialog->setCurrentColor(color);
        dialog->accept();
        if (didChoose)
        {
            *didChoose = true;
        }
    });
}

} // namespace

class InitialSetupTeacherEntryBootstrapIntegrationTests final : public QObject
{
    Q_OBJECT

private slots:
    void initialSetupPageCreatesTeacherThroughWorkspaceStore();
    void importedTeacherCanBeSelectedForDraftBeforeCancelRestoresOriginal();
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
    const QByteArray previousProfileBytes("preserved original profile bytes");
    {
        QFile previousProfile(profilePath);
        QVERIFY(previousProfile.open(QIODevice::WriteOnly));
        QCOMPARE(previousProfile.write(previousProfileBytes),
                 previousProfileBytes.size());
    }

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

    QFile recoveryJournal(recoveryJournalPath);
    QVERIFY(recoveryJournal.open(QIODevice::ReadOnly));
    const QJsonDocument recoveryRecord = QJsonDocument::fromJson(
        recoveryJournal.readAll()
        );
    recoveryJournal.close();
    QVERIFY(recoveryRecord.isObject());
    const QString originalBackup = recoveryRecord.object().value(
        QStringLiteral("originalBackup")
        ).toString();
    QVERIFY(!originalBackup.isEmpty());
    QFile preservedProfile(originalBackup);
    QVERIFY(preservedProfile.open(QIODevice::ReadOnly));
    QCOMPARE(preservedProfile.readAll(), previousProfileBytes);
    preservedProfile.close();

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
    auto* const classDetailsGroup = page->findChild<QGroupBox*>(
        QStringLiteral("classMngrNextInitialSetupClassDetails")
        );
    auto* const classTeacher = page->findChild<QComboBox*>(
        QStringLiteral("classMngrNextInitialSetupClassTeacher")
        );
    auto* const classColorButton = page->findChild<QPushButton*>(
        QStringLiteral("classMngrNextInitialSetupClassColorChoose")
        );
    auto* const classGrade = page->findChild<QComboBox*>(
        QStringLiteral("classMngrNextInitialSetupClassGrade")
        );
    auto* const classLevel = page->findChild<QComboBox*>(
        QStringLiteral("classMngrNextInitialSetupClassLevel")
        );
    auto* const readingBook = page->findChild<QComboBox*>(
        QStringLiteral("classMngrNextInitialSetupReadingBook")
        );
    auto* const essayBook = page->findChild<QComboBox*>(
        QStringLiteral("classMngrNextInitialSetupEssayBook")
        );
    auto* const continueClassDetails = page->findChild<QPushButton*>(
        QStringLiteral("classMngrNextInitialSetupClassDetailsContinue")
        );
    QVERIFY(englishName && koreanName && preferredSpelling && room);
    QVERIFY(wifiName && wifiPassword && continueButton && teacherStatus && pageStatus);
    QVERIFY(classDetailsGroup && classTeacher && classColorButton);
    QVERIFY(classGrade && classLevel && readingBook && essayBook);
    QVERIFY(continueClassDetails);

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
    QVERIFY(!classDetailsGroup->isHidden());
    QCOMPARE(classTeacher->count(), 1);
    QCOMPARE(classTeacher->currentData().toString(), QStringLiteral("1"));
    QCOMPARE(classGrade->currentText(), QStringLiteral("E4"));
    QCOMPARE(classLevel->currentText(), QStringLiteral("Theseus"));
    QVERIFY(readingBook->currentText().isEmpty());
    QVERIFY(essayBook->currentText().isEmpty());

    bool colorDialogAccepted = false;
    chooseColorInNextModalDialog(QColor(QStringLiteral("#123456")),
                                 &colorDialogAccepted);
    classColorButton->click();
    QVERIFY(colorDialogAccepted);
    continueClassDetails->click();

    QVERIFY(bootstrap.initialSetupClassDetailsDraft().has_value());
    const Application::InitialSetupClassDetailsDraft& draft =
        *bootstrap.initialSetupClassDetailsDraft();
    QCOMPARE(draft.koreanTeacherId.value(), std::string("1"));
    QVERIFY(draft.classColorChosen);
    QCOMPARE(draft.classColor, std::u16string(u"#123456"));
    QCOMPARE(draft.fontColor, std::u16string(u"#FFFFFF"));
    QCOMPARE(draft.classGrade, std::u16string(u"E4"));
    QCOMPARE(draft.classLevel, std::u16string(u"Theseus"));
    QVERIFY(draft.readingBook.empty());
    QVERIFY(draft.essayBook.empty());
    auto* const cancelSetup = page->findChild<QPushButton*>(
        QStringLiteral("classMngrNextInitialSetupCancel")
        );
    QVERIFY(cancelSetup);
    QVERIFY(!cancelSetup->isHidden());
    QVERIFY(pageStatus->text().contains(QStringLiteral("Setup is pending")));
    QVERIFY(bootstrap.workspaceStateSnapshot().session().has_value());
    QVERIFY(QFileInfo::exists(recoveryJournalPath));
    QVERIFY(QFileInfo::exists(originalBackup));
    QVERIFY(!bootstrap.shutdownRequested());
    QVERIFY(QFileInfo::exists(recoveryJournalPath));
    QCOMPARE(recentHistoryProbe->saveCount, 0);
    QVERIFY(recentHistoryProbe->savedHistories.empty());

    const Application::ShutdownResult stopped = bootstrap.shutdown();
    QVERIFY(stopped.shutdownRequested);
    QVERIFY(QFileInfo::exists(recoveryJournalPath));
    QVERIFY(QFileInfo::exists(originalBackup));
    QCOMPARE(recentHistoryProbe->saveCount, 0);
    const auto teachersCount = tableRowCount(
        profilePath, QStringLiteral("teachers")
        );
    const auto classesCount = tableRowCount(
        profilePath, QStringLiteral("classes")
        );
    const auto classDetailsCount = tableRowCount(
        profilePath, QStringLiteral("class_info")
        );
    const auto regularTimesCount = tableRowCount(
        profilePath, QStringLiteral("class_times")
        );
    const auto intensiveTimesCount = tableRowCount(
        profilePath, QStringLiteral("class_intensive_times")
        );
    QVERIFY(teachersCount && classesCount && classDetailsCount);
    QVERIFY(regularTimesCount && intensiveTimesCount);
    QCOMPARE(*teachersCount, 1);
    QCOMPARE(*classesCount, 0);
    QCOMPARE(*classDetailsCount, 0);
    QCOMPARE(*regularTimesCount, 0);
    QCOMPARE(*intensiveTimesCount, 0);
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

void InitialSetupTeacherEntryBootstrapIntegrationTests::
importedTeacherCanBeSelectedForDraftBeforeCancelRestoresOriginal()
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
        QStringLiteral("imported-teacher-setup.tps")
        );
    const QString workbookPath = QDir(
        QStringLiteral(CLASSMNGR_SOURCE_DIR)
        ).filePath(QStringLiteral("tests/fixtures/teacher_import/sectioned_review.xlsx"));
    const QString recoveryJournalPath = workspaceRoot.filePath(
        QStringLiteral("initial-setup-recovery.json")
        );
    QString fixtureError;
    QVERIFY2(
        createWorkspaceWithFixture(
            profilePath,
            QStringLiteral("representative_startup.sql"),
            &fixtureError
            ),
        qPrintable(fixtureError)
        );
    QByteArray originalProfileBytes;
    {
        QFile originalProfile(profilePath);
        QVERIFY(originalProfile.open(QIODevice::ReadOnly));
        originalProfileBytes = originalProfile.readAll();
        QVERIFY(!originalProfileBytes.isEmpty());
    }

    QApplication& application = *qApp;
    ApplicationStyleSheetGuard styleSheetGuard(application);
    auto historyProbe = std::make_shared<RecentWorkspaceHistoryProbe>();
    Bootstrap::ApplicationBootstrap::StartupWorkspaceSeams seams;
    seams.commandLineArguments = QStringList{
        QStringLiteral("imported-teacher-setup-test")
    };
    seams.recentAutoOpenEnabled = false;
    seams.initialSetupRecoveryJournalPath = recoveryJournalPath;
    seams.recentWorkspaceHistoryPortFactory = [historyProbe] {
        return std::make_unique<RecordingRecentWorkspaceHistoryPort>(historyProbe);
    };
    seams.initialSetupFileDialogPortFactory = [profilePath, workbookPath] {
        return std::make_unique<SelectedProfileFileDialog>(
            profilePath,
            workbookPath
            );
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
    QVERIFY(bootstrap.start().succeeded());
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

    QFile journalFile(recoveryJournalPath);
    QVERIFY(journalFile.open(QIODevice::ReadOnly));
    const QJsonDocument journalDocument = QJsonDocument::fromJson(
        journalFile.readAll()
        );
    journalFile.close();
    QVERIFY(journalDocument.isObject());
    const QString originalBackup = journalDocument.object().value(
        QStringLiteral("originalBackup")
        ).toString();
    QVERIFY(!originalBackup.isEmpty());
    QFile preservedOriginal(originalBackup);
    QVERIFY(preservedOriginal.open(QIODevice::ReadOnly));
    QCOMPARE(preservedOriginal.readAll(), originalProfileBytes);
    preservedOriginal.close();

    auto* const importTeachers = page->findChild<QPushButton*>(
        QStringLiteral("classMngrNextInitialSetupImportTeachers")
        );
    auto* const teacherStatus = page->findChild<QLabel*>(
        QStringLiteral("classMngrNextInitialSetupTeacherEntryStatus")
        );
    auto* const continueTeacherEntry = page->findChild<QPushButton*>(
        QStringLiteral("classMngrNextInitialSetupContinueTeacherEntry")
        );
    QVERIFY(importTeachers && teacherStatus && continueTeacherEntry);
    const auto teachersBeforeImport = tableRowCount(
        profilePath,
        QStringLiteral("teachers")
        );
    QVERIFY(teachersBeforeImport.has_value());
    QCOMPARE(*teachersBeforeImport, 0);

    QString importInteractionError;
    int importInteractionPhase = 0;
    QTimer importInteractionTimer;
    importInteractionTimer.setInterval(10);
    QObject::connect(
        &importInteractionTimer,
        &QTimer::timeout,
        &importInteractionTimer,
        [&] {
            auto* const dialog = qobject_cast<QDialog*>(
                QApplication::activeModalWidget()
                );
            if (!dialog)
            {
                return;
            }

            if (importInteractionPhase == 0)
            {
                auto* const browse = dialog->findChild<QPushButton*>(
                    QStringLiteral("classMngrNextTeacherImportBrowse")
                    );
                if (!browse)
                {
                    importInteractionError = QStringLiteral(
                        "The setup teacher-import dialog did not expose Browse."
                        );
                    return;
                }
                importInteractionPhase = 1;
                browse->click();
                return;
            }

            auto* const status = dialog->findChild<QLabel*>(
                QStringLiteral("classMngrNextTeacherImportStatus")
                );
            if (importInteractionPhase != 1 || !status
                || !status->text().contains(QStringLiteral("Review the workbook")))
            {
                return;
            }

            auto* const selectM1 = dialog->findChild<QRadioButton*>(
                QStringLiteral("classMngrNextTeacherImportSelect_0")
                );
            auto* const noneM2 = dialog->findChild<QRadioButton*>(
                QStringLiteral("classMngrNextTeacherImportNone_1")
                );
            auto* const noneH1 = dialog->findChild<QRadioButton*>(
                QStringLiteral("classMngrNextTeacherImportNone_2")
                );
            auto* const firstM1 = dialog->findChild<QCheckBox*>(
                QStringLiteral("classMngrNextTeacherImportCandidate_0_0")
                );
            auto* const secondM1 = dialog->findChild<QCheckBox*>(
                QStringLiteral("classMngrNextTeacherImportCandidate_0_1")
                );
            auto* const apply = dialog->findChild<QPushButton*>(
                QStringLiteral("classMngrNextTeacherImportApply")
                );
            if (!selectM1 || !noneM2 || !noneH1
                || !firstM1 || !secondM1 || !apply
                || !firstM1->isChecked() || secondM1->isChecked())
            {
                importInteractionError = QStringLiteral(
                    "The fixture's Korean teacher review choices were unexpected."
                    );
                return;
            }

            firstM1->setChecked(true);
            secondM1->setChecked(true);
            selectM1->setChecked(true);
            noneM2->setChecked(true);
            noneH1->setChecked(true);
            importInteractionPhase = 2;
            apply->click();
        }
        );
    QTimer importInteractionTimeout;
    importInteractionTimeout.setSingleShot(true);
    QObject::connect(
        &importInteractionTimeout,
        &QTimer::timeout,
        &importInteractionTimeout,
        [&] {
            if (importInteractionPhase >= 2)
            {
                return;
            }
            importInteractionError = QStringLiteral(
                "The pending-profile teacher import did not reach Apply."
                );
            if (QWidget* const modal = QApplication::activeModalWidget())
            {
                if (auto* const cancel = modal->findChild<QPushButton*>(
                        QStringLiteral("classMngrNextTeacherImportCancel")))
                {
                    cancel->click();
                }
            }
        }
        );
    importInteractionTimer.start();
    importInteractionTimeout.start(10000);
    importTeachers->click();
    importInteractionTimer.stop();
    importInteractionTimeout.stop();
    QVERIFY2(importInteractionError.isEmpty(),
             qPrintable(importInteractionError));
    QCOMPARE(importInteractionPhase, 2);
    QVERIFY(teacherStatus->text().contains(
        QStringLiteral("Korean teachers: 2 created")
        ));
    const auto importedTeachersCount = tableRowCount(
        profilePath,
        QStringLiteral("teachers")
        );
    QVERIFY(importedTeachersCount.has_value());
    QCOMPARE(*importedTeachersCount, 2);

    auto* const classDetailsGroup = page->findChild<QGroupBox*>(
        QStringLiteral("classMngrNextInitialSetupClassDetails")
        );
    auto* const classTeacher = page->findChild<QComboBox*>(
        QStringLiteral("classMngrNextInitialSetupClassTeacher")
        );
    auto* const classColorButton = page->findChild<QPushButton*>(
        QStringLiteral("classMngrNextInitialSetupClassColorChoose")
        );
    auto* const continueClassDetails = page->findChild<QPushButton*>(
        QStringLiteral("classMngrNextInitialSetupClassDetailsContinue")
        );
    auto* const cancelSetup = page->findChild<QPushButton*>(
        QStringLiteral("classMngrNextInitialSetupCancel")
        );
    auto* const setupStatus = page->findChild<QLabel*>(
        QStringLiteral("classMngrNextInitialSetupStatus")
        );
    QVERIFY(classDetailsGroup && classTeacher && classColorButton);
    QVERIFY(continueClassDetails && cancelSetup && setupStatus);

    continueTeacherEntry->click();
    QVERIFY(!classDetailsGroup->isHidden());
    QCOMPARE(classTeacher->count(), 3);
    classTeacher->setCurrentIndex(2);
    const QString selectedTeacherId = classTeacher->currentData().toString();
    QVERIFY(!selectedTeacherId.isEmpty());
    const std::optional<QString> selectedKoreanTeacher = readTeacherKoreanName(
        profilePath,
        selectedTeacherId
        );
    QVERIFY(selectedKoreanTeacher.has_value());
    QVERIFY(!selectedKoreanTeacher->trimmed().isEmpty());

    bool colorDialogAccepted = false;
    chooseColorInNextModalDialog(QColor(QStringLiteral("#234567")),
                                 &colorDialogAccepted);
    classColorButton->click();
    QVERIFY(colorDialogAccepted);
    continueClassDetails->click();

    QVERIFY(bootstrap.initialSetupClassDetailsDraft().has_value());
    const Application::InitialSetupClassDetailsDraft draft =
        *bootstrap.initialSetupClassDetailsDraft();
    QCOMPARE(draft.koreanTeacherId.value(), selectedTeacherId.toStdString());
    QVERIFY(draft.classColorChosen);
    QCOMPARE(draft.classColor, std::u16string(u"#234567"));
    QCOMPARE(draft.fontColor, std::u16string(u"#FFFFFF"));
    QCOMPARE(draft.classGrade, std::u16string(u"E4"));
    QCOMPARE(draft.classLevel, std::u16string(u"Theseus"));
    QVERIFY(draft.readingBook.empty());
    QVERIFY(draft.essayBook.empty());
    QVERIFY(bootstrap.workspaceStateSnapshot().session().has_value());
    QCOMPARE(historyProbe->saveCount, 0);
    QVERIFY(historyProbe->savedHistories.empty());
    const auto classesCount = tableRowCount(
        profilePath,
        QStringLiteral("classes")
        );
    const auto classDetailsCount = tableRowCount(
        profilePath,
        QStringLiteral("class_info")
        );
    const auto regularTimesCount = tableRowCount(
        profilePath,
        QStringLiteral("class_times")
        );
    const auto intensiveTimesCount = tableRowCount(
        profilePath,
        QStringLiteral("class_intensive_times")
        );
    QVERIFY(classesCount && classDetailsCount
        && regularTimesCount && intensiveTimesCount);
    QCOMPARE(*classesCount, 0);
    QCOMPARE(*classDetailsCount, 0);
    QCOMPARE(*regularTimesCount, 0);
    QCOMPARE(*intensiveTimesCount, 0);

    cancelSetup->click();
    QCOMPARE(setupStatus->text(), QStringLiteral("Choose where to save your profile."));
    QVERIFY(!bootstrap.initialSetupClassDetailsDraft().has_value());
    QVERIFY(!bootstrap.workspaceStateSnapshot().session().has_value());
    QFile restoredOriginal(profilePath);
    QVERIFY(restoredOriginal.open(QIODevice::ReadOnly));
    QCOMPARE(restoredOriginal.readAll(), originalProfileBytes);
    restoredOriginal.close();
    QVERIFY(!QFileInfo::exists(originalBackup));
    QVERIFY(!QFileInfo::exists(recoveryJournalPath));
    QCOMPARE(historyProbe->saveCount, 0);
    QVERIFY(historyProbe->savedHistories.empty());

    QVERIFY(bootstrap.shutdown().shutdownRequested);
    QCOMPARE(historyProbe->saveCount, 0);
}

QTEST_MAIN(InitialSetupTeacherEntryBootstrapIntegrationTests)

#include "initial_setup_teacher_entry_bootstrap_integration_tests.moc"
