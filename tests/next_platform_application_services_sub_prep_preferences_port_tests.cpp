#include "core/application_services.h"
#include "data/data_service.h"
#include "data/database/database_session.h"
#include "data/repositories/settings_repository.h"
#include "next/application/sub_prep_preferences.h"
#include "next/platform/application_services_sub_prep_preferences_port.h"

#include <QRegularExpression>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QUuid>
#include <QtTest/QtTest>

#include <array>
#include <cstddef>
#include <optional>
#include <string>

using namespace ClassMngr::Next;
using namespace ClassMngr::Next::Application;
using namespace ClassMngr::Next::Platform;

namespace
{

constexpr auto ClassMaterialsKey = "subPrep/classMaterials";
constexpr auto BookReportGradingKey = "subPrep/bookReportGrading";
constexpr auto BookReportSpecialInstructionsKey =
    "subPrep/bookReportSpecialInstructions";
constexpr auto SubCommentsKey = "subPrep/subComments";
constexpr auto UnrelatedKey = "subPrep/unrelatedPreference";

QString databasePath(QTemporaryDir& directory)
{
    return directory.filePath(
        QStringLiteral("sub-prep-preferences-%1.tps").arg(
            QUuid::createUuid().toString(QUuid::WithoutBraces)
            )
        );
}

bool openDatabase(
    ApplicationServices& services,
    QTemporaryDir& directory
    )
{
    return services.openDatabase(databasePath(directory)).has_value();
}

SettingsRepository* settingsRepository(ApplicationServices& services)
{
    DatabaseSession* const session = services.databaseSession();
    return session && session->isOpen()
        ? session->settingsRepository()
        : nullptr;
}

bool executeSql(
    ApplicationServices& services,
    const QString& statement
    )
{
    DatabaseSession* const session = services.databaseSession();
    if (!session || !session->isOpen())
    {
        return false;
    }

    QSqlQuery query(session->database());
    return query.exec(statement);
}

std::string utf8(const QString& value)
{
    const QByteArray encoded = value.toUtf8();
    return std::string(
        encoded.constData(),
        static_cast<std::size_t>(encoded.size())
        );
}

} // namespace

class NextPlatformApplicationServicesSubPrepPreferencesPortTests final
    : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void missingValuesPreserveOptionalDefaults();
    void presentEmptyOptionalValuesRemainPresent();
    void exactKeysRoundTripAndPreserveUnrelatedSettings();
    void atomicSaveFailureRollsBackAndPreservesUnrelatedSettings();
    void unavailableSettingsFailWithoutWrites();
    void closedSessionFailsWithoutDataServiceFallback();
    void repositoryReadFailureWarnsAndUsesDefaults();

private:
    QTemporaryDir m_directory;
};

void NextPlatformApplicationServicesSubPrepPreferencesPortTests::
initTestCase()
{
    QVERIFY(m_directory.isValid());
}

void NextPlatformApplicationServicesSubPrepPreferencesPortTests::
missingValuesPreserveOptionalDefaults()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    SettingsRepository* const repository = settingsRepository(services);
    QVERIFY(repository);

    ApplicationServicesSubPrepPreferencesPort port(services);
    const auto loaded = port.load();

    DatabaseSession* const session = services.databaseSession();
    QVERIFY(session);
    QSqlQuery materializedRows(session->database());
    QVERIFY(materializedRows.prepare(QStringLiteral(R"(
        SELECT COUNT(*)
        FROM app_settings
        WHERE key IN (?, ?, ?, ?)
    )")));
    for (const char* key : {
             ClassMaterialsKey,
             BookReportGradingKey,
             BookReportSpecialInstructionsKey,
             SubCommentsKey
         })
    {
        materializedRows.addBindValue(QString::fromUtf8(key));
    }
    QVERIFY(materializedRows.exec());
    QVERIFY(materializedRows.next());
    QCOMPARE(materializedRows.value(0).toInt(), 0);

    QVERIFY(loaded);
    QCOMPARE(
        loaded.value(),
        (SubPrepPreferences{
            .classMaterials = {},
            .bookReportGrading = std::nullopt,
            .bookReportSpecialInstructions = std::nullopt,
            .subComments = {}
        })
        );

    const auto storedClassMaterials = repository->loadSetting(
        QString::fromUtf8(ClassMaterialsKey)
        );
    const auto storedBookReportGrading = repository->loadSetting(
        QString::fromUtf8(BookReportGradingKey)
        );
    const auto storedSpecialInstructions =
        repository->loadSetting(
            QString::fromUtf8(BookReportSpecialInstructionsKey)
            );
    const auto storedSubComments = repository->loadSetting(
        QString::fromUtf8(SubCommentsKey)
        );
    QVERIFY(storedClassMaterials.has_value());
    QVERIFY(!storedClassMaterials->isValid());
    QVERIFY(storedBookReportGrading.has_value());
    QVERIFY(!storedBookReportGrading->isValid());
    QVERIFY(storedSpecialInstructions.has_value());
    QVERIFY(!storedSpecialInstructions->isValid());
    QVERIFY(storedSubComments.has_value());
    QVERIFY(!storedSubComments->isValid());
}

void NextPlatformApplicationServicesSubPrepPreferencesPortTests::
presentEmptyOptionalValuesRemainPresent()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    SettingsRepository* const repository = settingsRepository(services);
    QVERIFY(repository);
    QVERIFY(repository->saveSetting(
        QString::fromUtf8(BookReportGradingKey),
        QString()
        ));
    QVERIFY(repository->saveSetting(
        QString::fromUtf8(BookReportSpecialInstructionsKey),
        QString()
        ));

    ApplicationServicesSubPrepPreferencesPort port(services);
    const auto loaded = port.load();

    QVERIFY(loaded);
    QVERIFY(loaded.value().bookReportGrading.has_value());
    QVERIFY(loaded.value().bookReportGrading->empty());
    QVERIFY(loaded.value().bookReportSpecialInstructions.has_value());
    QVERIFY(loaded.value().bookReportSpecialInstructions->empty());
}

void NextPlatformApplicationServicesSubPrepPreferencesPortTests::
exactKeysRoundTripAndPreserveUnrelatedSettings()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    SettingsRepository* const repository = settingsRepository(services);
    QVERIFY(repository);
    QVERIFY(repository->saveSetting(
        QString::fromUtf8(UnrelatedKey),
        QStringLiteral("preserved")
        ));

    const SubPrepPreferences expected{
        .classMaterials = utf8(QStringLiteral("  Materials / 김  ")),
        .bookReportGrading = utf8(QStringLiteral("  Grading / 評価  ")),
        .bookReportSpecialInstructions =
            utf8(QStringLiteral("  Bring spare books / 📚  ")),
        .subComments = utf8(QStringLiteral("  Notes / 메모  "))
    };

    ApplicationServicesSubPrepPreferencesPort port(services);
    QVERIFY(port.save(expected));

    const auto loaded = port.load();
    QVERIFY(loaded);
    QCOMPARE(loaded.value(), expected);

    const std::array<const char*, 4> keys = {{
        ClassMaterialsKey,
        BookReportGradingKey,
        BookReportSpecialInstructionsKey,
        SubCommentsKey
    }};
    for (const char* key : keys)
    {
        const auto stored = repository->loadSetting(
            QString::fromUtf8(key)
            );
        QVERIFY(stored);
    }

    QCOMPARE(
        repository->loadSetting(QString::fromUtf8(ClassMaterialsKey))
            ->toString(),
        QStringLiteral("  Materials / 김  ")
        );
    QCOMPARE(
        repository->loadSetting(QString::fromUtf8(BookReportGradingKey))
            ->toString(),
        QStringLiteral("  Grading / 評価  ")
        );
    QCOMPARE(
        repository->loadSetting(QString::fromUtf8(BookReportSpecialInstructionsKey))
            ->toString(),
        QStringLiteral("  Bring spare books / 📚  ")
        );
    QCOMPARE(
        repository->loadSetting(QString::fromUtf8(SubCommentsKey))
            ->toString(),
        QStringLiteral("  Notes / 메모  ")
        );
    QCOMPARE(
        repository->loadSetting(QString::fromUtf8(UnrelatedKey))
            ->toString(),
        QStringLiteral("preserved")
        );
}

void NextPlatformApplicationServicesSubPrepPreferencesPortTests::
atomicSaveFailureRollsBackAndPreservesUnrelatedSettings()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    SettingsRepository* const repository = settingsRepository(services);
    QVERIFY(repository);

    const SubPrepPreferences initial{
        .classMaterials = "initial materials",
        .bookReportGrading = std::string("initial grading"),
        .bookReportSpecialInstructions =
            std::string("initial special instructions"),
        .subComments = "initial comments"
    };
    const SubPrepPreferences replacement{
        .classMaterials = "replacement materials",
        .bookReportGrading = std::string("replacement grading"),
        .bookReportSpecialInstructions =
            std::string("replacement special instructions"),
        .subComments = "replacement comments"
    };

    ApplicationServicesSubPrepPreferencesPort port(services);
    QVERIFY(port.save(initial));
    QVERIFY(repository->saveSetting(
        QString::fromUtf8(UnrelatedKey),
        QStringLiteral("preserved")
        ));
    QVERIFY(executeSql(
        services,
        QStringLiteral(R"(
            CREATE TRIGGER fail_sub_prep_preferences
            BEFORE INSERT ON app_settings
            WHEN NEW.key = 'subPrep/bookReportSpecialInstructions'
            BEGIN
                SELECT RAISE(ABORT, 'forced Sub Prep preference failure');
            END
        )")
        ));

    const auto saved = port.save(replacement);
    QVERIFY(!saved);
    QCOMPARE(saved.error().code, Domain::ErrorCode::Technical);
    QVERIFY(!saved.error().recoverable);

    const auto loaded = port.load();
    QVERIFY(loaded);
    QCOMPARE(loaded.value(), initial);
    QCOMPARE(
        repository->loadSetting(QString::fromUtf8(UnrelatedKey))
            ->toString(),
        QStringLiteral("preserved")
        );
}

void NextPlatformApplicationServicesSubPrepPreferencesPortTests::
unavailableSettingsFailWithoutWrites()
{
    ApplicationServices services;
    ApplicationServicesSubPrepPreferencesPort port(services);

    const auto unavailableLoad = port.load();
    QVERIFY(!unavailableLoad);
    QCOMPARE(unavailableLoad.error().code, Domain::ErrorCode::Technical);
    QVERIFY(!unavailableLoad.error().recoverable);

    const auto unavailableSave = port.save({
        .classMaterials = "materials",
        .bookReportGrading = std::string("grading"),
        .bookReportSpecialInstructions = std::string("special"),
        .subComments = "comments"
    });
    QVERIFY(!unavailableSave);
    QCOMPARE(unavailableSave.error().code, Domain::ErrorCode::Technical);
    QVERIFY(!unavailableSave.error().recoverable);

    ApplicationServicesSubPrepPreferencesPort nullPort(
        static_cast<ApplicationServices*>(nullptr)
        );
    const auto nullLoad = nullPort.load();
    QVERIFY(!nullLoad);
    QCOMPARE(nullLoad.error().code, Domain::ErrorCode::Technical);
    QVERIFY(!nullLoad.error().recoverable);
    const auto nullSave = nullPort.save({});
    QVERIFY(!nullSave);
    QCOMPARE(nullSave.error().code, Domain::ErrorCode::Technical);
    QVERIFY(!nullSave.error().recoverable);
}

void NextPlatformApplicationServicesSubPrepPreferencesPortTests::
closedSessionFailsWithoutDataServiceFallback()
{
    ApplicationServices services;
    const QString path = databasePath(m_directory);
    QVERIFY(services.openDatabase(path));
    QVERIFY(services.dataService());
    DatabaseSession* const session = services.databaseSession();
    QVERIFY(session);
    SettingsRepository* repository = session->settingsRepository();
    QVERIFY(repository);

    const QString classMaterials = QStringLiteral("  saved materials  ");
    const QString bookReportGrading = QStringLiteral("  saved grading  ");
    const QString specialInstructions = QStringLiteral("  saved special  ");
    const QString subComments = QStringLiteral("  saved notes  ");
    const QString unrelated = QStringLiteral("preserved");
    const SubPrepPreferences initial{
        .classMaterials = utf8(classMaterials),
        .bookReportGrading = utf8(bookReportGrading),
        .bookReportSpecialInstructions = utf8(specialInstructions),
        .subComments = utf8(subComments)
    };
    const SubPrepPreferences replacement{
        .classMaterials = "replacement materials",
        .bookReportGrading = std::string("replacement grading"),
        .bookReportSpecialInstructions = std::string("replacement special"),
        .subComments = "replacement notes"
    };

    ApplicationServicesSubPrepPreferencesPort port(services);
    QVERIFY(port.save(initial));
    QVERIFY(repository->saveSetting(
        QString::fromUtf8(UnrelatedKey),
        unrelated
        ));

    services.closeDatabase();
    QVERIFY(services.dataService());
    QVERIFY(!session->isOpen());

    const auto loaded = port.load();
    QVERIFY(!loaded);
    QCOMPARE(loaded.error().code, Domain::ErrorCode::Technical);
    QVERIFY(!loaded.error().recoverable);
    const auto saved = port.save(replacement);
    QVERIFY(!saved);
    QCOMPARE(saved.error().code, Domain::ErrorCode::Technical);
    QVERIFY(!saved.error().recoverable);

    QVERIFY(services.openDatabase(path));
    QVERIFY(session->isOpen());
    repository = session->settingsRepository();
    QVERIFY(repository);

    const auto storedClassMaterials = repository->loadSetting(
        QString::fromUtf8(ClassMaterialsKey)
        );
    QVERIFY(storedClassMaterials);
    QCOMPARE(storedClassMaterials->toString(), classMaterials);
    const auto storedBookReportGrading = repository->loadSetting(
        QString::fromUtf8(BookReportGradingKey)
        );
    QVERIFY(storedBookReportGrading);
    QCOMPARE(storedBookReportGrading->toString(), bookReportGrading);
    const auto storedSpecialInstructions = repository->loadSetting(
        QString::fromUtf8(BookReportSpecialInstructionsKey)
        );
    QVERIFY(storedSpecialInstructions);
    QCOMPARE(storedSpecialInstructions->toString(), specialInstructions);
    const auto storedSubComments = repository->loadSetting(
        QString::fromUtf8(SubCommentsKey)
        );
    QVERIFY(storedSubComments);
    QCOMPARE(storedSubComments->toString(), subComments);
    const auto storedUnrelated = repository->loadSetting(
        QString::fromUtf8(UnrelatedKey)
        );
    QVERIFY(storedUnrelated);
    QCOMPARE(storedUnrelated->toString(), unrelated);
}

void NextPlatformApplicationServicesSubPrepPreferencesPortTests::
repositoryReadFailureWarnsAndUsesDefaults()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    QVERIFY(executeSql(services, QStringLiteral("DROP TABLE app_settings")));

    ApplicationServicesSubPrepPreferencesPort port(services);
    for (int index = 0; index < 4; ++index)
    {
        QTest::ignoreMessage(
            QtWarningMsg,
            QRegularExpression(QStringLiteral("Failed to load setting.*"))
            );
    }

    const auto loaded = port.load();
    QVERIFY(loaded);
    QCOMPARE(
        loaded.value(),
        (SubPrepPreferences{
            .classMaterials = {},
            .bookReportGrading = std::nullopt,
            .bookReportSpecialInstructions = std::nullopt,
            .subComments = {}
        })
        );
}

QTEST_MAIN(NextPlatformApplicationServicesSubPrepPreferencesPortTests)

#include "next_platform_application_services_sub_prep_preferences_port_tests.moc"
