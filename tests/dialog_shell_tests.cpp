#include "core/settingsmanager.h"
#include "features/calendar/ui/calendar_event_dialog.h"
#include "features/roster/ui/roster_print_dialog.h"
#include "features/schedule/ui/schedule_editor_dialog.h"
#include "features/schedule/ui/schedule_import_dialog.h"
#include "features/schedule/ui/schedule_import_review_dialog.h"
#include "features/schedule/ui/schedule_print_dialog.h"
#include "features/schedule/ui/testing_assignment_dialog.h"
#include "features/speaking_eval/ui/speaking_eval_ai_batch_dialog.h"
#include "features/speaking_eval/ui/speaking_eval_batch_export_dialog.h"
#include "features/speaking_eval/ui/speaking_eval_notes_dialog.h"
#include "features/speaking_eval/ui/speaking_eval_report_dialog.h"
#include "features/sub_prep/ui/sub_prep_print_dialog.h"
#include "ui/shared/dialogs/dialog_shell.h"
#include "ui/shared/dialogs/update_dialog.h"
#include "ui/shared/printing/pdf_print_dialog.h"
#include "ui/shared/widgets/text_fit_dialog_button_box.h"
#include "ui/shared/widgets/on_screen_keyboard.h"

#include <QDialogButtonBox>
#include <QApplication>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QTranslator>
#include <QVBoxLayout>

#include <type_traits>

static_assert(std::is_base_of_v<DialogShell, CalendarEventDialog>);
static_assert(std::is_base_of_v<DialogShell, ScheduleEditorDialog>);
static_assert(std::is_base_of_v<DialogShell, ScheduleImportDialog>);
static_assert(std::is_base_of_v<DialogShell, ScheduleImportReviewDialog>);
static_assert(std::is_base_of_v<DialogShell, SchedulePrintDialog>);
static_assert(std::is_base_of_v<DialogShell, TestingAssignmentDialog>);
static_assert(std::is_base_of_v<DialogShell, RosterPrintDialog>);
static_assert(std::is_base_of_v<DialogShell, SubPrepPrintDialog>);
static_assert(std::is_base_of_v<DialogShell, PdfPrintDialog>);
static_assert(std::is_base_of_v<DialogShell, UpdateDialog>);
static_assert(std::is_base_of_v<DialogShell, SpeakingEvalAiBatchDialog>);
static_assert(std::is_base_of_v<DialogShell, SpeakingEvalBatchExportDialog>);
static_assert(std::is_base_of_v<DialogShell, SpeakingEvalNotesDialog>);
static_assert(std::is_base_of_v<DialogShell, SpeakingEvalReportDialog>);

class AcceptTrackingDialogShell final : public DialogShell
{
public:
    using DialogShell::DialogShell;

    void accept() override
    {
        acceptInvoked = true;
    }

    bool acceptInvoked = false;
};

class RetranslationTrackingDialogShell final : public DialogShell
{
public:
    using DialogShell::DialogShell;

    int retranslationCount = 0;

protected:
    void retranslateDialog() override
    {
        ++retranslationCount;
        setWindowTitle(QStringLiteral("Translated policy title"));
        setHeader(
            QStringLiteral("Translated heading"),
            QStringLiteral("Translated supporting text")
            );
    }
};

class DialogPolicyTranslator final : public QTranslator
{
public:
    QString translate(
        const char* context,
        const char* sourceText,
        const char* disambiguation = nullptr,
        int number = -1
        ) const override
    {
        Q_UNUSED(disambiguation);
        Q_UNUSED(number);

        if (qstrcmp(context, "DialogShell") != 0)
        {
            return {};
        }
        if (qstrcmp(sourceText, "Dialog header") == 0)
        {
            return QStringLiteral("Translated dialog header");
        }
        if (qstrcmp(sourceText, "Dialog actions") == 0)
        {
            return QStringLiteral("Translated dialog actions");
        }
        return {};
    }
};

class DialogShellTests : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void appliesSharedLayoutHeaderAndAccessibilityPolicy();
    void standardButtonsProvideDefaultAndEscapeBehavior();
    void buttonRolesPreserveDialogValidation();
    void retainsParentOwnershipAndModalContract();
    void languageChangeRetranslatesShellAndDialog();
    void persistsGeometryByStableDialogKey();
    void calendarEventDialogsExposeUntargetedKeyboard();
    void calendarEventDialogShowsInlineValidation();

private:
    QTemporaryDir m_settingsRoot;
};

void DialogShellTests::initTestCase()
{
    QVERIFY(m_settingsRoot.isValid());
    qputenv(
        "CLASSMNGR_SETTINGS_ROOT",
        m_settingsRoot.path().toUtf8()
        );
    SettingsManager::instance().clear();
}

void DialogShellTests::appliesSharedLayoutHeaderAndAccessibilityPolicy()
{
    DialogShell dialog(QStringLiteral("test key"));
    dialog.setWindowTitle(QStringLiteral("Policy Test"));
    dialog.setHeader(
        QStringLiteral("Heading"),
        QStringLiteral("Supporting text")
        );
    dialog.show();

    QCOMPARE(dialog.dialogKey(), QStringLiteral("test_key"));
    QCOMPARE(dialog.objectName(), QStringLiteral("test_keyDialog"));
    QCOMPARE(dialog.accessibleName(), QStringLiteral("Policy Test"));
    QCOMPARE(dialog.contentLayout()->contentsMargins(), QMargins(18, 18, 18, 18));
    QCOMPARE(dialog.contentLayout()->spacing(), 10);

    auto* heading = dialog.findChild<QLabel*>(
        QStringLiteral("test_keyHeaderTitle")
        );
    auto* subtitle = dialog.findChild<QLabel*>(
        QStringLiteral("test_keyHeaderSubtitle")
        );
    QVERIFY(heading);
    QVERIFY(subtitle);
    QCOMPARE(heading->text(), QStringLiteral("Heading"));
    QCOMPARE(subtitle->text(), QStringLiteral("Supporting text"));
}

void DialogShellTests::standardButtonsProvideDefaultAndEscapeBehavior()
{
    DialogShell dialog(QStringLiteral("buttonPolicy"));
    auto* buttons = dialog.addButtonBox(
        QDialogButtonBox::Save | QDialogButtonBox::Cancel
        );
    auto* saveButton = buttons->button(QDialogButtonBox::Save);
    QVERIFY(saveButton);
    QVERIFY(saveButton->isDefault());
    QCOMPARE(
        buttons->objectName(),
        QStringLiteral("buttonPolicyButtonBox")
        );

    QSignalSpy rejectedSpy(&dialog, &QDialog::rejected);
    dialog.show();
    QTest::keyClick(&dialog, Qt::Key_Escape);
    QCOMPARE(rejectedSpy.count(), 1);
}

void DialogShellTests::buttonRolesPreserveDialogValidation()
{
    AcceptTrackingDialogShell validatingDialog(
        QStringLiteral("validationPolicy")
        );
    auto* standardButtons = validatingDialog.addButtonBox(
        QDialogButtonBox::Save | QDialogButtonBox::Cancel
        );
    standardButtons->button(QDialogButtonBox::Save)->click();
    QVERIFY(validatingDialog.acceptInvoked);

    DialogShell workflowDialog(QStringLiteral("workflowPolicy"));
    auto* workflowButtons = workflowDialog.addButtonBox(
        QDialogButtonBox::Cancel
        );
    auto* applyButton = workflowButtons->addButton(
        QStringLiteral("Apply"),
        QDialogButtonBox::ActionRole
        );
    QSignalSpy acceptedSpy(&workflowDialog, &QDialog::accepted);
    applyButton->click();
    QCOMPARE(acceptedSpy.count(), 0);
}

void DialogShellTests::retainsParentOwnershipAndModalContract()
{
    QWidget owner;
    DialogShell dialog(QStringLiteral("ownedDialog"), &owner);

    QCOMPARE(dialog.parentWidget(), &owner);
    QVERIFY(dialog.isModal());
}

void DialogShellTests::languageChangeRetranslatesShellAndDialog()
{
    DialogPolicyTranslator translator;
    QVERIFY(QCoreApplication::installTranslator(&translator));

    RetranslationTrackingDialogShell dialog(
        QStringLiteral("retranslationPolicy")
        );
    dialog.setWindowTitle(QStringLiteral("Original policy title"));
    dialog.setHeader(
        QStringLiteral("Original heading"),
        QStringLiteral("Original supporting text")
        );
    auto* buttons = dialog.addButtonBox(QDialogButtonBox::Ok);

    QEvent languageChange(QEvent::LanguageChange);
    QVERIFY(QCoreApplication::sendEvent(&dialog, &languageChange));

    QCOMPARE(dialog.retranslationCount, 1);
    QCOMPARE(dialog.windowTitle(), QStringLiteral("Translated policy title"));
    QCOMPARE(dialog.accessibleName(), QStringLiteral("Translated policy title"));
    auto* header = dialog.findChild<QWidget*>(
        QStringLiteral("retranslationPolicyHeader")
        );
    QVERIFY(header);
    QCOMPARE(
        header->accessibleName(),
        QStringLiteral("Translated dialog header")
        );
    QCOMPARE(
        buttons->accessibleName(),
        QStringLiteral("Translated dialog actions")
        );

    QVERIFY(QCoreApplication::removeTranslator(&translator));
}

void DialogShellTests::persistsGeometryByStableDialogKey()
{
    QSize savedSize;
    {
        DialogShell dialog(QStringLiteral("geometryPolicy"));
        dialog.resize(512, 376);
        dialog.show();
        QCoreApplication::processEvents();
        savedSize = dialog.size();
        dialog.close();
    }

    QVERIFY(
        !SettingsManager::instance()
             .get(QStringLiteral("ui/dialogs/geometryPolicy/geometry"))
             .toByteArray()
             .isEmpty()
        );

    DialogShell restored(QStringLiteral("geometryPolicy"));
    restored.resize(300, 200);
    restored.show();
    QCoreApplication::processEvents();
    QCOMPARE(restored.size(), savedSize);
}

void DialogShellTests::calendarEventDialogsExposeUntargetedKeyboard()
{
    CalendarEvent event;

    for (const bool existingEvent : {false, true})
    {
        CalendarEventDialog dialog(event, existingEvent, true);
        dialog.show();
        QApplication::processEvents();

        auto* trigger = dialog.findChild<QPushButton*>(
            QStringLiteral("calendarEventKoreanKeyboardButton")
            );
        auto* keyboard = dialog.findChild<OnScreenKeyboard*>();
        QVERIFY(trigger);
        QVERIFY(keyboard);
        QVERIFY(!trigger->icon().isNull());
        QCOMPARE(trigger->accessibleName(), QStringLiteral("Korean Keyboard"));

        trigger->click();
        QApplication::processEvents();
        QVERIFY(keyboard->isVisible());
        QVERIFY(!keyboard->target());
        keyboard->close();
    }
}

void DialogShellTests::calendarEventDialogShowsInlineValidation()
{
    CalendarEventDialog dialog(CalendarEvent{}, false, true);
    dialog.show();
    QApplication::processEvents();

    auto* titleEdit = dialog.findChild<QLineEdit*>(
        QStringLiteral("calendarEventTitleEdit")
        );
    auto* message = dialog.findChild<QLabel*>(
        QStringLiteral("calendarEventTitleValidationMessage")
        );
    auto* buttons = dialog.findChild<QDialogButtonBox*>();
    QVERIFY(titleEdit);
    QVERIFY(message);
    QVERIFY(buttons);

    buttons->button(QDialogButtonBox::Save)->click();
    QApplication::processEvents();

    QCOMPARE(
        titleEdit->property("formValidationState").toString(),
        QStringLiteral("error")
        );
    QCOMPARE(message->text(), QStringLiteral("This field is required."));
    QVERIFY(!message->isHidden());
    QVERIFY(titleEdit->hasFocus());
    QVERIFY(dialog.isVisible());
}

QTEST_MAIN(DialogShellTests)
#include "dialog_shell_tests.moc"
