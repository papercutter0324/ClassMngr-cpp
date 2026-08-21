#include "core/application_services.h"
#include "features/setup/ui/initial_setup_wizard.h"
#include "ui/shared/widgets/on_screen_keyboard.h"

#include <QApplication>
#include <QPushButton>
#include <QtTest>

#include <utility>

class InitialSetupWizardTests : public QObject
{
    Q_OBJECT

private slots:
    void keyboardIsAvailableAtTextEntryStages();
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

QTEST_MAIN(InitialSetupWizardTests)

#include "initial_setup_wizard_tests.moc"
