#include "core/settingsmanager.h"
#include "data/database/workspace_store.h"
#include "next/bootstrap/application_bootstrap.h"
#include "next/bootstrap/startup_update_dispatcher.h"
#include "next/resources/resource_root_resolver.h"
#include "next/ui/application_shell.h"
#include "next/ui/initial_page.h"
#include "next/ui/page_host.h"
#include "next/application/file_dialog_port.h"
#include "next/resources/resource_catalog.h"
#include "next/resources/resource_loader.h"
#include "next/ui/page_host.h"
#include "preferences_test_ports.h"

#include <QApplication>
#include <QCheckBox>
#include <QColor>
#include <QColorDialog>
#include <QComboBox>
#include <QDir>
#include <QDialog>
#include <QEvent>
#include <QFile>
#include <QFileInfo>
#include <QGroupBox>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QMessageBox>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QTimer>
#include <QTranslator>
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

[[nodiscard]] bool executeWorkspaceSql(
    const QString& path,
    const QString& statement
    )
{
    const QString connectionName = QStringLiteral("setup-write-%1").arg(
        QUuid::createUuid().toString(QUuid::WithoutBraces)
        );
    bool succeeded = false;
    {
        QSqlDatabase database = QSqlDatabase::addDatabase(
            QStringLiteral("QSQLITE"), connectionName
            );
        database.setDatabaseName(path);
        if (database.open())
        {
            QSqlQuery query(database);
            succeeded = query.exec(statement);
            database.close();
        }
    }
    QSqlDatabase::removeDatabase(connectionName);
    return succeeded;
}

struct ClassRecord final
{
    int id = 0;
    QString name;
};

[[nodiscard]] std::optional<ClassRecord> readFirstClass(const QString& path)
{
    const QString connectionName = QStringLiteral("setup-class-%1").arg(
        QUuid::createUuid().toString(QUuid::WithoutBraces)
        );
    std::optional<ClassRecord> record;
    {
        QSqlDatabase database = QSqlDatabase::addDatabase(
            QStringLiteral("QSQLITE"), connectionName
            );
        database.setDatabaseName(path);
        if (database.open())
        {
            QSqlQuery query(database);
            if (query.exec(QStringLiteral("SELECT id, name FROM classes ORDER BY id LIMIT 1"))
                && query.next())
            {
                record = ClassRecord{
                    .id = query.value(0).toInt(),
                    .name = query.value(1).toString()
                };
            }
            database.close();
        }
    }
    QSqlDatabase::removeDatabase(connectionName);
    return record;
}

struct ClassDetailsRecord final
{
    int classId = 0;
    qint64 teacherId = 0;
    QString grade;
    QString level;
    QString readingBook;
    QString essayBook;
    QString classColor;
    QString fontColor;
};

[[nodiscard]] std::optional<ClassDetailsRecord> readClassDetails(
    const QString& path
    )
{
    const QString connectionName = QStringLiteral("setup-class-details-%1").arg(
        QUuid::createUuid().toString(QUuid::WithoutBraces)
        );
    std::optional<ClassDetailsRecord> record;
    {
        QSqlDatabase database = QSqlDatabase::addDatabase(
            QStringLiteral("QSQLITE"), connectionName
            );
        database.setDatabaseName(path);
        if (database.open())
        {
            QSqlQuery query(database);
            if (query.exec(QStringLiteral(
                    "SELECT class_id, teacher_id, class_grade, class_level, "
                    "reading_book, essay_book, class_color, font_color "
                    "FROM class_info ORDER BY class_id LIMIT 1"
                    ))
                && query.next())
            {
                record = ClassDetailsRecord{
                    .classId = query.value(0).toInt(),
                    .teacherId = query.value(1).toLongLong(),
                    .grade = query.value(2).toString(),
                    .level = query.value(3).toString(),
                    .readingBook = query.value(4).toString(),
                    .essayBook = query.value(5).toString(),
                    .classColor = query.value(6).toString(),
                    .fontColor = query.value(7).toString()
                };
            }
            database.close();
        }
    }
    QSqlDatabase::removeDatabase(connectionName);
    return record;
}

struct ScheduleRowRecord final
{
    int classId = 0;
    QString day;
    QString start;
    QString end;
};

[[nodiscard]] std::optional<std::vector<ScheduleRowRecord>> readScheduleRows(
    const QString& path,
    const QString& tableName
    )
{
    const QString connectionName = QStringLiteral("setup-schedule-%1").arg(
        QUuid::createUuid().toString(QUuid::WithoutBraces)
        );
    std::optional<std::vector<ScheduleRowRecord>> rows;
    {
        QSqlDatabase database = QSqlDatabase::addDatabase(
            QStringLiteral("QSQLITE"), connectionName
            );
        database.setDatabaseName(path);
        if (database.open())
        {
            QSqlQuery query(database);
            if (query.exec(QStringLiteral(
                    "SELECT class_id, day, start_time, end_time FROM %1 ORDER BY rowid"
                    ).arg(tableName)))
            {
                rows.emplace();
                while (query.next())
                {
                    rows->push_back({
                        .classId = query.value(0).toInt(),
                        .day = query.value(1).toString(),
                        .start = query.value(2).toString(),
                        .end = query.value(3).toString()
                    });
                }
            }
            database.close();
        }
    }
    QSqlDatabase::removeDatabase(connectionName);
    return rows;
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
    void importedTeacherTimesPersistBeforeCancelRestoresOriginal();
    void classTimesLabelsAndBehavior_data();
    void classTimesLabelsAndBehavior();
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
    QVERIFY(readingBook && essayBook);
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
importedTeacherTimesPersistBeforeCancelRestoresOriginal()
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
    auto* const readingBook = page->findChild<QComboBox*>(
        QStringLiteral("classMngrNextInitialSetupReadingBook")
        );
    auto* const essayBook = page->findChild<QComboBox*>(
        QStringLiteral("classMngrNextInitialSetupEssayBook")
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
    QVERIFY(readingBook && essayBook);
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

    const int readingBookIndex = readingBook->findText(
        QStringLiteral("Reading Explorer 1")
        );
    const int essayBookIndex = essayBook->findText(QStringLiteral("4A"));
    QVERIFY(readingBookIndex >= 0);
    QVERIFY(essayBookIndex >= 0);
    readingBook->setCurrentIndex(readingBookIndex);
    essayBook->setCurrentIndex(essayBookIndex);

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
    QCOMPARE(draft.readingBook, std::u16string(u"Reading Explorer 1"));
    QCOMPARE(draft.essayBook, std::u16string(u"4A"));
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

    auto* const classTimesGroup = page->findChild<QGroupBox*>(
        QStringLiteral("classMngrNextInitialSetupClassTimes")
        );
    auto* const addRegular = page->findChild<QPushButton*>(
        QStringLiteral("classMngrNextInitialSetupAddRegularTime")
        );
    auto* const addIntensive = page->findChild<QPushButton*>(
        QStringLiteral("classMngrNextInitialSetupAddIntensiveTime")
        );
    auto* const continueClassTimes = page->findChild<QPushButton*>(
        QStringLiteral("classMngrNextInitialSetupClassTimesContinue")
        );
    auto* const classTimesStatus = page->findChild<QLabel*>(
        QStringLiteral("classMngrNextInitialSetupClassTimesStatus")
        );
    QVERIFY(classTimesGroup && addRegular && addIntensive);
    QVERIFY(continueClassTimes && classTimesStatus);
    QVERIFY(!classTimesGroup->isHidden());
    QVERIFY(classDetailsGroup->isHidden());

    const auto regularRows = [&] {
        return classTimesGroup->findChildren<QWidget*>(
            QStringLiteral("classMngrNextInitialSetupRegularScheduleRow")
            );
    };
    const auto intensiveRows = [&] {
        return classTimesGroup->findChildren<QWidget*>(
            QStringLiteral("classMngrNextInitialSetupIntensiveScheduleRow")
            );
    };
    const auto rowCombo = [](QWidget* row, const char* objectName) {
        return row->findChild<QComboBox*>(QString::fromLatin1(objectName));
    };

    QCOMPARE(regularRows().size(), 1);
    QWidget* regularDefaultRow = regularRows().front();
    auto* regularDay = rowCombo(
        regularDefaultRow, "classMngrNextInitialSetupScheduleDay"
        );
    auto* regularHour = rowCombo(
        regularDefaultRow, "classMngrNextInitialSetupScheduleHour"
        );
    auto* regularMinute = rowCombo(
        regularDefaultRow, "classMngrNextInitialSetupScheduleMinute"
        );
    auto* regularPeriod = rowCombo(
        regularDefaultRow, "classMngrNextInitialSetupSchedulePeriod"
        );
    auto* regularEnd = rowCombo(
        regularDefaultRow, "classMngrNextInitialSetupScheduleEnd"
        );
    QVERIFY(regularDay && regularHour && regularMinute && regularPeriod && regularEnd);
    QCOMPARE(regularDay->count(), 7);
    QCOMPARE(regularDay->currentData().toString(), QStringLiteral("Monday"));
    QCOMPARE(regularHour->count(), 7);
    QCOMPARE(regularHour->itemText(0), QStringLiteral("3"));
    QCOMPARE(regularHour->itemText(6), QStringLiteral("9"));
    QCOMPARE(regularHour->currentText(), QStringLiteral("4"));
    QCOMPARE(regularMinute->count(), 5);
    QCOMPARE(regularMinute->itemText(0), QStringLiteral(":00"));
    QCOMPARE(regularMinute->itemText(1), QStringLiteral(":05"));
    QCOMPARE(regularMinute->itemText(2), QStringLiteral(":25"));
    QCOMPARE(regularMinute->itemText(3), QStringLiteral(":30"));
    QCOMPARE(regularMinute->itemText(4), QStringLiteral(":35"));
    QCOMPARE(regularPeriod->itemText(0), QStringLiteral("PM"));
    QCOMPARE(regularPeriod->itemText(1), QStringLiteral("AM"));
    QCOMPARE(regularEnd->currentText(), QStringLiteral("4:55 PM"));
    QCOMPARE(regularEnd->count(), 5);

    addRegular->click();
    QCOMPARE(regularRows().size(), 2);
    QWidget* const addedRegularRow = regularRows().last();
    auto* regularRemoveButton = addedRegularRow->findChild<QPushButton*>(
        QStringLiteral("classMngrNextInitialSetupScheduleRemove")
        );
    QVERIFY(regularRemoveButton);
    regularRemoveButton->click();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QCOMPARE(regularRows().size(), 1);

    addIntensive->click();
    QCOMPARE(intensiveRows().size(), 1);
    QWidget* const addedIntensiveRow = intensiveRows().front();
    auto* intensiveHour = rowCombo(
        addedIntensiveRow, "classMngrNextInitialSetupScheduleHour"
        );
    QVERIFY(intensiveHour);
    QCOMPARE(intensiveHour->count(), 12);
    QCOMPARE(intensiveHour->itemText(0), QStringLiteral("1"));
    QCOMPARE(intensiveHour->itemText(11), QStringLiteral("12"));
    auto* intensiveRemoveButton = addedIntensiveRow->findChild<QPushButton*>(
        QStringLiteral("classMngrNextInitialSetupScheduleRemove")
        );
    QVERIFY(intensiveRemoveButton);
    intensiveRemoveButton->click();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QCOMPARE(intensiveRows().size(), 0);

    auto* regularDefaultRemoveButton = regularDefaultRow->findChild<QPushButton*>(
        QStringLiteral("classMngrNextInitialSetupScheduleRemove")
        );
    QVERIFY(regularDefaultRemoveButton);
    regularDefaultRemoveButton->click();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QCOMPARE(regularRows().size(), 0);
    QTimer::singleShot(0, [] {
        if (auto* const message = qobject_cast<QMessageBox*>(
                QApplication::activeModalWidget()))
        {
            message->accept();
        }
    });
    continueClassTimes->click();
    QVERIFY(regularRows().isEmpty());
    QVERIFY(intensiveRows().isEmpty());
    const auto noClassAfterZeroRows = tableRowCount(
        profilePath,
        QStringLiteral("classes")
        );
    QVERIFY(noClassAfterZeroRows.has_value());
    QCOMPARE(*noClassAfterZeroRows, 0);

    addRegular->click();
    addRegular->click();
    addIntensive->click();
    addIntensive->click();
    QCOMPARE(regularRows().size(), 2);
    QCOMPARE(intensiveRows().size(), 2);

    regularDefaultRow = regularRows().front();
    QWidget* const regularSecondRow = regularRows().back();
    regularDay = rowCombo(regularSecondRow,
                          "classMngrNextInitialSetupScheduleDay");
    regularHour = rowCombo(regularSecondRow,
                           "classMngrNextInitialSetupScheduleHour");
    regularMinute = rowCombo(regularSecondRow,
                             "classMngrNextInitialSetupScheduleMinute");
    regularPeriod = rowCombo(regularSecondRow,
                             "classMngrNextInitialSetupSchedulePeriod");
    regularEnd = rowCombo(regularSecondRow,
                          "classMngrNextInitialSetupScheduleEnd");
    QVERIFY(regularDay && regularHour && regularMinute && regularPeriod && regularEnd);
    regularDay->setCurrentIndex(1);
    regularHour->setCurrentText(QStringLiteral("5"));
    regularMinute->setCurrentText(QStringLiteral(":05"));
    regularEnd->setCurrentText(QStringLiteral("6:30 PM"));

    QWidget* const intensiveFirstRow = intensiveRows().front();
    auto* intensiveDay = rowCombo(
        intensiveFirstRow, "classMngrNextInitialSetupScheduleDay"
        );
    intensiveHour = rowCombo(
        intensiveFirstRow, "classMngrNextInitialSetupScheduleHour"
        );
    auto* intensiveMinute = rowCombo(
        intensiveFirstRow, "classMngrNextInitialSetupScheduleMinute"
        );
    auto* intensivePeriod = rowCombo(
        intensiveFirstRow, "classMngrNextInitialSetupSchedulePeriod"
        );
    auto* intensiveEnd = rowCombo(
        intensiveFirstRow, "classMngrNextInitialSetupScheduleEnd"
        );
    QVERIFY(intensiveDay && intensiveHour && intensiveMinute
            && intensivePeriod && intensiveEnd);
    intensiveDay->setCurrentIndex(3);
    intensiveHour->setCurrentText(QStringLiteral("1"));
    intensiveMinute->setCurrentText(QStringLiteral(":25"));
    intensivePeriod->setCurrentText(QStringLiteral("AM"));
    intensiveEnd->setCurrentText(QStringLiteral("2:50 AM"));

    QWidget* const intensiveSecondRow = intensiveRows().back();
    intensiveDay = rowCombo(
        intensiveSecondRow, "classMngrNextInitialSetupScheduleDay"
        );
    intensiveHour = rowCombo(
        intensiveSecondRow, "classMngrNextInitialSetupScheduleHour"
        );
    intensiveMinute = rowCombo(
        intensiveSecondRow, "classMngrNextInitialSetupScheduleMinute"
        );
    intensivePeriod = rowCombo(
        intensiveSecondRow, "classMngrNextInitialSetupSchedulePeriod"
        );
    intensiveEnd = rowCombo(
        intensiveSecondRow, "classMngrNextInitialSetupScheduleEnd"
        );
    QVERIFY(intensiveDay && intensiveHour && intensiveMinute
            && intensivePeriod && intensiveEnd);
    intensiveDay->setCurrentIndex(6);
    intensiveHour->setCurrentText(QStringLiteral("12"));
    intensiveEnd->setCurrentText(QStringLiteral("3:55 PM"));

    QVERIFY(executeWorkspaceSql(
        profilePath,
        QStringLiteral(
            "CREATE TRIGGER fail_initial_setup_class_create "
            "BEFORE INSERT ON classes BEGIN "
            "SELECT RAISE(ABORT, 'injected class creation failure'); END"
            )
        ));
    continueClassTimes->click();
    QVERIFY(classTimesStatus->text().contains(QStringLiteral("retry")));
    const auto classesAfterCreateFailure = tableRowCount(
        profilePath, QStringLiteral("classes")
        );
    QVERIFY(classesAfterCreateFailure.has_value());
    QCOMPARE(*classesAfterCreateFailure, 0);
    QVERIFY(executeWorkspaceSql(
        profilePath,
        QStringLiteral("DROP TRIGGER fail_initial_setup_class_create")
        ));

    QVERIFY(executeWorkspaceSql(
        profilePath,
        QStringLiteral(
            "CREATE TRIGGER fail_initial_setup_intensive_insert "
            "BEFORE INSERT ON class_intensive_times BEGIN "
            "SELECT RAISE(ABORT, 'injected intensive schedule write failure'); END"
            )
        ));
    continueClassTimes->click();
    QVERIFY(classTimesStatus->text().contains(QStringLiteral("retry")));
    const std::optional<ClassRecord> classAfterDetailsFailure = readFirstClass(profilePath);
    QVERIFY(classAfterDetailsFailure.has_value());
    QVERIFY(classAfterDetailsFailure->name.isEmpty());
    const auto classesAfterSaveFailure = tableRowCount(
        profilePath, QStringLiteral("classes")
        );
    const auto detailsAfterSaveFailure = tableRowCount(
        profilePath, QStringLiteral("class_info")
        );
    const auto regularAfterSaveFailure = tableRowCount(
        profilePath, QStringLiteral("class_times")
        );
    const auto intensiveAfterSaveFailure = tableRowCount(
        profilePath, QStringLiteral("class_intensive_times")
        );
    QVERIFY(classesAfterSaveFailure && detailsAfterSaveFailure
            && regularAfterSaveFailure && intensiveAfterSaveFailure);
    QCOMPARE(*classesAfterSaveFailure, 1);
    QCOMPARE(*detailsAfterSaveFailure, 0);
    QCOMPARE(*regularAfterSaveFailure, 0);
    QCOMPARE(*intensiveAfterSaveFailure, 0);
    QVERIFY(bootstrap.initialSetupClassDetailsDraft().has_value());
    QCOMPARE(
        bootstrap.initialSetupClassDetailsDraft()->koreanTeacherId.value(),
        selectedTeacherId.toStdString()
        );
    QVERIFY(executeWorkspaceSql(
        profilePath,
        QStringLiteral("DROP TRIGGER fail_initial_setup_intensive_insert")
        ));
    continueClassTimes->click();

    const auto classesAfterRetry = tableRowCount(
        profilePath, QStringLiteral("classes")
        );
    const auto detailsAfterRetry = tableRowCount(
        profilePath, QStringLiteral("class_info")
        );
    QVERIFY(classesAfterRetry && detailsAfterRetry);
    QCOMPARE(*classesAfterRetry, 1);
    QCOMPARE(*detailsAfterRetry, 1);
    const std::optional<ClassRecord> savedClass = readFirstClass(profilePath);
    QVERIFY(savedClass.has_value());
    QCOMPARE(savedClass->id, classAfterDetailsFailure->id);
    QVERIFY(savedClass->name.isEmpty());
    const std::optional<ClassDetailsRecord> details = readClassDetails(profilePath);
    QVERIFY(details.has_value());
    QCOMPARE(details->classId, savedClass->id);
    QCOMPARE(details->teacherId, selectedTeacherId.toLongLong());
    QCOMPARE(details->grade, QStringLiteral("E4"));
    QCOMPARE(details->level, QStringLiteral("Theseus"));
    QCOMPARE(details->readingBook, QStringLiteral("Reading Explorer 1"));
    QCOMPARE(details->essayBook, QStringLiteral("4A"));
    QCOMPARE(details->classColor, QStringLiteral("#234567"));
    QCOMPARE(details->fontColor, QStringLiteral("#FFFFFF"));

    const auto savedRegularRows = readScheduleRows(
        profilePath, QStringLiteral("class_times")
        );
    const auto savedIntensiveRows = readScheduleRows(
        profilePath, QStringLiteral("class_intensive_times")
        );
    QVERIFY(savedRegularRows.has_value());
    QVERIFY(savedIntensiveRows.has_value());
    QCOMPARE(savedRegularRows->size(), std::size_t(2));
    QCOMPARE(savedIntensiveRows->size(), std::size_t(2));
    QCOMPARE(savedRegularRows->at(0).classId, savedClass->id);
    QCOMPARE(savedRegularRows->at(0).day, QStringLiteral("Monday"));
    QCOMPARE(savedRegularRows->at(0).start, QStringLiteral("4:00 PM"));
    QCOMPARE(savedRegularRows->at(0).end, QStringLiteral("4:55 PM"));
    QCOMPARE(savedRegularRows->at(1).classId, savedClass->id);
    QCOMPARE(savedRegularRows->at(1).day, QStringLiteral("Tuesday"));
    QCOMPARE(savedRegularRows->at(1).start, QStringLiteral("5:05 PM"));
    QCOMPARE(savedRegularRows->at(1).end, QStringLiteral("6:30 PM"));
    QCOMPARE(savedIntensiveRows->at(0).classId, savedClass->id);
    QCOMPARE(savedIntensiveRows->at(0).day, QStringLiteral("Thursday"));
    QCOMPARE(savedIntensiveRows->at(0).start, QStringLiteral("1:25 AM"));
    QCOMPARE(savedIntensiveRows->at(0).end, QStringLiteral("2:50 AM"));
    QCOMPARE(savedIntensiveRows->at(1).classId, savedClass->id);
    QCOMPARE(savedIntensiveRows->at(1).day, QStringLiteral("Sunday"));
    QCOMPARE(savedIntensiveRows->at(1).start, QStringLiteral("12:00 PM"));
    QCOMPARE(savedIntensiveRows->at(1).end, QStringLiteral("3:55 PM"));

    QVERIFY(!classTimesGroup->isEnabled());
    QVERIFY(classTimesStatus->text().contains(QStringLiteral("saved")));
    QCOMPARE(setupStatus->text(), QStringLiteral("Profile created. Setup is pending."));
    QVERIFY(bootstrap.workspaceStateSnapshot().session().has_value());
    QVERIFY(QFileInfo::exists(recoveryJournalPath));
    QVERIFY(QFileInfo::exists(originalBackup));
    QCOMPARE(historyProbe->saveCount, 0);
    QVERIFY(historyProbe->savedHistories.empty());

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

void InitialSetupTeacherEntryBootstrapIntegrationTests::
classTimesLabelsAndBehavior_data()
{
    QTest::addColumn<QString>("locale");
    QTest::addColumn<QString>("theme");
    QTest::addColumn<QString>("classTimesTitle");
    QTest::addColumn<QString>("zeroRowsMessage");
    QTest::addColumn<QString>("expectedRegularHeading");
    QTest::addColumn<QString>("expectedIntensiveHeading");
    QTest::addColumn<QString>("dayLabel");
    QTest::addColumn<QString>("removeLabel");
    QTest::addColumn<QString>("addRegularLabel");
    QTest::addColumn<QString>("addIntensiveLabel");
    QTest::addColumn<QString>("continueLabel");
    QTest::addColumn<QString>("savedStatus");

    const auto add = [](const char* name, const char* locale,
                        const char* theme, const char* title,
                        const char* zeroMessage,
                        const char* regular, const char* intensive,
                        const char* day, const char* remove,
                        const char* addRegular, const char* addIntensive,
                        const char* next, const char* saved) {
        QTest::newRow(name)
            << QString::fromLatin1(locale)
            << QString::fromLatin1(theme)
            << QString::fromUtf8(title)
            << QString::fromUtf8(zeroMessage)
            << QString::fromUtf8(regular)
            << QString::fromUtf8(intensive)
            << QString::fromUtf8(day)
            << QString::fromUtf8(remove)
            << QString::fromUtf8(addRegular)
            << QString::fromUtf8(addIntensive)
            << QString::fromUtf8(next)
            << QString::fromUtf8(saved);
    };
    add("english-light", "en_US", "light", "Class Times",
        "Enter at least one regular or intensive class time before continuing.",
        "Regular Schedule", "Intensive Schedule", "Day", "Remove",
        "+ Add Time", "+ Add Intensive Time", "Continue",
        "Class details and times saved. Setup remains pending.");
    add("english-dark", "en_US", "dark", "Class Times",
        "Enter at least one regular or intensive class time before continuing.",
        "Regular Schedule", "Intensive Schedule", "Day", "Remove",
        "+ Add Time", "+ Add Intensive Time", "Continue",
        "Class details and times saved. Setup remains pending.");
    add("korean-light", "ko_KR", "light", "수업 시간",
        "계속하려면 정규 또는 집중 수업 시간을 하나 이상 입력하세요.",
        "정규 수업 일정", "집중 수업 일정", "요일", "제거",
        "+ 시간 추가", "+ 집중 수업 시간 추가", "계속",
        "학급 정보와 수업 시간을 저장했습니다. 초기 설정은 계속 진행 중입니다.");
    add("korean-dark", "ko_KR", "dark", "수업 시간",
        "계속하려면 정규 또는 집중 수업 시간을 하나 이상 입력하세요.",
        "정규 수업 일정", "집중 수업 일정", "요일", "제거",
        "+ 시간 추가", "+ 집중 수업 시간 추가", "계속",
        "학급 정보와 수업 시간을 저장했습니다. 초기 설정은 계속 진행 중입니다.");
}

void InitialSetupTeacherEntryBootstrapIntegrationTests::classTimesLabelsAndBehavior()
{
    QFETCH(QString, locale);
    QFETCH(QString, theme);
    QFETCH(QString, classTimesTitle);
    QFETCH(QString, zeroRowsMessage);
    QFETCH(QString, expectedRegularHeading);
    QFETCH(QString, expectedIntensiveHeading);
    QFETCH(QString, dayLabel);
    QFETCH(QString, removeLabel);
    QFETCH(QString, addRegularLabel);
    QFETCH(QString, addIntensiveLabel);
    QFETCH(QString, continueLabel);
    QFETCH(QString, savedStatus);

    QApplication& application = *qApp;
    ApplicationStyleSheetGuard styleSheetGuard(application);
    QTranslator translator;
    if (locale == QStringLiteral("ko_KR"))
    {
        const QString translationPath = QDir(
            QFileInfo(builtResourceCatalogPath()).absolutePath()
            ).filePath(QStringLiteral("assets/translations/ClassMngr_ko_KR.qm"));
        QVERIFY2(translator.load(translationPath), qPrintable(translationPath));
        QVERIFY(application.installTranslator(&translator));
    }

    QFile styleFile(QDir(builtResourceRoot()).filePath(
        QStringLiteral("assets/styles/%1.qss").arg(theme)
        ));
    QVERIFY2(styleFile.open(QIODevice::ReadOnly), qPrintable(styleFile.fileName()));
    application.setStyleSheet(QString::fromUtf8(styleFile.readAll()));

    auto catalog = Resources::ResourceCatalog::load(
        builtResourceCatalogPath(), builtResourceRoot()
        );
    QVERIFY2(catalog.has_value(),
             catalog ? "" : qPrintable(catalog.error().message));
    Resources::ResourceLoader loader(std::move(catalog.value()));
    Test::MemoryPreferences preferences;
    Ui::PageHost host(loader, preferences.model);

    Ui::InitialSetupPageState state{
        .setupPending = true,
        .classDetailsReady = true
    };
    int saveCalls = 0;
    std::optional<Application::InitialSetupClassTimes> savedTimes;
    const Ui::InitialSetupPageActions actions{
        .state = [&state] { return state; },
        .loadPersonalDetails = [] {
            return Application::PersonalDetailsReadResult::success(
                Application::PersonalDetailsSaveRequest{}
                );
        },
        .saveClassTimes = [&] (
            const Application::InitialSetupClassTimes& times
            ) {
            ++saveCalls;
            savedTimes = times;
            state.classTimesSaved = true;
            return Application::InitialSetupClassTimesSaveResult::success();
        }
    };
    const Ui::PageDescriptor descriptor{
        .id = *Ui::PageId::fromString(QStringLiteral("initial_setup")),
        .label = QStringLiteral("Initial Setup"),
        .factory = [actions](const Ui::PageContext& context)
            -> Ui::PageOperationResult<std::unique_ptr<Ui::Page>> {
            return std::unique_ptr<Ui::Page>(
                std::make_unique<Ui::InitialPage>(context, actions)
                );
        }
    };
    QVERIFY(host.registerPage(descriptor));
    QVERIFY(host.activatePage(QStringLiteral("initial_setup")));
    auto* const page = dynamic_cast<Ui::InitialPage*>(
        host.pageInstance(QStringLiteral("initial_setup"))
        );
    QVERIFY(page);
    auto* const group = page->findChild<QGroupBox*>(
        QStringLiteral("classMngrNextInitialSetupClassTimes")
        );
    auto* const regularHeading = page->findChild<QLabel*>(
        QStringLiteral("classMngrNextInitialSetupRegularScheduleHeading")
        );
    auto* const intensiveHeading = page->findChild<QLabel*>(
        QStringLiteral("classMngrNextInitialSetupIntensiveScheduleHeading")
        );
    auto* const addRegular = page->findChild<QPushButton*>(
        QStringLiteral("classMngrNextInitialSetupAddRegularTime")
        );
    auto* const addIntensive = page->findChild<QPushButton*>(
        QStringLiteral("classMngrNextInitialSetupAddIntensiveTime")
        );
    auto* const continueTimes = page->findChild<QPushButton*>(
        QStringLiteral("classMngrNextInitialSetupClassTimesContinue")
        );
    QVERIFY(group && regularHeading && intensiveHeading);
    QVERIFY(addRegular && addIntensive && continueTimes);
    QCOMPARE(group->title(), classTimesTitle);
    QCOMPARE(regularHeading->text(), expectedRegularHeading);
    QCOMPARE(intensiveHeading->text(), expectedIntensiveHeading);
    QCOMPARE(addRegular->text(), addRegularLabel);
    QCOMPARE(addIntensive->text(), addIntensiveLabel);
    QCOMPARE(continueTimes->text(), continueLabel);
    QVERIFY(!group->isHidden());
    QVERIFY(group->isEnabled());

    auto rows = [&group](const QString& objectName) {
        QList<QWidget*> visibleRows;
        for (QWidget* row : group->findChildren<QWidget*>(objectName))
        {
            if (!row->isHidden())
            {
                visibleRows.append(row);
            }
        }
        return visibleRows;
    };
    const QString regularRowName =
        QStringLiteral("classMngrNextInitialSetupRegularScheduleRow");
    const QString intensiveRowName =
        QStringLiteral("classMngrNextInitialSetupIntensiveScheduleRow");
    QCOMPARE(rows(regularRowName).size(), 1);
    QWidget* const defaultRow = rows(regularRowName).front();
    auto* day = defaultRow->findChild<QComboBox*>(
        QStringLiteral("classMngrNextInitialSetupScheduleDay")
        );
    auto* hour = defaultRow->findChild<QComboBox*>(
        QStringLiteral("classMngrNextInitialSetupScheduleHour")
        );
    auto* minute = defaultRow->findChild<QComboBox*>(
        QStringLiteral("classMngrNextInitialSetupScheduleMinute")
        );
    auto* period = defaultRow->findChild<QComboBox*>(
        QStringLiteral("classMngrNextInitialSetupSchedulePeriod")
        );
    auto* end = defaultRow->findChild<QComboBox*>(
        QStringLiteral("classMngrNextInitialSetupScheduleEnd")
        );
    auto* remove = defaultRow->findChild<QPushButton*>(
        QStringLiteral("classMngrNextInitialSetupScheduleRemove")
        );
    QVERIFY(day && hour && minute && period && end && remove);
    QCOMPARE(day->itemText(0), dayLabel == QStringLiteral("Day")
        ? QStringLiteral("Monday") : QStringLiteral("월요일"));
    QCOMPARE(day->currentData().toString(), QStringLiteral("Monday"));
    QCOMPARE(hour->currentText(), QStringLiteral("4"));
    QCOMPARE(minute->currentText(), QStringLiteral(":00"));
    QCOMPARE(period->currentText(), QStringLiteral("PM"));
    QCOMPARE(end->currentText(), QStringLiteral("4:55 PM"));
    QCOMPARE(remove->text(), removeLabel);

    addRegular->click();
    QCOMPARE(rows(regularRowName).size(), 2);
    auto* regularToRemove = rows(regularRowName).last()->findChild<QPushButton*>(
        QStringLiteral("classMngrNextInitialSetupScheduleRemove")
        );
    QVERIFY(regularToRemove);
    regularToRemove->click();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QCOMPARE(rows(regularRowName).size(), 1);

    addIntensive->click();
    QCOMPARE(rows(intensiveRowName).size(), 1);
    auto* intensiveToRemove = rows(intensiveRowName).front()->findChild<QPushButton*>(
        QStringLiteral("classMngrNextInitialSetupScheduleRemove")
        );
    QVERIFY(intensiveToRemove);
    intensiveToRemove->click();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QCOMPARE(rows(intensiveRowName).size(), 0);

    remove->click();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    bool informationShown = false;
    QString informationText;
    QTimer::singleShot(0, [&informationShown, &informationText] {
        QWidget* const modal = QApplication::activeModalWidget();
        informationShown = modal != nullptr;
        if (auto* const information = qobject_cast<QMessageBox*>(
                modal))
        {
            informationText = information->text();
            information->accept();
        }
        else if (modal)
        {
            modal->close();
        }
    });
    continueTimes->click();
    QVERIFY(informationShown);
    QCOMPARE(informationText, zeroRowsMessage);
    QCOMPARE(saveCalls, 0);
    QCOMPARE(rows(regularRowName).size(), 0);

    addRegular->click();
    QWidget* const fullDurationRegularRow = rows(regularRowName).front();
    auto* regularOptionHour = fullDurationRegularRow->findChild<QComboBox*>(
        QStringLiteral("classMngrNextInitialSetupScheduleHour")
        );
    auto* regularOptionMinute = fullDurationRegularRow->findChild<QComboBox*>(
        QStringLiteral("classMngrNextInitialSetupScheduleMinute")
        );
    auto* regularOptionPeriod = fullDurationRegularRow->findChild<QComboBox*>(
        QStringLiteral("classMngrNextInitialSetupSchedulePeriod")
        );
    auto* regularOptionEnd = fullDurationRegularRow->findChild<QComboBox*>(
        QStringLiteral("classMngrNextInitialSetupScheduleEnd")
        );
    QVERIFY(regularOptionHour && regularOptionMinute
            && regularOptionPeriod && regularOptionEnd);
    regularOptionHour->setCurrentText(QStringLiteral("3"));
    regularOptionMinute->setCurrentText(QStringLiteral(":00"));
    regularOptionPeriod->setCurrentText(QStringLiteral("PM"));
    const QStringList expectedRegularEndLabels{
        QStringLiteral("3:55 PM"), QStringLiteral("4:25 PM"),
        QStringLiteral("4:55 PM"), QStringLiteral("5:55 PM"),
        QStringLiteral("6:55 PM")
    };
    const QList<int> expectedRegularEndMinutes{955, 985, 1015, 1075, 1135};
    QCOMPARE(regularOptionEnd->count(), expectedRegularEndLabels.size());
    for (qsizetype index = 0; index < expectedRegularEndLabels.size(); ++index)
    {
        QCOMPARE(regularOptionEnd->itemText(index),
                 expectedRegularEndLabels.at(index));
        QCOMPARE(regularOptionEnd->itemData(index).toInt(),
                 expectedRegularEndMinutes.at(index));
        QVERIFY(!regularOptionEnd->itemText(index).contains(
            QStringLiteral("AM")
            ));
    }
    regularOptionEnd->setCurrentIndex(4);

    addRegular->click();
    QWidget* const regularCapRow = rows(regularRowName).back();
    auto* regularCapDay = regularCapRow->findChild<QComboBox*>(
        QStringLiteral("classMngrNextInitialSetupScheduleDay")
        );
    regularOptionHour = regularCapRow->findChild<QComboBox*>(
        QStringLiteral("classMngrNextInitialSetupScheduleHour")
        );
    regularOptionMinute = regularCapRow->findChild<QComboBox*>(
        QStringLiteral("classMngrNextInitialSetupScheduleMinute")
        );
    regularOptionPeriod = regularCapRow->findChild<QComboBox*>(
        QStringLiteral("classMngrNextInitialSetupSchedulePeriod")
        );
    regularOptionEnd = regularCapRow->findChild<QComboBox*>(
        QStringLiteral("classMngrNextInitialSetupScheduleEnd")
        );
    QVERIFY(regularCapDay && regularOptionHour && regularOptionMinute
            && regularOptionPeriod && regularOptionEnd);
    regularCapDay->setCurrentIndex(1);
    regularOptionHour->setCurrentText(QStringLiteral("9"));
    regularOptionMinute->setCurrentText(QStringLiteral(":35"));
    regularOptionPeriod->setCurrentText(QStringLiteral("PM"));
    QCOMPARE(regularOptionEnd->count(), 0);
    QVERIFY(regularOptionEnd->findText(QStringLiteral("10:30 PM")) < 0);
    regularOptionMinute->setCurrentText(QStringLiteral(":00"));
    QCOMPARE(regularOptionEnd->count(), 1);
    QCOMPARE(regularOptionEnd->currentText(), QStringLiteral("9:55 PM"));
    QCOMPARE(regularOptionEnd->currentData().toInt(), 1315);

    addIntensive->click();
    QWidget* const fullDurationIntensiveRow = rows(intensiveRowName).front();
    auto* intensiveOptionDay = fullDurationIntensiveRow->findChild<QComboBox*>(
        QStringLiteral("classMngrNextInitialSetupScheduleDay")
        );
    auto* intensiveOptionHour = fullDurationIntensiveRow->findChild<QComboBox*>(
        QStringLiteral("classMngrNextInitialSetupScheduleHour")
        );
    auto* intensiveOptionMinute = fullDurationIntensiveRow->findChild<QComboBox*>(
        QStringLiteral("classMngrNextInitialSetupScheduleMinute")
        );
    auto* intensiveOptionPeriod = fullDurationIntensiveRow->findChild<QComboBox*>(
        QStringLiteral("classMngrNextInitialSetupSchedulePeriod")
        );
    auto* intensiveOptionEnd = fullDurationIntensiveRow->findChild<QComboBox*>(
        QStringLiteral("classMngrNextInitialSetupScheduleEnd")
        );
    QVERIFY(intensiveOptionDay && intensiveOptionHour && intensiveOptionMinute
            && intensiveOptionPeriod && intensiveOptionEnd);
    intensiveOptionDay->setCurrentIndex(1);
    intensiveOptionHour->setCurrentText(QStringLiteral("1"));
    intensiveOptionMinute->setCurrentText(QStringLiteral(":00"));
    intensiveOptionPeriod->setCurrentText(QStringLiteral("AM"));
    const QStringList expectedIntensiveEndLabels{
        QStringLiteral("1:55 AM"), QStringLiteral("2:25 AM"),
        QStringLiteral("2:55 AM"), QStringLiteral("3:55 AM"),
        QStringLiteral("4:55 AM")
    };
    const QList<int> expectedIntensiveEndMinutes{115, 145, 175, 235, 295};
    QCOMPARE(intensiveOptionEnd->count(), expectedIntensiveEndLabels.size());
    for (qsizetype index = 0; index < expectedIntensiveEndLabels.size(); ++index)
    {
        QCOMPARE(intensiveOptionEnd->itemText(index),
                 expectedIntensiveEndLabels.at(index));
        QCOMPARE(intensiveOptionEnd->itemData(index).toInt(),
                 expectedIntensiveEndMinutes.at(index));
    }
    intensiveOptionEnd->setCurrentIndex(2);

    addIntensive->click();
    QWidget* const intensiveCapRow = rows(intensiveRowName).back();
    intensiveOptionDay = intensiveCapRow->findChild<QComboBox*>(
        QStringLiteral("classMngrNextInitialSetupScheduleDay")
        );
    intensiveOptionHour = intensiveCapRow->findChild<QComboBox*>(
        QStringLiteral("classMngrNextInitialSetupScheduleHour")
        );
    intensiveOptionMinute = intensiveCapRow->findChild<QComboBox*>(
        QStringLiteral("classMngrNextInitialSetupScheduleMinute")
        );
    intensiveOptionPeriod = intensiveCapRow->findChild<QComboBox*>(
        QStringLiteral("classMngrNextInitialSetupSchedulePeriod")
        );
    intensiveOptionEnd = intensiveCapRow->findChild<QComboBox*>(
        QStringLiteral("classMngrNextInitialSetupScheduleEnd")
        );
    QVERIFY(intensiveOptionDay && intensiveOptionHour && intensiveOptionMinute
            && intensiveOptionPeriod && intensiveOptionEnd);
    intensiveOptionDay->setCurrentIndex(2);
    intensiveOptionHour->setCurrentText(QStringLiteral("9"));
    intensiveOptionMinute->setCurrentText(QStringLiteral(":35"));
    intensiveOptionPeriod->setCurrentText(QStringLiteral("PM"));
    QCOMPARE(intensiveOptionEnd->count(), 0);
    QVERIFY(intensiveOptionEnd->findText(QStringLiteral("10:30 PM")) < 0);
    intensiveOptionMinute->setCurrentText(QStringLiteral(":00"));
    QCOMPARE(intensiveOptionEnd->count(), 1);
    QCOMPARE(intensiveOptionEnd->currentText(), QStringLiteral("9:55 PM"));
    QCOMPARE(intensiveOptionEnd->currentData().toInt(), 1315);

    continueTimes->click();
    QCOMPARE(saveCalls, 1);
    QVERIFY(savedTimes.has_value());
    QCOMPARE(savedTimes->regularTimes.size(), std::size_t(2));
    QCOMPARE(savedTimes->intensiveTimes.size(), std::size_t(2));
    QVERIFY(savedTimes->regularTimes.at(0).weekday() == Domain::Weekday::Monday);
    QCOMPARE(savedTimes->regularTimes.at(0).startMinute().rawValue(), 900);
    QCOMPARE(savedTimes->regularTimes.at(0).endMinute().rawValue(), 1135);
    QVERIFY(savedTimes->regularTimes.at(1).weekday() == Domain::Weekday::Tuesday);
    QCOMPARE(savedTimes->regularTimes.at(1).startMinute().rawValue(), 1260);
    QCOMPARE(savedTimes->regularTimes.at(1).endMinute().rawValue(), 1315);
    QVERIFY(savedTimes->intensiveTimes.at(0).weekday() == Domain::Weekday::Tuesday);
    QCOMPARE(savedTimes->intensiveTimes.at(0).startMinute().rawValue(), 60);
    QCOMPARE(savedTimes->intensiveTimes.at(0).endMinute().rawValue(), 175);
    QVERIFY(savedTimes->intensiveTimes.at(1).weekday() == Domain::Weekday::Wednesday);
    QCOMPARE(savedTimes->intensiveTimes.at(1).startMinute().rawValue(), 1260);
    QCOMPARE(savedTimes->intensiveTimes.at(1).endMinute().rawValue(), 1315);
    QVERIFY(state.classTimesSaved);
    QVERIFY(!group->isEnabled());
    auto* const status = page->findChild<QLabel*>(
        QStringLiteral("classMngrNextInitialSetupClassTimesStatus")
        );
    QVERIFY(status);
    QCOMPARE(status->text(), savedStatus);
    if (locale == QStringLiteral("ko_KR"))
    {
        QVERIFY(application.removeTranslator(&translator));
    }
}

QTEST_MAIN(InitialSetupTeacherEntryBootstrapIntegrationTests)

#include "initial_setup_teacher_entry_bootstrap_integration_tests.moc"
