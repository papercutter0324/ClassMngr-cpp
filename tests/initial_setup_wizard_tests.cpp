#include "core/application_services.h"
#include "features/setup/ui/initial_setup_wizard.h"
#include "ui/shared/widgets/on_screen_keyboard.h"

#include <QApplication>
#include <QLineEdit>
#include <QPushButton>
#include <QtTest>

#include <utility>

class InitialSetupWizardTests : public QObject
{
    Q_OBJECT

private slots:
    void keyboardIsAvailableAtTextEntryStages();
    void personalNameFieldRetainsFocusWhileTyping();
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

    for (const auto key : {Qt::Key_A, Qt::Key_l, Qt::Key_e, Qt::Key_x})
    {
        QTest::keyClick(name, key);
        QVERIFY(name->hasFocus());
    }

    QCOMPARE(name->text(), QStringLiteral("Alex"));
}

QTEST_MAIN(InitialSetupWizardTests)

#include "initial_setup_wizard_tests.moc"
