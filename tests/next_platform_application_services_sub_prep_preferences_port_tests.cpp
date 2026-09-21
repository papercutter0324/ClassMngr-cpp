#include "core/application_services.h"
#include "data/data_service.h"
#include "data/database/database_session.h"
#include "next/application/sub_prep_preferences.h"
#include "next/platform/application_services_sub_prep_preferences_port.h"

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

bool executeSql(
    ApplicationServices& services,
    const QString& statement
    )
{
    DataService* dataService = services.dataService();
    if (!dataService || !dataService->databaseSession())
    {
        return false;
    }

    QSqlQuery query(dataService->databaseSession()->database());
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

    ApplicationServicesSubPrepPreferencesPort port(services);
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

    QVERIFY(services.dataService());
    const auto storedClassMaterials = services.dataService()->loadSetting(
        QString::fromUtf8(ClassMaterialsKey)
        );
    const auto storedBookReportGrading = services.dataService()->loadSetting(
        QString::fromUtf8(BookReportGradingKey)
        );
    const auto storedSpecialInstructions =
        services.dataService()->loadSetting(
            QString::fromUtf8(BookReportSpecialInstructionsKey)
            );
    const auto storedSubComments = services.dataService()->loadSetting(
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
    QVERIFY(services.dataService());
    QVERIFY(services.dataService()->saveSetting(
        QString::fromUtf8(BookReportGradingKey),
        QString()
        ));
    QVERIFY(services.dataService()->saveSetting(
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
    QVERIFY(services.dataService());
    QVERIFY(services.dataService()->saveSetting(
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
        const auto stored = services.dataService()->loadSetting(
            QString::fromUtf8(key)
            );
        QVERIFY(stored);
    }

    QCOMPARE(
        services.dataService()
            ->loadSetting(QString::fromUtf8(ClassMaterialsKey))
            ->toString(),
        QStringLiteral("  Materials / 김  ")
        );
    QCOMPARE(
        services.dataService()
            ->loadSetting(QString::fromUtf8(BookReportGradingKey))
            ->toString(),
        QStringLiteral("  Grading / 評価  ")
        );
    QCOMPARE(
        services.dataService()
            ->loadSetting(QString::fromUtf8(BookReportSpecialInstructionsKey))
            ->toString(),
        QStringLiteral("  Bring spare books / 📚  ")
        );
    QCOMPARE(
        services.dataService()
            ->loadSetting(QString::fromUtf8(SubCommentsKey))
            ->toString(),
        QStringLiteral("  Notes / 메모  ")
        );
    QCOMPARE(
        services.dataService()
            ->loadSetting(QString::fromUtf8(UnrelatedKey))
            ->toString(),
        QStringLiteral("preserved")
        );
}

void NextPlatformApplicationServicesSubPrepPreferencesPortTests::
atomicSaveFailureRollsBackAndPreservesUnrelatedSettings()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    QVERIFY(services.dataService());

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
    QVERIFY(services.dataService()->saveSetting(
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

    const auto loaded = port.load();
    QVERIFY(loaded);
    QCOMPARE(loaded.value(), initial);
    QCOMPARE(
        services.dataService()
            ->loadSetting(QString::fromUtf8(UnrelatedKey))
            ->toString(),
        QStringLiteral("preserved")
        );
}

void NextPlatformApplicationServicesSubPrepPreferencesPortTests::
unavailableSettingsFailWithoutWrites()
{
    ApplicationServices services;
    ApplicationServicesSubPrepPreferencesPort port(services);

    QVERIFY(!port.load());
    QVERIFY(!port.save({
        .classMaterials = "materials",
        .bookReportGrading = std::string("grading"),
        .bookReportSpecialInstructions = std::string("special"),
        .subComments = "comments"
    }));

    ApplicationServicesSubPrepPreferencesPort nullPort(nullptr);
    QVERIFY(!nullPort.load());
    QVERIFY(!nullPort.save({}));
}

QTEST_MAIN(NextPlatformApplicationServicesSubPrepPreferencesPortTests)

#include "next_platform_application_services_sub_prep_preferences_port_tests.moc"
