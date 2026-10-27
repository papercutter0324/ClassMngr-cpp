#include "data/database/workspace_store.h"
#include "features/my_info/data/signature_image_processor.h"
#include "features/my_info/data/typed_signature_renderer.h"
#include "next/application/personal_details_save_use_case.h"
#include "next/platform/workspace_store_gateway.h"
#include "next/platform/workspace_store_personal_details_port.h"

#include <QFile>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QUuid>
#include <QtTest/QtTest>

#include <cstddef>
#include <string>

using namespace ClassMngr::Next;

namespace
{

QString databasePath(QTemporaryDir& directory)
{
    return directory.filePath(
        QStringLiteral("initial-setup-personal-details-%1.tps").arg(
            QUuid::createUuid().toString(QUuid::WithoutBraces)
            )
        );
}

std::string utf8(const QString& value)
{
    const QByteArray bytes = value.toUtf8();
    return std::string(
        bytes.constData(),
        static_cast<std::size_t>(bytes.size())
        );
}

std::string bytes(const QByteArray& value)
{
    return value.isEmpty()
        ? std::string{}
        : std::string(
              value.constData(),
              static_cast<std::size_t>(value.size())
              );
}

bool applyFixture(WorkspaceStore& store)
{
    QFile fixture(QStringLiteral(CLASSMNGR_SOURCE_DIR)
                  + QStringLiteral(
                      "/tests/fixtures/workspaces/initial_setup_personal_details.sql"
                      ));
    if (!fixture.open(QIODevice::ReadOnly))
    {
        return false;
    }

    QSqlQuery seed(store.database());
    return seed.exec(QString::fromUtf8(fixture.readAll()));
}

bool executeSql(WorkspaceStore& store, const QString& statement)
{
    QSqlQuery query(store.database());
    return query.exec(statement);
}

Application::PersonalDetailsSaveRequest replacementDetails()
{
    using Application::CurrentCampusPreferenceValue;
    using Application::PersonalSignatureFontChoice;
    using Application::PersonalSignatureMode;
    using Domain::PersonalSignatureImageSaveBytes;

    const QByteArray renderedSignature = TypedSignature::renderForEmbedding(
        QString::fromUtf8("김선생님 / 🧭"),
        TypedSignatureFont::GreatVibes
        );
    return {
        .name = utf8(QString::fromUtf8("김선생님 / 🧭")),
        .campus = CurrentCampusPreferenceValue(
            utf8(QString::fromUtf8("성남 캠퍼스"))
            ),
        .zoomLoginId = utf8(QStringLiteral("teacher.next@example.test")),
        .zoomPassword = utf8(QStringLiteral("next-fixture-secret")),
        .zoomNotAvailable = false,
        .signatureImage = PersonalSignatureImageSaveBytes(
            bytes(renderedSignature)
            ),
        .signatureMode = PersonalSignatureMode::Type,
        .typedSignatureText = utf8(QString::fromUtf8("김선생님 / 🧭")),
        .typedSignatureFont = PersonalSignatureFontChoice::GreatVibes
    };
}

} // namespace

class NextPlatformWorkspaceStorePersonalDetailsPortTests final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void fixtureRoundTripRollsBackAndRetriesThroughTheTypedUseCase();
    void unavailableStoreReturnsRecoverableReadAndWriteFailures();
    void normalWorkspaceGatewayAllowsEmptyNameWhileSetupValidationRemainsStrict();
    void legacyZoomSettingsLoadAndMigrateIntoTheCurrentBundle();
    void missingValuesUseTheLegacyPersonalDetailsDefaults();

private:
    QTemporaryDir m_directory;
};

void NextPlatformWorkspaceStorePersonalDetailsPortTests::initTestCase()
{
    QVERIFY(m_directory.isValid());
}

void NextPlatformWorkspaceStorePersonalDetailsPortTests::
fixtureRoundTripRollsBackAndRetriesThroughTheTypedUseCase()
{
    const QString path = databasePath(m_directory);
    WorkspaceStore store;
    QVERIFY(store.open(path));
    QVERIFY(applyFixture(store));

    Platform::WorkspaceStorePersonalDetailsPort port(store);
    const Application::PersonalDetailsReadQuery read(port);
    const auto fixture = read.execute();
    QVERIFY(fixture);
    QCOMPARE(fixture.value().name, std::string("Fixture Teacher"));
    QCOMPARE(
        fixture.value().campus.value(),
        std::string("Fixture Campus")
        );
    QCOMPARE(
        fixture.value().zoomLoginId,
        std::string("fixture.zoom@example.test")
        );
    QCOMPARE(fixture.value().zoomPassword, std::string("fixture-only"));
    QVERIFY(!fixture.value().zoomNotAvailable);
    QVERIFY(fixture.value().signatureImage.empty());
    QCOMPARE(
        fixture.value().signatureMode,
        Application::PersonalSignatureMode::Image
        );
    QCOMPARE(fixture.value().typedSignatureFont,
             Application::PersonalSignatureFontChoice::JustAnotherHand);

    Application::PersonalDetailsSaveUseCase save(port);
    Application::PersonalDetailsSaveRequest invalid;
    const auto rejected = save.execute(invalid);
    QVERIFY(!rejected);
    QCOMPARE(rejected.error().code, Domain::ErrorCode::Validation);
    QVERIFY(rejected.error().recoverable);

    Application::PersonalDetailsSaveRequest replacement = replacementDetails();
    QVERIFY(!replacement.signatureImage.empty());

    QVERIFY(executeSql(
        store,
        QStringLiteral(R"(
            CREATE TRIGGER fail_initial_setup_personal_details_save
            BEFORE INSERT ON app_settings
            WHEN NEW.key = 'myInfo/typedSignatureText'
            BEGIN
                SELECT RAISE(ABORT, 'forced setup details failure');
            END
        )")
        ));
    const auto failed = save.execute(replacement);
    QVERIFY(!failed);
    QCOMPARE(failed.error().code, Domain::ErrorCode::Technical);
    QVERIFY(failed.error().recoverable);

    const auto afterFailure = read.execute();
    QVERIFY(afterFailure);
    QCOMPARE(afterFailure.value().name, std::string("Fixture Teacher"));
    QCOMPARE(afterFailure.value().campus.value(), std::string("Fixture Campus"));
    QCOMPARE(
        afterFailure.value().zoomLoginId,
        std::string("fixture.zoom@example.test")
        );

    QVERIFY(executeSql(
        store,
        QStringLiteral(
            "DROP TRIGGER fail_initial_setup_personal_details_save"
            )
        ));
    QVERIFY(save.execute(replacement));

    store.close();
    QVERIFY(!store.isOpen());

    WorkspaceStore reopened;
    QVERIFY(reopened.openExisting(path));
    Platform::WorkspaceStorePersonalDetailsPort reopenedPort(reopened);
    const Application::PersonalDetailsReadQuery reopenedRead(reopenedPort);
    const auto saved = reopenedRead.execute();
    QVERIFY(saved);

    QCOMPARE(saved.value().name, replacement.name);
    QCOMPARE(saved.value().campus, replacement.campus);
    QCOMPARE(saved.value().zoomLoginId, replacement.zoomLoginId);
    QCOMPARE(saved.value().zoomPassword, replacement.zoomPassword);
    QCOMPARE(saved.value().zoomNotAvailable, replacement.zoomNotAvailable);
    QCOMPARE(saved.value().signatureMode, replacement.signatureMode);
    QCOMPARE(saved.value().typedSignatureText, replacement.typedSignatureText);
    QCOMPARE(saved.value().typedSignatureFont, replacement.typedSignatureFont);
    QVERIFY(!saved.value().signatureImage.empty());
    QCOMPARE(
        saved.value().signatureImage.value(),
        bytes(SignatureImage::prepareForEmbedding(
            QByteArray(
                replacement.signatureImage.value().data(),
                static_cast<qsizetype>(replacement.signatureImage.value().size())
                )
            ))
        );

    SettingsRepository* const settings = reopened.settingsRepository();
    QVERIFY(settings);
    const auto storedName = settings->loadSetting(QStringLiteral("myInfo/name"));
    const auto storedMode = settings->loadSetting(
        QStringLiteral("myInfo/signatureMode")
        );
    const auto storedFont = settings->loadSetting(
        QStringLiteral("myInfo/typedSignatureFont")
        );
    const auto storedSignatureImage = settings->loadSetting(
        QStringLiteral("myInfo/signatureImage")
        );
    const auto unrelated = settings->loadSetting(
        QStringLiteral("fixture/unrelatedSetting")
        );
    QVERIFY(storedName);
    QVERIFY(storedMode);
    QVERIFY(storedFont);
    QVERIFY(storedSignatureImage);
    QVERIFY(unrelated);
    QCOMPARE(storedName->toString(), QString::fromUtf8("김선생님 / 🧭"));
    QCOMPARE(storedMode->toInt(), 1);
    QCOMPARE(storedFont->toInt(), 2);
    QCOMPARE(
        storedSignatureImage->toString(),
        QString::fromLatin1(
            SignatureImage::prepareForEmbedding(
                QByteArray(
                    replacement.signatureImage.value().data(),
                    static_cast<qsizetype>(
                        replacement.signatureImage.value().size()
                        )
                    )
                ).toBase64()
            )
        );
    QCOMPARE(unrelated->toString(), QStringLiteral("preserve-this-value"));
}

void NextPlatformWorkspaceStorePersonalDetailsPortTests::
unavailableStoreReturnsRecoverableReadAndWriteFailures()
{
    WorkspaceStore unopened;
    Platform::WorkspaceStorePersonalDetailsPort port(unopened);
    const Application::PersonalDetailsReadQuery read(port);
    const Application::PersonalDetailsSaveUseCase save(port);

    const auto loaded = read.execute();
    QVERIFY(!loaded);
    QVERIFY(loaded.error().recoverable);

    Application::PersonalDetailsSaveRequest request;
    request.name = "A Teacher";
    const auto saved = save.execute(request);
    QVERIFY(!saved);
    QVERIFY(saved.error().recoverable);
}

void NextPlatformWorkspaceStorePersonalDetailsPortTests::
normalWorkspaceGatewayAllowsEmptyNameWhileSetupValidationRemainsStrict()
{
    const QString path = databasePath(m_directory);
    {
        WorkspaceStore seed;
        QVERIFY(seed.open(path));
        QVERIFY(applyFixture(seed));
    }

    Platform::WorkspaceStoreGateway gateway;
    const auto opened = gateway.openWorkspace({
        Application::WorkspaceLocation(utf8(path))
    });
    QVERIFY(opened);

    const auto setupSave = gateway.savePersonalDetails({});
    QVERIFY(!setupSave);
    QCOMPARE(setupSave.error().code, Domain::ErrorCode::Validation);

    const auto normalSave = gateway.saveWorkspacePersonalDetails({});
    QVERIFY(normalSave);
    const auto readBack = gateway.readPersonalDetails();
    QVERIFY(readBack);
    QVERIFY(readBack.value().name.empty());

    QVERIFY(gateway.closeWorkspace({opened.value()}));
}

void NextPlatformWorkspaceStorePersonalDetailsPortTests::
legacyZoomSettingsLoadAndMigrateIntoTheCurrentBundle()
{
    const QString path = databasePath(m_directory);
    WorkspaceStore store;
    QVERIFY(store.open(path));
    QVERIFY(applyFixture(store));
    SettingsRepository* const settings = store.settingsRepository();
    QVERIFY(settings);

    QVERIFY(settings->saveSetting(
        QStringLiteral("subPrep/personalZoomEmail"),
        QStringLiteral("legacy.zoom@example.test")));
    QVERIFY(settings->saveSetting(
        QStringLiteral("subPrep/personalZoomPassword"),
        QStringLiteral("legacy-only-secret")));
    QVERIFY(settings->saveSetting(
        QStringLiteral("subPrep/personalZoomNotAvailable"), false));
    QVERIFY(executeSql(
        store,
        QStringLiteral(R"(
            DELETE FROM app_settings
            WHERE key IN (
                'myInfo/zoomLoginId',
                'myInfo/zoomPassword',
                'myInfo/zoomNotAvailable'
            )
        )")
        ));

    Platform::WorkspaceStorePersonalDetailsPort port(store);
    const Application::PersonalDetailsReadQuery read(port);
    const auto details = read.execute();
    QVERIFY(details);
    QCOMPARE(details.value().zoomLoginId,
             std::string("legacy.zoom@example.test"));
    QCOMPARE(details.value().zoomPassword,
             std::string("legacy-only-secret"));
    QVERIFY(!details.value().zoomNotAvailable);

    const auto migratedLogin = settings->loadSetting(
        QStringLiteral("myInfo/zoomLoginId"));
    const auto migratedPassword = settings->loadSetting(
        QStringLiteral("myInfo/zoomPassword"));
    const auto migratedAvailability = settings->loadSetting(
        QStringLiteral("myInfo/zoomNotAvailable"));
    QVERIFY(migratedLogin);
    QVERIFY(migratedPassword);
    QVERIFY(migratedAvailability);
    QCOMPARE(migratedLogin->toString(),
             QStringLiteral("legacy.zoom@example.test"));
    QCOMPARE(migratedPassword->toString(),
             QStringLiteral("legacy-only-secret"));
    QVERIFY(!migratedAvailability->toBool());
}

void NextPlatformWorkspaceStorePersonalDetailsPortTests::
missingValuesUseTheLegacyPersonalDetailsDefaults()
{
    WorkspaceStore store;
    QVERIFY(store.open(databasePath(m_directory)));
    Platform::WorkspaceStorePersonalDetailsPort port(store);
    const Application::PersonalDetailsReadQuery read(port);
    const auto details = read.execute();
    QVERIFY(details);
    QVERIFY(details.value().name.empty());
    QVERIFY(details.value().campus.value().empty());
    QCOMPARE(details.value().zoomLoginId, std::string("N/A"));
    QCOMPARE(details.value().zoomPassword, std::string("N/A"));
    QVERIFY(details.value().zoomNotAvailable);
    QVERIFY(details.value().signatureImage.empty());
    QCOMPARE(details.value().signatureMode,
             Application::PersonalSignatureMode::Image);
    QVERIFY(details.value().typedSignatureText.empty());
    QCOMPARE(details.value().typedSignatureFont,
             Application::PersonalSignatureFontChoice::JustAnotherHand);
}

QTEST_MAIN(NextPlatformWorkspaceStorePersonalDetailsPortTests)
#include "next_platform_workspace_store_personal_details_port_tests.moc"
