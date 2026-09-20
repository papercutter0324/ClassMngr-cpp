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

#include <QComboBox>
#include <QDateEdit>
#include <QDialogButtonBox>
#include <QApplication>
#include <QLabel>
#include <QLineEdit>
#include <QRadioButton>
#include <QPushButton>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTimeEdit>
#include <QTest>
#include <QTranslator>
#include <QVBoxLayout>

#include <string>
#include <type_traits>

using CalendarEventEditDraft =
    ClassMngr::Next::Application::CalendarEventEditDraft;
using CalendarEventId =
    ClassMngr::Next::Domain::CalendarEventId;

static_assert(std::is_base_of_v<DialogShell, CalendarEventDialog>);
static_assert(std::is_constructible_v<
    CalendarEventDialog,
    const CalendarEventEditDraft&,
    bool,
    bool
    >);
static_assert(!std::is_constructible_v<
    CalendarEventDialog,
    const CalendarEvent&,
    bool,
    bool
    >);
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
    void calendarEventDialogMapsTypedEditDraft();
    void calendarEventDialogAppliesTypedDefaults();
    void calendarEventDialogShowsInlineValidation();

private:
    QTemporaryDir m_settingsRoot;
};

CalendarEventId calendarEventId(
    const char* value
    )
{
    return *CalendarEventId::fromString(value);
}

CalendarEventEditDraft timedCalendarEventDraft()
{
    CalendarEventEditDraft draft;
    draft.id = calendarEventId("42");
    draft.repeatSeriesId = "series-42";
    draft.title = "Timed event";
    draft.startDate = "2026-09-20";
    draft.endDate = "2026-09-20";
    draft.startTime = "09:15";
    draft.endTime = "10:45";
    draft.eventType = "Meeting";
    draft.timeStatus = "Timed";
    return draft;
}

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
    CalendarEventEditDraft draft;

    for (const bool existingEvent : {false, true})
    {
        CalendarEventDialog dialog(draft, existingEvent, true);
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

void DialogShellTests::calendarEventDialogMapsTypedEditDraft()
{
    const CalendarEventEditDraft timed = timedCalendarEventDraft();

    CalendarEventDialog timedDialog(timed, true, true);
    const auto timedDraft = timedDialog.eventData();
    QVERIFY(timedDraft.id.has_value());
    QCOMPARE(timedDraft.id->value(), std::string("42"));
    QVERIFY(timedDraft.repeatSeriesId.has_value());
    QCOMPARE(
        timedDraft.repeatSeriesId.value(),
        std::string("series-42")
        );
    QCOMPARE(timedDraft.title, std::string("Timed event"));
    QCOMPARE(timedDraft.startDate, std::string("2026-09-20"));
    QCOMPARE(timedDraft.endDate, std::string("2026-09-20"));
    QVERIFY(timedDraft.startTime.has_value());
    QVERIFY(timedDraft.endTime.has_value());
    QCOMPARE(timedDraft.startTime.value(), std::string("09:15"));
    QCOMPARE(timedDraft.endTime.value(), std::string("10:45"));
    QVERIFY(!timedDraft.allDay);
    QCOMPARE(timedDraft.eventType, std::string("Meeting"));
    QCOMPARE(timedDraft.timeStatus, std::string("Timed"));

    auto* timedStartTime = timedDialog.findChild<QTimeEdit*>(
        QStringLiteral("calendarEventStartTimeEdit")
        );
    auto* timedEndTime = timedDialog.findChild<QTimeEdit*>(
        QStringLiteral("calendarEventEndTimeEdit")
        );
    QVERIFY(timedStartTime);
    QVERIFY(timedEndTime);
    QCOMPARE(timedStartTime->displayFormat(), QStringLiteral("HH:mm"));
    QCOMPARE(timedEndTime->displayFormat(), QStringLiteral("HH:mm"));

    QRadioButton* thisAndFollowingButton = nullptr;
    QRadioButton* meetingButton = nullptr;
    for (auto* button : timedDialog.findChildren<QRadioButton*>())
    {
        if (button->text() == QStringLiteral("This and following events"))
        {
            thisAndFollowingButton = button;
        }
        if (button->property("eventType").toString()
            == QStringLiteral("Meeting"))
        {
            meetingButton = button;
        }
    }
    QVERIFY(thisAndFollowingButton);
    QVERIFY(meetingButton);
    QVERIFY(meetingButton->isChecked());
    QCOMPARE(
        timedDialog.seriesEditScope(),
        CalendarEventSeriesEditScope::ThisEventOnly
        );
    thisAndFollowingButton->click();
    QCOMPARE(
        timedDialog.seriesEditScope(),
        CalendarEventSeriesEditScope::ThisAndFollowingEvents
        );

    CalendarEventEditDraft allDay = timed;
    allDay.id.reset();
    allDay.repeatSeriesId.reset();
    allDay.allDay = true;
    allDay.startTime.reset();
    allDay.endTime.reset();
    allDay.eventType = "Holiday";
    CalendarEventDialog allDayDialog(allDay, false, true);
    const auto allDayDraft = allDayDialog.eventData();
    QVERIFY(!allDayDraft.id.has_value());
    QVERIFY(!allDayDraft.repeatSeriesId.has_value());
    QVERIFY(allDayDraft.allDay);
    QVERIFY(!allDayDraft.startTime.has_value());
    QVERIFY(!allDayDraft.endTime.has_value());
    QCOMPARE(allDayDraft.eventType, std::string("Holiday"));
    QCOMPARE(allDayDraft.timeStatus, std::string("Timed"));

    auto* allDayStartTime = allDayDialog.findChild<QTimeEdit*>(
        QStringLiteral("calendarEventStartTimeEdit")
        );
    auto* allDayEndTime = allDayDialog.findChild<QTimeEdit*>(
        QStringLiteral("calendarEventEndTimeEdit")
        );
    QVERIFY(allDayStartTime);
    QVERIFY(allDayEndTime);
    QCOMPARE(allDayStartTime->time(), QTime(0, 0));
    QCOMPARE(allDayEndTime->time(), QTime(23, 59));
    QVERIFY(!allDayStartTime->isEnabled());
    QVERIFY(!allDayEndTime->isEnabled());

    CalendarEventEditDraft unconfirmed = timed;
    unconfirmed.id.reset();
    unconfirmed.repeatSeriesId.reset();
    unconfirmed.startTime.reset();
    unconfirmed.endTime.reset();
    unconfirmed.timeStatus = "Unconfirmed";
    unconfirmed.eventType = "Workshop";
    CalendarEventDialog unconfirmedDialog(unconfirmed, false, true);
    const auto unconfirmedDraft = unconfirmedDialog.eventData();
    QVERIFY(!unconfirmedDraft.allDay);
    QVERIFY(!unconfirmedDraft.startTime.has_value());
    QVERIFY(!unconfirmedDraft.endTime.has_value());
    QCOMPARE(unconfirmedDraft.eventType, std::string("Workshop"));
    QCOMPARE(unconfirmedDraft.timeStatus, std::string("Unconfirmed"));

    auto* unconfirmedStartTime = unconfirmedDialog.findChild<QTimeEdit*>(
        QStringLiteral("calendarEventStartTimeEdit")
        );
    auto* unconfirmedEndTime = unconfirmedDialog.findChild<QTimeEdit*>(
        QStringLiteral("calendarEventEndTimeEdit")
        );
    QVERIFY(unconfirmedStartTime);
    QVERIFY(unconfirmedEndTime);
    QCOMPARE(unconfirmedStartTime->time(), QTime(9, 0));
    QCOMPARE(unconfirmedEndTime->time(), QTime(10, 0));
    QVERIFY(!unconfirmedStartTime->isEnabled());
    QVERIFY(!unconfirmedEndTime->isEnabled());
}

void DialogShellTests::calendarEventDialogAppliesTypedDefaults()
{
    const CalendarEventEditDraft defaults;
    CalendarEventDialog dialog(defaults, false, true);

    auto* startDate = dialog.findChild<QDateEdit*>(
        QStringLiteral("calendarEventStartDateEdit")
        );
    auto* endDate = dialog.findChild<QDateEdit*>(
        QStringLiteral("calendarEventEndDateEdit")
        );
    auto* startTime = dialog.findChild<QTimeEdit*>(
        QStringLiteral("calendarEventStartTimeEdit")
        );
    auto* endTime = dialog.findChild<QTimeEdit*>(
        QStringLiteral("calendarEventEndTimeEdit")
        );
    auto* repeatFrequency = dialog.findChild<QComboBox*>(
        QStringLiteral("calendarEventRepeatFrequencyCombo")
        );
    QVERIFY(startDate);
    QVERIFY(endDate);
    QVERIFY(startTime);
    QVERIFY(endTime);
    QVERIFY(repeatFrequency);

    const QDate today = QDate::currentDate();
    QCOMPARE(startDate->date(), today);
    QCOMPARE(endDate->date(), today);
    QCOMPARE(startTime->time(), QTime(9, 0));
    QCOMPARE(endTime->time(), QTime(10, 0));
    QVERIFY(!dialog.repeatEnabled());
    QCOMPARE(
        dialog.repeatFrequency(),
        CalendarEventRepeatFrequency::Weekly
        );
    QCOMPARE(repeatFrequency->currentIndex(), 1);

    const auto draft = dialog.eventData();
    const std::string todayText =
        today.toString(Qt::ISODate).toStdString();
    QVERIFY(!draft.id.has_value());
    QVERIFY(!draft.repeatSeriesId.has_value());
    QCOMPARE(draft.title, std::string());
    QCOMPARE(draft.startDate, todayText);
    QCOMPARE(draft.endDate, todayText);
    QCOMPARE(draft.startTime.value(), std::string("09:00"));
    QCOMPARE(draft.endTime.value(), std::string("10:00"));
    QVERIFY(!draft.allDay);
    QCOMPARE(draft.eventType, std::string("Other"));
    QCOMPARE(draft.timeStatus, std::string("Timed"));
}

void DialogShellTests::calendarEventDialogShowsInlineValidation()
{
    CalendarEventDialog dialog(CalendarEventEditDraft{}, false, true);
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
