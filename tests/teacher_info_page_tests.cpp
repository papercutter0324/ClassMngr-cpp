#include "features/teacher/ui/teacher_info_page.h"
#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "data/database/database_session.h"
#include "data/repositories/teacher_repository.h"
#include "fakes/fake_user_prompt_service.h"
#include "ui/shared/dialogs/user_prompt_service.h"
#include "ui/shared/widgets/on_screen_keyboard.h"

#include <QApplication>
#include <QCalendarWidget>
#include <QComboBox>
#include <QCoreApplication>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QMetaObject>
#include <QInputDialog>
#include <QPushButton>
#include <QSqlQuery>
#include <QTimer>
#include <QTemporaryDir>
#include <QSignalSpy>
#include <QtTest>

namespace
{
QGridLayout* containingGrid(
    const TeacherInfoPage& page,
    QWidget* widget
    )
{
    const QList<QGridLayout*> grids =
        page.findChildren<QGridLayout*>();

    for (QGridLayout* grid : grids)
    {
        if (grid->indexOf(widget) >= 0)
        {
            return grid;
        }
    }

    return nullptr;
}

void verifyPosition(
    QGridLayout* grid,
    QWidget* widget,
    int expectedRow,
    int expectedColumn
    )
{
    QVERIFY(grid);
    QVERIFY(widget);

    int row = -1;
    int column = -1;
    int rowSpan = -1;
    int columnSpan = -1;
    grid->getItemPosition(
        grid->indexOf(widget),
        &row,
        &column,
        &rowSpan,
        &columnSpan
        );

    QCOMPARE(row, expectedRow);
    QCOMPARE(column, expectedColumn);
}
} // namespace

class TeacherInfoPageTests : public QObject
{
    Q_OBJECT

private slots:
    void cleanup();
    void personalDetailsUseRequestedTwoRowOrder();
    void preferredNameListsAvailableNameChoices();
    void promptsWhenSecondPreferredNameChoiceIsAdded();
    void birthdayUsesCalendarMonthAndDayOnly();
    void headerKeyboardOpensUntargeted();
    void inlineValidationBlocksManualSaveUntilCorrected();
    void saveUsesApplicationEditAndReloadsCanonicalTeacher();
    void applicationValidationBlocksInvalidTeacherWrite();
    void repositoryUpdateFailureKeepsDirtyStateAndShowsWarning();
    void repositoryReloadFailureKeepsDirtyStateAfterSuccessfulWrite();
};

void TeacherInfoPageTests::cleanup()
{
    DialogServices::setUserPromptServiceForTesting(nullptr);
}

void TeacherInfoPageTests::personalDetailsUseRequestedTwoRowOrder()
{
    TeacherInfoPage page(nullptr);

    auto* english = page.findChild<QLineEdit*>(
        QStringLiteral("teacherEnEdit"));
    auto* korean = page.findChild<QLineEdit*>(
        QStringLiteral("teacherKrEdit"));
    auto* romanization = page.findChild<QLineEdit*>(
        QStringLiteral("preferredRomanizationEdit"));
    auto* preferredName = page.findChild<QComboBox*>(
        QStringLiteral("preferredNameCombo"));
    auto* room = page.findChild<QLineEdit*>(
        QStringLiteral("roomNumberEdit"));
    auto* birthday = page.findChild<QLineEdit*>(
        QStringLiteral("birthdayEdit"));
    auto* phone = page.findChild<QLineEdit*>(
        QStringLiteral("phoneNumberEdit"));

    QGridLayout* grid = containingGrid(page, english);
    verifyPosition(grid, english, 1, 0);
    verifyPosition(grid, korean, 1, 1);
    verifyPosition(grid, romanization, 1, 2);
    verifyPosition(grid, preferredName, 1, 3);
    verifyPosition(grid, room, 5, 0);
    verifyPosition(grid, birthday, 5, 1);
    verifyPosition(grid, phone, 5, 2);

    page.resize(640, 480);
    page.show();
    QCoreApplication::processEvents();
    QCOMPARE(birthday->width(), korean->width());
}

void TeacherInfoPageTests::preferredNameListsAvailableNameChoices()
{
    TeacherInfoPage page(nullptr);

    Teacher teacher;
    teacher.id = 1;
    teacher.teacherEn = QStringLiteral("Alex Kim");
    teacher.preferredRomanization = QStringLiteral("Gim Allekseu");
    teacher.preferredName = QStringLiteral("Gim Allekseu");
    page.loadTeacher(teacher);

    auto* preferredName = page.findChild<QComboBox*>(
        QStringLiteral("preferredNameCombo"));
    QVERIFY(preferredName);
    QCOMPARE(preferredName->count(), 2);
    QCOMPARE(preferredName->itemText(0), QStringLiteral("Alex Kim"));
    QCOMPARE(preferredName->itemText(1), QStringLiteral("Gim Allekseu"));
    QCOMPARE(preferredName->currentText(), QStringLiteral("Gim Allekseu"));

    teacher.preferredRomanization.clear();
    teacher.preferredName.clear();
    page.loadTeacher(teacher);

    QCOMPARE(preferredName->count(), 1);
    QCOMPARE(preferredName->currentText(), QStringLiteral("Alex Kim"));
}

void TeacherInfoPageTests::promptsWhenSecondPreferredNameChoiceIsAdded()
{
    TeacherInfoPage page(nullptr);

    Teacher teacher;
    teacher.id = 1;
    teacher.teacherEn = QStringLiteral("Alex Kim");
    page.loadTeacher(teacher);

    auto* preferredSpelling = page.findChild<QLineEdit*>(
        QStringLiteral("preferredRomanizationEdit"));
    auto* preferredName = page.findChild<QComboBox*>(
        QStringLiteral("preferredNameCombo"));
    QVERIFY(preferredSpelling);
    QVERIFY(preferredName);

    bool dialogShown = false;
    QTimer::singleShot(
        0,
        [&dialogShown]
        {
            auto* dialog = qobject_cast<QInputDialog*>(
                QApplication::activeModalWidget()
                );
            dialogShown = dialog
                && dialog->windowTitle()
                    == QStringLiteral("Select Preferred Name");

            if (!dialog)
            {
                return;
            }

            if (auto* choices = dialog->findChild<QComboBox*>())
            {
                choices->setCurrentIndex(1);
            }

            dialog->accept();
        }
        );

    preferredSpelling->setText(QStringLiteral("Gim Allekseu"));

    QVERIFY(dialogShown);
    QCOMPARE(preferredName->currentText(), QStringLiteral("Gim Allekseu"));
}

void TeacherInfoPageTests::birthdayUsesCalendarMonthAndDayOnly()
{
    TeacherInfoPage page(nullptr);

    Teacher teacher;
    teacher.id = 1;
    teacher.teacherEn = QStringLiteral("Alex");
    teacher.birthday = QStringLiteral("02-29");
    page.loadTeacher(teacher);

    auto* birthday = page.findChild<QLineEdit*>(
        QStringLiteral("birthdayEdit"));
    QVERIFY(birthday);
    QVERIFY(birthday->isReadOnly());
    QCOMPARE(
        birthday->text(),
        QLocale().toString(
            QDate(2000, 2, 29),
            QStringLiteral("MMMM d")
            )
        );

    auto* calendar = birthday->findChild<QCalendarWidget*>();
    QVERIFY(calendar);
    QCOMPARE(calendar->minimumDate(), QDate(2000, 1, 1));
    QCOMPARE(calendar->maximumDate(), QDate(2000, 12, 31));
    QCOMPARE(
        calendar->verticalHeaderFormat(),
        QCalendarWidget::NoVerticalHeader
        );
    QVERIFY(!page.hasUnsavedChanges());

    const QDate selectedDate(2000, 12, 31);
    QVERIFY(
        QMetaObject::invokeMethod(
            calendar,
            "clicked",
            Qt::DirectConnection,
            Q_ARG(QDate, selectedDate)
            )
        );
    QVERIFY(page.hasUnsavedChanges());
    QCOMPARE(
        birthday->text(),
        QLocale().toString(selectedDate, QStringLiteral("MMMM d"))
        );

    birthday->clear();
    QVERIFY(page.hasUnsavedChanges());
    QVERIFY(birthday->text().isEmpty());
    QCOMPARE(birthday->placeholderText(), QStringLiteral("Not set"));
}

void TeacherInfoPageTests::headerKeyboardOpensUntargeted()
{
    TeacherInfoPage page(nullptr);
    page.resize(800, 600);
    page.show();
    QApplication::processEvents();

    auto* trigger = page.findChild<QPushButton*>(
        QStringLiteral("teacherInfoKoreanKeyboardButton")
        );
    auto* keyboard = page.findChild<OnScreenKeyboard*>();
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

void TeacherInfoPageTests::inlineValidationBlocksManualSaveUntilCorrected()
{
    TeacherInfoPage page(nullptr);
    page.setSaveMode(SaveMode::Manual);

    Teacher teacher;
    teacher.id = 1;
    teacher.teacherEn = QStringLiteral("Alex");
    page.loadTeacher(teacher);

    auto* english = page.findChild<QLineEdit*>(
        QStringLiteral("teacherEnEdit")
        );
    auto* message = page.findChild<QLabel*>(
        QStringLiteral("teacherEnValidationMessage")
        );
    auto* saveButton = page.findChild<QPushButton*>(
        QStringLiteral("teacherInfoSaveButton")
        );
    QVERIFY(english);
    QVERIFY(message);
    QVERIFY(saveButton);

    english->clear();

    QVERIFY(page.hasUnsavedChanges());
    QCOMPARE(
        english->property("formValidationState").toString(),
        QStringLiteral("error")
        );
    QCOMPARE(message->text(), QStringLiteral("This field is required."));
    QVERIFY(!message->isHidden());
    QVERIFY(!saveButton->isEnabled());

    english->setText(QStringLiteral("Alex"));

    QVERIFY(!english->property("formValidationState").isValid());
    QVERIFY(message->isHidden());
    QVERIFY(saveButton->isEnabled());
}

void TeacherInfoPageTests::saveUsesApplicationEditAndReloadsCanonicalTeacher()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    ApplicationServices services;
    QVERIFY(services.openDatabase(
        directory.filePath(QStringLiteral("teacher-profile.tps"))));

    Teacher original;
    original.teacherKr = QStringLiteral("김선생");
    original.teacherEn = QStringLiteral("Alex");
    const Result<int> created = services.teacherService()->create(original);
    QVERIFY(created.has_value());
    original.id = *created;

    TeacherInfoPage page(&services);
    page.setSaveMode(SaveMode::Manual);
    page.loadTeacher(original);

    auto* korean = page.findChild<QLineEdit*>(QStringLiteral("teacherKrEdit"));
    auto* english = page.findChild<QLineEdit*>(QStringLiteral("teacherEnEdit"));
    auto* koreanWarning = page.findChild<QLabel*>(
        QStringLiteral("teacherKrValidationMessage"));
    QVERIFY(korean);
    QVERIFY(english);
    QVERIFY(koreanWarning);

    QSignalSpy saved(&page, &TeacherInfoPage::teacherSaved);
    korean->setText(QStringLiteral("김가나다"));
    english->setText(QStringLiteral("Alexandra"));
    QVERIFY(page.hasUnsavedChanges());
    QCOMPARE(
        korean->property("formValidationState").toString(),
        QStringLiteral("warning"));

    QVERIFY(page.saveChanges());
    QVERIFY(!page.hasUnsavedChanges());
    QCOMPARE(saved.count(), 1);
    QCOMPARE(saved.constFirst().constFirst().toInt(), original.id);

    const Result<Teacher> reloaded =
        services.teacherService()->teacher(original.id);
    QVERIFY(reloaded.has_value());
    QCOMPARE(page.teacher().teacherEn, reloaded->teacherEn);
    QCOMPARE(page.teacher().teacherKr, reloaded->teacherKr);
    QCOMPARE(reloaded->teacherEn, QStringLiteral("Alexandra"));
    QCOMPARE(reloaded->teacherKr, QStringLiteral("김가나다"));
    QCOMPARE(koreanWarning->text(), QStringLiteral("Review this value."));
}

void TeacherInfoPageTests::applicationValidationBlocksInvalidTeacherWrite()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    ApplicationServices services;
    QVERIFY(services.openDatabase(
        directory.filePath(QStringLiteral("teacher-profile-validation.tps"))));

    Teacher original;
    original.teacherKr = QStringLiteral("김선생");
    original.teacherEn = QStringLiteral("Alex");
    const Result<int> created = services.teacherService()->create(original);
    QVERIFY(created.has_value());
    original.id = *created;

    TeacherInfoPage page(&services);
    page.setSaveMode(SaveMode::Manual);
    page.loadTeacher(original);

    auto* korean = page.findChild<QLineEdit*>(QStringLiteral("teacherKrEdit"));
    auto* english = page.findChild<QLineEdit*>(QStringLiteral("teacherEnEdit"));
    auto* englishMessage = page.findChild<QLabel*>(
        QStringLiteral("teacherEnValidationMessage"));
    QVERIFY(korean);
    QVERIFY(english);
    QVERIFY(englishMessage);

    QSignalSpy saved(&page, &TeacherInfoPage::teacherSaved);
    korean->clear();
    english->clear();
    QVERIFY(page.hasUnsavedChanges());
    QVERIFY(!page.saveChanges());
    QVERIFY(page.hasUnsavedChanges());
    QCOMPARE(saved.count(), 0);
    QCOMPARE(
        english->property("formValidationState").toString(),
        QStringLiteral("error"));
    QCOMPARE(englishMessage->text(), QStringLiteral("This field is required."));

    const Result<Teacher> unchanged =
        services.teacherService()->teacher(original.id);
    QVERIFY(unchanged.has_value());
    QCOMPARE(unchanged->teacherEn, QStringLiteral("Alex"));
    QCOMPARE(unchanged->teacherKr, QStringLiteral("김선생"));
}

void TeacherInfoPageTests::
repositoryUpdateFailureKeepsDirtyStateAndShowsWarning()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    ApplicationServices services;
    QVERIFY(services.openDatabase(
        directory.filePath(QStringLiteral("teacher-profile-update-failure.tps"))));

    Teacher original;
    original.teacherKr = QStringLiteral("김선생");
    original.teacherEn = QStringLiteral("Alex");
    const Result<int> created = services.teacherService()->create(original);
    QVERIFY(created);
    original.id = *created;

    QSqlQuery trigger(services.databaseSession()->database());
    QVERIFY(trigger.exec(QStringLiteral(
        "CREATE TRIGGER reject_teacher_profile_update "
        "BEFORE UPDATE ON teachers "
        "WHEN NEW.teacher_en = 'Reject Profile' "
        "BEGIN "
        "SELECT RAISE(ABORT, 'injected profile update failure'); "
        "END"
        )));

    FakeUserPromptService prompts;
    DialogServices::setUserPromptServiceForTesting(&prompts);
    TeacherInfoPage page(&services);
    page.setSaveMode(SaveMode::Manual);
    page.loadTeacher(original);

    auto* english = page.findChild<QLineEdit*>(
        QStringLiteral("teacherEnEdit"));
    QVERIFY(english);
    QSignalSpy saved(&page, &TeacherInfoPage::teacherSaved);
    english->setText(QStringLiteral("Reject Profile"));
    QVERIFY(page.hasUnsavedChanges());

    QVERIFY(!page.saveChanges());
    QVERIFY(page.hasUnsavedChanges());
    QCOMPARE(saved.count(), 0);
    QCOMPARE(prompts.messages.size(), 1);
    QCOMPARE(prompts.messages.first().severity, PromptSeverity::Warning);
    QCOMPARE(prompts.messages.first().title,
        QStringLiteral("Save Teacher Information"));
    QCOMPARE(prompts.messages.first().message,
        QStringLiteral("The teacher information could not be saved."));
    QVERIFY(prompts.messages.first().details.contains(
        QStringLiteral("injected profile update failure")));
    QCOMPARE(page.teacher().teacherEn, QStringLiteral("Alex"));

    const Result<Teacher> unchanged = services.databaseSession()
        ->teacherRepository()->getTeacher(original.id);
    QVERIFY(unchanged);
    QCOMPARE(unchanged->teacherEn, QStringLiteral("Alex"));
    QCOMPARE(unchanged->teacherKr, QStringLiteral("김선생"));
}

void TeacherInfoPageTests::
repositoryReloadFailureKeepsDirtyStateAfterSuccessfulWrite()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    ApplicationServices services;
    QVERIFY(services.openDatabase(
        directory.filePath(QStringLiteral("teacher-profile-reload-failure.tps"))));

    Teacher original;
    original.teacherKr = QStringLiteral("김선생");
    original.teacherEn = QStringLiteral("Alex");
    const Result<int> created = services.teacherService()->create(original);
    QVERIFY(created);
    original.id = *created;

    QSqlQuery trigger(services.databaseSession()->database());
    QVERIFY(trigger.exec(QStringLiteral(
        "CREATE TABLE teacher_profile_update_audit (teacher_id INTEGER, teacher_en TEXT)"
        )));
    QVERIFY(trigger.exec(QStringLiteral(
        "CREATE TRIGGER remove_teacher_after_profile_update "
        "AFTER UPDATE OF teacher_en ON teachers "
        "WHEN NEW.teacher_en = 'Reload Failure' "
        "BEGIN "
        "INSERT INTO teacher_profile_update_audit(teacher_id, teacher_en) "
        "VALUES (NEW.id, NEW.teacher_en); "
        "DELETE FROM teachers WHERE id = NEW.id; "
        "END"
        )));

    FakeUserPromptService prompts;
    DialogServices::setUserPromptServiceForTesting(&prompts);
    TeacherInfoPage page(&services);
    page.setSaveMode(SaveMode::Manual);
    page.loadTeacher(original);

    auto* english = page.findChild<QLineEdit*>(
        QStringLiteral("teacherEnEdit"));
    QVERIFY(english);
    QSignalSpy saved(&page, &TeacherInfoPage::teacherSaved);
    english->setText(QStringLiteral("Reload Failure"));
    QVERIFY(page.hasUnsavedChanges());

    QVERIFY(!page.saveChanges());
    QVERIFY(page.hasUnsavedChanges());
    QCOMPARE(saved.count(), 0);
    QCOMPARE(prompts.messages.size(), 1);
    QCOMPARE(prompts.messages.first().severity, PromptSeverity::Warning);
    QCOMPARE(prompts.messages.first().title,
        QStringLiteral("Save Teacher Information"));
    QCOMPARE(prompts.messages.first().message,
        QStringLiteral("The saved teacher information could not be reloaded."));
    QVERIFY(prompts.messages.first().details.contains(
        QStringLiteral("no matching record"), Qt::CaseInsensitive));
    QCOMPARE(page.teacher().teacherEn, QStringLiteral("Alex"));

    QSqlQuery audit(services.databaseSession()->database());
    QVERIFY(audit.exec(QStringLiteral(
        "SELECT teacher_id, teacher_en FROM teacher_profile_update_audit")));
    QVERIFY(audit.next());
    QCOMPARE(audit.value(0).toInt(), original.id);
    QCOMPARE(audit.value(1).toString(), QStringLiteral("Reload Failure"));
    QVERIFY(!audit.next());
    const Result<Teacher> deleted = services.databaseSession()
        ->teacherRepository()->getTeacher(original.id);
    QVERIFY(!deleted);
}

QTEST_MAIN(TeacherInfoPageTests)

#include "teacher_info_page_tests.moc"
