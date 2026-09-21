#include "core/application_services.h"
#include "data/data_service.h"
#include "data/database/database_session.h"
#include "features/setup/ui/initial_setup_wizard.h"
#include "fakes/fake_user_prompt_service.h"
#include "ui/shared/widgets/on_screen_keyboard.h"
#include "features/my_info/data/signature_image_processor.h"
#include "app/services/feature_services.h"
#include "ui/shared/dialogs/user_prompt_service.h"

#include <QApplication>
#include <QBuffer>
#include <QImage>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QUuid>
#include <QtTest>

#include <utility>

namespace
{

constexpr auto SignatureImageKey = "myInfo/signatureImage";
constexpr auto DisplayNameKey = "myInfo/name";
constexpr auto CampusKey = "myInfo/campus";
constexpr auto ZoomLoginIdKey = "myInfo/zoomLoginId";
constexpr auto ZoomPasswordKey = "myInfo/zoomPassword";
constexpr auto ZoomNotAvailableKey = "myInfo/zoomNotAvailable";
constexpr auto SignatureModeKey = "myInfo/signatureMode";
constexpr auto TypedSignatureTextKey = "myInfo/typedSignatureText";
constexpr auto TypedSignatureFontKey = "myInfo/typedSignatureFont";
constexpr auto UnrelatedKey = "myInfo/unrelatedPreference";

QString databasePath(QTemporaryDir& directory)
{
    return directory.filePath(
        QStringLiteral("initial-setup-wizard-%1.tps").arg(
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

QByteArray sourcePng()
{
    QImage source(2, 1, QImage::Format_RGB32);
    source.setPixelColor(0, 0, Qt::white);
    source.setPixelColor(1, 0, Qt::black);

    QByteArray encoded;
    QBuffer buffer(&encoded);
    if (!buffer.open(QIODevice::WriteOnly) || !source.save(&buffer, "PNG"))
    {
        return {};
    }

    return encoded;
}

QLabel* signaturePreview(InitialSetupWizard& wizard)
{
    return wizard.findChild<QLabel*>(
        QStringLiteral("signatureImagePreview"));
}

void showPersonalDetailsPage(InitialSetupWizard& wizard)
{
    wizard.setStartId(InitialSetupWizard::PersonalDetailsPage);
    wizard.show();
    QApplication::processEvents();
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

} // namespace

class InitialSetupWizardTests : public QObject
{
    Q_OBJECT

private slots:
    void keyboardIsAvailableAtTextEntryStages();
    void personalNameFieldRetainsFocusWhileTyping();
    void storedSignatureImagePreviewsAndSurvivesSaveWithoutReplacement();
    void missingInvalidAndUnavailableSignatureImagesStayEmpty();
    void storedDisplayNamePreservesUtf8AndWhitespace();
    void missingAndUnavailableDisplayNameStayEmpty();
    void aggregateSavePreservesAllPersonalDetailsWithoutDataLoss();
    void aggregateSaveFailureLeavesAllPersonalDetailsUnchanged();

private:
    QTemporaryDir m_directory;
};

void InitialSetupWizardTests::keyboardIsAvailableAtTextEntryStages()
{
    ApplicationServices services;

    for (const auto [pageId, title] : {
             std::pair{
                 InitialSetupWizard::PersonalDetailsPage,
                 QStringLiteral("Add Your Information")},
             std::pair{
                 InitialSetupWizard::TeacherEntryPage,
                 QStringLiteral("Add Korean Teacher(s)")}})
    {
        InitialSetupWizard wizard(&services);
        wizard.setStartId(pageId);
        wizard.show();
        QApplication::processEvents();

        QCOMPARE(wizard.currentId(), pageId);
        QCOMPARE(wizard.currentPage()->title(), title);

        auto* trigger = wizard.findChild<QPushButton*>(
            QStringLiteral("initialSetupKoreanKeyboardButton"));
        auto* keyboard = wizard.findChild<OnScreenKeyboard*>();
        QVERIFY(trigger);
        QVERIFY(keyboard);
        QVERIFY(trigger->isVisible());
        QVERIFY(!trigger->icon().isNull());
        QCOMPARE(trigger->accessibleName(), QStringLiteral("Korean Keyboard"));
        QVERIFY(trigger->geometry().center().x() > wizard.width() * 3 / 4);
        QVERIFY(trigger->geometry().top() < wizard.height() / 4);
        QCOMPARE(
            wizard.contentsRect().right() - trigger->geometry().right(), 20);

        trigger->click();
        QApplication::processEvents();
        QVERIFY(keyboard->isVisible());
        keyboard->close();
    }
}

void InitialSetupWizardTests::personalNameFieldRetainsFocusWhileTyping()
{
    ApplicationServices services;
    InitialSetupWizard wizard(&services);
    wizard.setStartId(InitialSetupWizard::PersonalDetailsPage);
    wizard.show();
    QApplication::processEvents();

    auto* name = wizard.findChild<QLineEdit*>(
        QStringLiteral("setupUserName"));
    QVERIFY(name);

    name->setFocus();
    QTRY_VERIFY(name->hasFocus());

    for (const auto key : {Qt::Key_A, Qt::Key_L, Qt::Key_E, Qt::Key_X})
    {
        QTest::keyClick(name, key);
        QVERIFY(name->hasFocus());
    }

    QCOMPARE(name->text(), QStringLiteral("alex"));
}

void InitialSetupWizardTests::
storedSignatureImagePreviewsAndSurvivesSaveWithoutReplacement()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    QVERIFY(services.settingsService());

    const QByteArray source = sourcePng();
    QVERIFY(!source.isEmpty());
    QVERIFY(
        services.settingsService()->save(
            QString::fromUtf8(SignatureImageKey),
            QString::fromLatin1(source.toBase64())
            )
        );

    InitialSetupWizard wizard(&services);
    showPersonalDetailsPage(wizard);

    auto* preview = signaturePreview(wizard);
    QVERIFY(preview);
    QVERIFY(preview->text().isEmpty());

    auto* name = wizard.findChild<QLineEdit*>(
        QStringLiteral("setupUserName"));
    QVERIFY(name);
    name->setText(QStringLiteral("Stored Teacher"));

    auto* next = wizard.button(QWizard::NextButton);
    QVERIFY(next);
    next->click();
    QApplication::processEvents();

    const QByteArray expected =
        SignatureImage::prepareForEmbedding(source);
    const QByteArray stored = services.settingsService()->loadOrDefault(
        QString::fromUtf8(SignatureImageKey),
        QVariant()
        ).toString().toLatin1();
    QCOMPARE(stored, expected.toBase64());
}

void InitialSetupWizardTests::missingInvalidAndUnavailableSignatureImagesStayEmpty()
{
    {
        ApplicationServices services;
        QVERIFY(openDatabase(services, m_directory));

        InitialSetupWizard wizard(&services);
        showPersonalDetailsPage(wizard);

        auto* preview = signaturePreview(wizard);
        QVERIFY(preview);
        QVERIFY(!preview->text().isEmpty());
    }

    {
        ApplicationServices services;
        QVERIFY(openDatabase(services, m_directory));
        QVERIFY(services.settingsService());
        QVERIFY(
            services.settingsService()->save(
                QString::fromUtf8(SignatureImageKey),
                QStringLiteral("%%%not-base64%%")
                )
            );

        InitialSetupWizard wizard(&services);
        showPersonalDetailsPage(wizard);

        auto* preview = signaturePreview(wizard);
        QVERIFY(preview);
        QVERIFY(!preview->text().isEmpty());
    }

    {
        ApplicationServices services;
        InitialSetupWizard wizard(&services);
        showPersonalDetailsPage(wizard);

        auto* preview = signaturePreview(wizard);
        QVERIFY(preview);
        QVERIFY(!preview->text().isEmpty());
    }
}

void InitialSetupWizardTests::storedDisplayNamePreservesUtf8AndWhitespace()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    QVERIFY(services.settingsService());

    const QString storedName = QStringLiteral("  홍길동  ");
    QVERIFY(
        services.settingsService()->save(
            QString::fromUtf8(DisplayNameKey),
            storedName
            )
        );

    InitialSetupWizard wizard(&services);
    showPersonalDetailsPage(wizard);

    auto* name = wizard.findChild<QLineEdit*>(
        QStringLiteral("setupUserName"));
    QVERIFY(name);
    QCOMPARE(name->text(), storedName);
}

void InitialSetupWizardTests::missingAndUnavailableDisplayNameStayEmpty()
{
    {
        ApplicationServices services;
        QVERIFY(openDatabase(services, m_directory));

        InitialSetupWizard wizard(&services);
        showPersonalDetailsPage(wizard);

        auto* name = wizard.findChild<QLineEdit*>(
            QStringLiteral("setupUserName"));
        QVERIFY(name);
        QVERIFY(name->text().isEmpty());
    }

    {
        ApplicationServices services;
        InitialSetupWizard wizard(&services);
        showPersonalDetailsPage(wizard);

        auto* name = wizard.findChild<QLineEdit*>(
            QStringLiteral("setupUserName"));
        QVERIFY(name);
        QVERIFY(name->text().isEmpty());
    }
}

void InitialSetupWizardTests::
aggregateSavePreservesAllPersonalDetailsWithoutDataLoss()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    QVERIFY(services.settingsService());

    const QByteArray source = sourcePng();
    QVERIFY(!source.isEmpty());
    const QString typedText = QString::fromUtf8(
        "  \xEA\xB9\x80\xEC\x84\xA0\xEC\x83\x9D\xEB\x8B\x98 / "
        "\xF0\x9F\xA7\xAD  "
        );
    const QVariantMap initialValues = {
        {QString::fromUtf8(DisplayNameKey), QStringLiteral("Old Name")},
        {QString::fromUtf8(CampusKey), QStringLiteral("Jeongja")},
        {QString::fromUtf8(ZoomLoginIdKey), QStringLiteral("old-login")},
        {QString::fromUtf8(ZoomPasswordKey), QStringLiteral("old-password")},
        {QString::fromUtf8(ZoomNotAvailableKey), false},
        {
            QString::fromUtf8(SignatureImageKey),
            QString::fromLatin1(source.toBase64())
        },
        {QString::fromUtf8(SignatureModeKey), 1},
        {QString::fromUtf8(TypedSignatureTextKey), typedText},
        {QString::fromUtf8(TypedSignatureFontKey), 2},
        {QString::fromUtf8(UnrelatedKey), QStringLiteral("preserved")}
    };
    QVERIFY(services.settingsService()->saveAll(initialValues));

    InitialSetupWizard wizard(&services);
    showPersonalDetailsPage(wizard);

    auto* name = wizard.findChild<QLineEdit*>(
        QStringLiteral("setupUserName"));
    QVERIFY(name);
    name->setText(QStringLiteral("  New Teacher  "));

    auto* next = wizard.button(QWizard::NextButton);
    QVERIFY(next);
    next->click();
    QApplication::processEvents();

    QCOMPARE(
        services.settingsService()->loadOrDefault(
            QString::fromUtf8(DisplayNameKey), QVariant()
            ).toString(),
        QStringLiteral("New Teacher")
        );
    QCOMPARE(
        services.settingsService()->loadOrDefault(
            QString::fromUtf8(CampusKey), QVariant()
            ).toString(),
        initialValues.value(QString::fromUtf8(CampusKey)).toString()
        );
    QCOMPARE(
        services.settingsService()->loadOrDefault(
            QString::fromUtf8(ZoomLoginIdKey), QVariant()
            ).toString(),
        initialValues.value(QString::fromUtf8(ZoomLoginIdKey)).toString()
        );
    QCOMPARE(
        services.settingsService()->loadOrDefault(
            QString::fromUtf8(ZoomPasswordKey), QVariant()
            ).toString(),
        initialValues.value(QString::fromUtf8(ZoomPasswordKey)).toString()
        );
    QCOMPARE(
        services.settingsService()->loadOrDefault(
            QString::fromUtf8(ZoomNotAvailableKey), QVariant()
            ).toBool(),
        false
        );
    QCOMPARE(
        services.settingsService()->loadOrDefault(
            QString::fromUtf8(SignatureImageKey), QVariant()
            ).toString(),
        QString::fromLatin1(
            SignatureImage::prepareForEmbedding(source).toBase64()
            )
        );
    QCOMPARE(
        services.settingsService()->loadOrDefault(
            QString::fromUtf8(SignatureModeKey), QVariant()
            ).toInt(),
        1
        );
    QCOMPARE(
        services.settingsService()->loadOrDefault(
            QString::fromUtf8(TypedSignatureTextKey), QVariant()
            ).toString(),
        typedText
        );
    QCOMPARE(
        services.settingsService()->loadOrDefault(
            QString::fromUtf8(TypedSignatureFontKey), QVariant()
            ).toInt(),
        2
        );
    QCOMPARE(
        services.settingsService()->loadOrDefault(
            QString::fromUtf8(UnrelatedKey), QVariant()
            ).toString(),
        QStringLiteral("preserved")
        );
}

void InitialSetupWizardTests::
aggregateSaveFailureLeavesAllPersonalDetailsUnchanged()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    QVERIFY(services.settingsService());

    const QVariantMap initialValues = {
        {QString::fromUtf8(DisplayNameKey), QStringLiteral("Old Name")},
        {QString::fromUtf8(CampusKey), QStringLiteral("Jeongja")},
        {QString::fromUtf8(ZoomLoginIdKey), QStringLiteral("old-login")},
        {QString::fromUtf8(ZoomPasswordKey), QStringLiteral("old-password")},
        {QString::fromUtf8(ZoomNotAvailableKey), true},
        {QString::fromUtf8(SignatureImageKey), QStringLiteral("old-image")},
        {QString::fromUtf8(SignatureModeKey), 0},
        {QString::fromUtf8(TypedSignatureTextKey), QStringLiteral("old text")},
        {QString::fromUtf8(TypedSignatureFontKey), 1},
        {QString::fromUtf8(UnrelatedKey), QStringLiteral("preserved")}
    };
    QVERIFY(services.settingsService()->saveAll(initialValues));
    QVERIFY(executeSql(
        services,
        QStringLiteral(R"(
            CREATE TRIGGER fail_initial_setup_personal_details_save
            BEFORE INSERT ON app_settings
            WHEN NEW.key = 'myInfo/typedSignatureFont'
            BEGIN
                SELECT RAISE(ABORT, 'forced initial setup save failure');
            END
        )")
        ));

    FakeUserPromptService prompts;
    DialogServices::setUserPromptServiceForTesting(&prompts);

    InitialSetupWizard wizard(&services);
    showPersonalDetailsPage(wizard);
    auto* name = wizard.findChild<QLineEdit*>(
        QStringLiteral("setupUserName"));
    QVERIFY(name);
    name->setText(QStringLiteral("New Teacher"));

    auto* next = wizard.button(QWizard::NextButton);
    QVERIFY(next);
    next->click();
    QApplication::processEvents();

    QCOMPARE(prompts.messages.size(), 1);
    QCOMPARE(prompts.messages.constFirst().title, QStringLiteral("Initial Setup"));
    QCOMPARE(
        prompts.messages.constFirst().message,
        QStringLiteral("Your personal information could not be saved.")
        );

    for (auto setting = initialValues.cbegin();
         setting != initialValues.cend();
         ++setting)
    {
        QCOMPARE(
            services.settingsService()->loadOrDefault(
                setting.key(), QVariant()
                ),
            setting.value()
            );
    }

    DialogServices::setUserPromptServiceForTesting(nullptr);
}

QTEST_MAIN(InitialSetupWizardTests)

#include "initial_setup_wizard_tests.moc"
