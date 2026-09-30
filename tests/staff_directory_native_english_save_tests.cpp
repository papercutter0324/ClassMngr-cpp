#include "core/application_services.h"
#include "data/database/database_session.h"
#include "data/repositories/native_english_teacher_repository.h"
#include "domain/models/native_english_teacher.h"
#include "features/teacher/ui/staff_directory_page.h"
#include "fakes/fake_user_prompt_service.h"
#include "ui/shared/dialogs/user_prompt_service.h"

#include <QPushButton>
#include <QSignalSpy>
#include <QSqlError>
#include <QSqlQuery>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QUuid>
#include <QtTest/QtTest>

namespace
{

constexpr int NativeEnglishIdRole = Qt::UserRole + 1;

QString databasePath(QTemporaryDir& directory)
{
    return directory.filePath(
        QStringLiteral("staff-directory-native-save-%1.tps").arg(
            QUuid::createUuid().toString(QUuid::WithoutBraces)
            )
        );
}

QList<NativeEnglishTeacher> fixtureTeachers()
{
    NativeEnglishTeacher updateTarget;
    updateTarget.name = QStringLiteral("Update Target");
    updateTarget.position = QStringLiteral("NET");
    updateTarget.phoneNumber = QStringLiteral("010-1000-0001");
    updateTarget.email = QStringLiteral("update@example.test");
    updateTarget.birthday = QStringLiteral("01-02");
    updateTarget.nationality = QStringLiteral("Korea");

    NativeEnglishTeacher deleteTarget;
    deleteTarget.name = QStringLiteral("Delete Target");
    deleteTarget.position = QStringLiteral("NET");
    deleteTarget.phoneNumber = QStringLiteral("010-1000-0002");
    deleteTarget.email = QStringLiteral("delete@example.test");
    deleteTarget.birthday = QStringLiteral("03-04");
    deleteTarget.nationality = QStringLiteral("Canada");

    return {updateTarget, deleteTarget};
}

bool seedTeachers(ApplicationServices& services)
{
    return static_cast<bool>(services.databaseSession()
        ->nativeEnglishTeacherRepository()->saveDirectory(fixtureTeachers(), {}));
}

QPushButton* buttonWithText(QWidget& page, const QString& text)
{
    for (QPushButton* button : page.findChildren<QPushButton*>())
    {
        if (button->text() == text)
        {
            return button;
        }
    }
    return nullptr;
}

int rowForId(const QTableWidget& table, const int id)
{
    for (int row = 0; row < table.rowCount(); ++row)
    {
        const auto* item = table.item(row, 0);
        if (item && item->data(NativeEnglishIdRole).toInt() == id)
        {
            return row;
        }
    }
    return -1;
}

bool hasTeacher(
    const QList<NativeEnglishTeacher>& teachers,
    const QString& name
    )
{
    for (const NativeEnglishTeacher& teacher : teachers)
    {
        if (teacher.name == name)
        {
            return true;
        }
    }
    return false;
}

bool sameTeacherLists(
    const QList<NativeEnglishTeacher>& left,
    const QList<NativeEnglishTeacher>& right
    )
{
    if (left.size() != right.size())
    {
        return false;
    }
    for (qsizetype index = 0; index < left.size(); ++index)
    {
        const NativeEnglishTeacher& a = left.at(index);
        const NativeEnglishTeacher& b = right.at(index);
        if (a.id != b.id
            || a.name != b.name
            || a.position != b.position
            || a.phoneNumber != b.phoneNumber
            || a.email != b.email
            || a.birthday != b.birthday
            || a.nationality != b.nationality)
        {
            return false;
        }
    }
    return true;
}

NativeEnglishTeacher teacherNamed(
    const QList<NativeEnglishTeacher>& teachers,
    const QString& name
    )
{
    for (const NativeEnglishTeacher& teacher : teachers)
    {
        if (teacher.name == name)
        {
            return teacher;
        }
    }
    return {};
}

QString expectedValidationWarning()
{
    return QStringLiteral(
        "Each Native English Teacher needs a unique name and a valid MM-dd birthday.");
}

}

class StaffDirectoryNativeEnglishSaveTests final : public QObject
{
    Q_OBJECT

private slots:
    void cleanup();
    void publicPageSaveMatchesPinnedBaselineBehavior();
    void duplicateUnicodeComparisonKeyPreventsWritesAndShowsCurrentWarning();
    void invalidBirthdayPreventsWritesAndShowsCurrentWarning();
    void unavailableSessionIsSilentAndRetainsDirtyRows();
    void repositoryFailureWarnsAndRetainsDirtyRows();
    void automaticSaveKeepsRepositoryFailuresQuiet();
};

void StaffDirectoryNativeEnglishSaveTests::cleanup()
{
    DialogServices::setUserPromptServiceForTesting(nullptr);
}

void StaffDirectoryNativeEnglishSaveTests::
publicPageSaveMatchesPinnedBaselineBehavior()
{
    // Pin the common public-page save outcome from accepted F160 commit
    // b703d3260a01b783594ffe6b87d10d9f7b02d193.
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    QVERIFY(seedTeachers(services));

    const auto original = services.databaseSession()
        ->nativeEnglishTeacherRepository()->getAll();
    QVERIFY(original);
    const NativeEnglishTeacher updateOriginal =
        teacherNamed(*original, QStringLiteral("Update Target"));
    const NativeEnglishTeacher deleteOriginal =
        teacherNamed(*original, QStringLiteral("Delete Target"));
    QVERIFY(updateOriginal.id > 0);
    QVERIFY(deleteOriginal.id > 0);

    StaffDirectoryPage page(
        &services,
        StaffDirectoryKind::NativeEnglishTeachers);
    page.setSaveMode(SaveMode::Manual);
    QVERIFY(page.loadDirectory());
    auto* table = page.findChild<QTableWidget*>(
        QStringLiteral("nativeEnglishTeachersTable"));
    QVERIFY(table);

    QSignalSpy saved(&page, &StaffDirectoryPage::directorySaved);
    const int updateRow = rowForId(*table, updateOriginal.id);
    const int deleteRow = rowForId(*table, deleteOriginal.id);
    QVERIFY(updateRow >= 0);
    QVERIFY(deleteRow >= 0);
    table->item(updateRow, 0)->setText(QStringLiteral("  Renamed   Teacher  "));
    table->item(updateRow, 1)->setText(QStringLiteral(" Team Leader "));
    table->item(updateRow, 2)->setText(QStringLiteral("010-2222-3333"));
    table->item(updateRow, 3)->setText(QStringLiteral("renamed@example.test"));
    table->item(updateRow, 4)->setText(QStringLiteral("02-29"));
    table->item(updateRow, 5)->setText(QStringLiteral("Australia"));

    FakeUserPromptService prompts;
    prompts.scriptedChoices.enqueue(PromptChoice::Destructive);
    DialogServices::setUserPromptServiceForTesting(&prompts);
    table->selectRow(deleteRow);
    QPushButton* const deleteButton = buttonWithText(page, QStringLiteral("Delete"));
    QVERIFY(deleteButton);
    deleteButton->click();

    QPushButton* const addButton = buttonWithText(page, QStringLiteral("Add"));
    QVERIFY(addButton);
    addButton->click();
    const int insertRow = table->rowCount() - 1;
    table->item(insertRow, 0)->setText(QStringLiteral("New Teacher"));
    table->item(insertRow, 1)->setText(QStringLiteral("NET"));
    table->item(insertRow, 2)->setText(QStringLiteral("010-3333-4444"));
    table->item(insertRow, 3)->setText(QStringLiteral("new@example.test"));
    table->item(insertRow, 4)->setText(QString());
    table->item(insertRow, 5)->setText(QStringLiteral("New Zealand"));
    table->setCurrentCell(insertRow, 1);

    QVERIFY(page.hasUnsavedChanges());
    QVERIFY(page.saveChanges());
    QCOMPARE(saved.count(), 1);
    QVERIFY(!page.hasUnsavedChanges());

    const auto persisted = services.databaseSession()
        ->nativeEnglishTeacherRepository()->getAll();
    QVERIFY(persisted);
    QCOMPARE(persisted->size(), 2);
    QVERIFY(!hasTeacher(*persisted, QStringLiteral("Delete Target")));

    const NativeEnglishTeacher updated =
        teacherNamed(*persisted, QStringLiteral("Renamed Teacher"));
    QCOMPARE(updated.id, updateOriginal.id);
    QCOMPARE(updated.position, QStringLiteral("Team Leader"));
    QCOMPARE(updated.phoneNumber, QStringLiteral("010-2222-3333"));
    QCOMPARE(updated.email, QStringLiteral("renamed@example.test"));
    QCOMPARE(updated.birthday, QStringLiteral("02-29"));
    QCOMPARE(updated.nationality, QStringLiteral("Australia"));

    const NativeEnglishTeacher inserted =
        teacherNamed(*persisted, QStringLiteral("New Teacher"));
    QVERIFY(inserted.id > 0);
    QVERIFY(inserted.id != updateOriginal.id);
    QCOMPARE(inserted.position, QStringLiteral("NET"));
    QCOMPARE(inserted.phoneNumber, QStringLiteral("010-3333-4444"));
    QCOMPARE(inserted.email, QStringLiteral("new@example.test"));
    QCOMPARE(inserted.birthday, QString());
    QCOMPARE(inserted.nationality, QStringLiteral("New Zealand"));

    const int reloadedNewRow = rowForId(*table, inserted.id);
    QVERIFY(reloadedNewRow >= 0);
    QCOMPARE(table->item(reloadedNewRow, 0)->text(), QStringLiteral("New Teacher"));
    QCOMPARE(table->item(reloadedNewRow, 5)->text(), QStringLiteral("New Zealand"));
    QVERIFY(page.saveChanges());
    QCOMPARE(saved.count(), 1);
}

void StaffDirectoryNativeEnglishSaveTests::
duplicateUnicodeComparisonKeyPreventsWritesAndShowsCurrentWarning()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    QVERIFY(seedTeachers(services));

    StaffDirectoryPage page(
        &services,
        StaffDirectoryKind::NativeEnglishTeachers);
    page.setSaveMode(SaveMode::Manual);
    QVERIFY(page.loadDirectory());
    auto* table = page.findChild<QTableWidget*>(
        QStringLiteral("nativeEnglishTeachersTable"));
    QVERIFY(table);
    const QString uppercaseKey =
        QStringLiteral("\u00C4").simplified().toCaseFolded();
    const QString lowercaseKey =
        QStringLiteral("\u00E4").simplified().toCaseFolded();
    QCOMPARE(uppercaseKey, lowercaseKey);
    table->item(0, 0)->setText(QStringLiteral("\u00C4"));
    table->item(1, 0)->setText(QStringLiteral("\u00E4"));

    const auto before = services.databaseSession()
        ->nativeEnglishTeacherRepository()->getAll();
    QVERIFY(before);
    QSignalSpy saved(&page, &StaffDirectoryPage::directorySaved);
    FakeUserPromptService prompts;
    DialogServices::setUserPromptServiceForTesting(&prompts);

    QVERIFY(!page.saveChanges());

    QVERIFY(page.hasUnsavedChanges());
    QCOMPARE(saved.count(), 0);
    QCOMPARE(prompts.messages.size(), 1);
    QCOMPARE(prompts.messages.first().severity, PromptSeverity::Warning);
    QCOMPARE(prompts.messages.first().title, QStringLiteral("Save Directory"));
    QCOMPARE(prompts.messages.first().message, expectedValidationWarning());
    const auto after = services.databaseSession()
        ->nativeEnglishTeacherRepository()->getAll();
    QVERIFY(after);
    QVERIFY(sameTeacherLists(*after, *before));
}

void StaffDirectoryNativeEnglishSaveTests::
invalidBirthdayPreventsWritesAndShowsCurrentWarning()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    QVERIFY(seedTeachers(services));

    StaffDirectoryPage page(
        &services,
        StaffDirectoryKind::NativeEnglishTeachers);
    page.setSaveMode(SaveMode::Manual);
    QVERIFY(page.loadDirectory());
    auto* table = page.findChild<QTableWidget*>(
        QStringLiteral("nativeEnglishTeachersTable"));
    QVERIFY(table);
    table->item(0, 4)->setText(QStringLiteral("02-30"));

    const auto before = services.databaseSession()
        ->nativeEnglishTeacherRepository()->getAll();
    QVERIFY(before);
    QSignalSpy saved(&page, &StaffDirectoryPage::directorySaved);
    FakeUserPromptService prompts;
    DialogServices::setUserPromptServiceForTesting(&prompts);

    QVERIFY(!page.saveChanges());

    QVERIFY(page.hasUnsavedChanges());
    QCOMPARE(saved.count(), 0);
    QCOMPARE(prompts.messages.size(), 1);
    QCOMPARE(prompts.messages.first().severity, PromptSeverity::Warning);
    QCOMPARE(prompts.messages.first().title, QStringLiteral("Save Directory"));
    QCOMPARE(prompts.messages.first().message, expectedValidationWarning());
    const auto after = services.databaseSession()
        ->nativeEnglishTeacherRepository()->getAll();
    QVERIFY(after);
    QVERIFY(sameTeacherLists(*after, *before));
}

void StaffDirectoryNativeEnglishSaveTests::
unavailableSessionIsSilentAndRetainsDirtyRows()
{
    ApplicationServices services;
    QVERIFY(services.dataService());
    QVERIFY(!services.hasOpenDatabase());
    StaffDirectoryPage page(
        &services,
        StaffDirectoryKind::NativeEnglishTeachers);
    page.setSaveMode(SaveMode::Manual);
    auto* table = page.findChild<QTableWidget*>(
        QStringLiteral("nativeEnglishTeachersTable"));
    QVERIFY(table);
    QPushButton* const addButton = buttonWithText(page, QStringLiteral("Add"));
    QVERIFY(addButton);
    addButton->click();
    table->item(0, 0)->setText(QString());

    QSignalSpy saved(&page, &StaffDirectoryPage::directorySaved);
    FakeUserPromptService prompts;
    DialogServices::setUserPromptServiceForTesting(&prompts);

    QVERIFY(!page.saveChanges());

    QVERIFY(page.hasUnsavedChanges());
    QCOMPARE(table->rowCount(), 1);
    QCOMPARE(table->item(0, 0)->text(), QString());
    QCOMPARE(saved.count(), 0);
    QCOMPARE(prompts.messages.size(), 0);
}

void StaffDirectoryNativeEnglishSaveTests::
repositoryFailureWarnsAndRetainsDirtyRows()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    QVERIFY(seedTeachers(services));

    StaffDirectoryPage page(
        &services,
        StaffDirectoryKind::NativeEnglishTeachers);
    page.setSaveMode(SaveMode::Manual);
    QVERIFY(page.loadDirectory());
    auto* table = page.findChild<QTableWidget*>(
        QStringLiteral("nativeEnglishTeachersTable"));
    QVERIFY(table);
    table->item(0, 0)->setText(QStringLiteral("Edited Locally"));
    FakeUserPromptService prompts;
    prompts.scriptedChoices.enqueue(PromptChoice::Destructive);
    DialogServices::setUserPromptServiceForTesting(&prompts);
    table->selectRow(1);
    QPushButton* const deleteButton = buttonWithText(page, QStringLiteral("Delete"));
    QVERIFY(deleteButton);
    deleteButton->click();
    QCOMPARE(table->rowCount(), 1);

    QSqlQuery drop(services.databaseSession()->database());
    QVERIFY2(drop.exec(QStringLiteral("DROP TABLE native_english_teachers")),
        qPrintable(drop.lastError().text()));
    QSignalSpy saved(&page, &StaffDirectoryPage::directorySaved);

    QVERIFY(!page.saveChanges());

    QVERIFY(page.hasUnsavedChanges());
    QCOMPARE(table->rowCount(), 1);
    QCOMPARE(table->item(0, 0)->text(), QStringLiteral("Edited Locally"));
    QCOMPARE(saved.count(), 0);
    QCOMPARE(prompts.messages.size(), 1);
    QCOMPARE(prompts.messages.first().severity, PromptSeverity::Warning);
    QCOMPARE(prompts.messages.first().title, QStringLiteral("Save Directory"));
    QVERIFY(!prompts.messages.first().message.isEmpty());
}

void StaffDirectoryNativeEnglishSaveTests::
automaticSaveKeepsRepositoryFailuresQuiet()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    QVERIFY(seedTeachers(services));

    StaffDirectoryPage page(
        &services,
        StaffDirectoryKind::NativeEnglishTeachers);
    QVERIFY(page.loadDirectory());
    auto* table = page.findChild<QTableWidget*>(
        QStringLiteral("nativeEnglishTeachersTable"));
    QVERIFY(table);

    QSqlQuery drop(services.databaseSession()->database());
    QVERIFY2(drop.exec(QStringLiteral("DROP TABLE native_english_teachers")),
        qPrintable(drop.lastError().text()));

    FakeUserPromptService prompts;
    DialogServices::setUserPromptServiceForTesting(&prompts);
    QSignalSpy saved(&page, &StaffDirectoryPage::directorySaved);
    table->item(0, 0)->setText(QStringLiteral("Automatic Edit"));
    QVERIFY(page.hasUnsavedChanges());

    QTest::qWait(1100);

    QVERIFY(page.hasUnsavedChanges());
    QCOMPARE(table->item(0, 0)->text(), QStringLiteral("Automatic Edit"));
    QCOMPARE(saved.count(), 0);
    QCOMPARE(prompts.messages.size(), 0);
}

QTEST_MAIN(StaffDirectoryNativeEnglishSaveTests)

#include "staff_directory_native_english_save_tests.moc"
