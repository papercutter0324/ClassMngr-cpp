#include "core/application_services.h"
#include "features/setup/ui/initial_setup_wizard.h"
#include "ui/shared/widgets/on_screen_keyboard.h"
#include "features/my_info/data/signature_image_processor.h"
#include "app/services/feature_services.h"

#include <QApplication>
#include <QBuffer>
#include <QImage>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTemporaryDir>
#include <QUuid>
#include <QtTest>

#include <utility>

namespace
{

constexpr auto SignatureImageKey = "myInfo/signatureImage";
constexpr auto DisplayNameKey = "myInfo/name";

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

QTEST_MAIN(InitialSetupWizardTests)

#include "initial_setup_wizard_tests.moc"
