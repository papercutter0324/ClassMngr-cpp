#include "core/application_services.h"
#include "data/database/database_session.h"
#include "data/repositories/native_english_teacher_repository.h"
#include "domain/models/native_english_teacher.h"
#include "features/teacher/ui/staff_directory_page.h"
#include "staff_directory_test_utils.h"
#include "fakes/fake_user_prompt_service.h"
#include "ui/shared/dialogs/user_prompt_service.h"

#include <QPushButton>
#include <QSignalSpy>
#include <QSqlError>
#include <QSqlQuery>
#include <QTableView>
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

int rowForId(const QTableView& table, const int id)
{
    for (int row = 0; row < table.model()->rowCount(); ++row)
    {
        if (staffDirectoryCellData(
                table, row, 0, NativeEnglishIdRole).toInt() == id)
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
    void deletedTeacherSaveIsSilentAndRetainsEditedRows();
    void repositoryFailureWarnsAndRetainsDirtyRows();
    void automaticSaveKeepsRepositoryFailuresQuiet();
    void automaticSavePersistsEditAndEmitsSaved();
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
    auto* table = page.findChild<QTableView*>(
        QStringLiteral("nativeEnglishTeachersTable"));
    QVERIFY(table);

    QSignalSpy saved(&page, &StaffDirectoryPage::directorySaved);
    const int updateRow = rowForId(*table, updateOriginal.id);
    const int deleteRow = rowForId(*table, deleteOriginal.id);
    QVERIFY(updateRow >= 0);
    QVERIFY(deleteRow >= 0);
    setStaffDirectoryCellText(*table, updateRow, 0, QStringLiteral("  Renamed   Teacher  "));
    setStaffDirectoryCellText(*table, updateRow, 1, QStringLiteral(" Team Leader "));
    setStaffDirectoryCellText(*table, updateRow, 2, QStringLiteral("010-2222-3333"));
    setStaffDirectoryCellText(*table, updateRow, 3, QStringLiteral("renamed@example.test"));
    setStaffDirectoryCellText(*table, updateRow, 4, QStringLiteral("02-29"));
    setStaffDirectoryCellText(*table, updateRow, 5, QStringLiteral("Australia"));

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
    const int insertRow = table->model()->rowCount() - 1;
    setStaffDirectoryCellText(*table, insertRow, 0, QStringLiteral("New Teacher"));
    setStaffDirectoryCellText(*table, insertRow, 1, QStringLiteral("NET"));
    setStaffDirectoryCellText(*table, insertRow, 2, QStringLiteral("010-3333-4444"));
    setStaffDirectoryCellText(*table, insertRow, 3, QStringLiteral("new@example.test"));
    setStaffDirectoryCellText(*table, insertRow, 4, QString());
    setStaffDirectoryCellText(*table, insertRow, 5, QStringLiteral("New Zealand"));
    table->setCurrentIndex(table->model()->index(insertRow, 1));

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
    QCOMPARE(staffDirectoryCellText(*table, reloadedNewRow, 0), QStringLiteral("New Teacher"));
    QCOMPARE(staffDirectoryCellText(*table, reloadedNewRow, 5), QStringLiteral("New Zealand"));
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
    auto* table = page.findChild<QTableView*>(
        QStringLiteral("nativeEnglishTeachersTable"));
    QVERIFY(table);
    const QString uppercaseKey = QStringLiteral(
        "\u00a0\u00C4   Teacher\u3000Name\u2003").simplified().toCaseFolded();
    const QString lowercaseKey = QStringLiteral(
        "\u00E4 Teacher Name").simplified().toCaseFolded();
    QCOMPARE(uppercaseKey, lowercaseKey);
    setStaffDirectoryCellText(
        *table,
        0,
        0,
        QStringLiteral("\u00a0\u00C4   Teacher\u3000Name\u2003"));
    setStaffDirectoryCellText(*table, 1, 0, QStringLiteral("\u00E4 Teacher Name"));

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
    auto* table = page.findChild<QTableView*>(
        QStringLiteral("nativeEnglishTeachersTable"));
    QVERIFY(table);
    setStaffDirectoryCellText(*table, 0, 4, QStringLiteral("02-30"));

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
    auto* table = page.findChild<QTableView*>(
        QStringLiteral("nativeEnglishTeachersTable"));
    QVERIFY(table);
    QPushButton* const addButton = buttonWithText(page, QStringLiteral("Add"));
    QVERIFY(addButton);
    addButton->click();
    setStaffDirectoryCellText(*table, 0, 0, QString());

    QSignalSpy saved(&page, &StaffDirectoryPage::directorySaved);
    FakeUserPromptService prompts;
    DialogServices::setUserPromptServiceForTesting(&prompts);

    QVERIFY(!page.saveChanges());

    QVERIFY(page.hasUnsavedChanges());
    QCOMPARE(table->model()->rowCount(), 1);
    QCOMPARE(staffDirectoryCellText(*table, 0, 0), QString());
    QCOMPARE(saved.count(), 0);
    QCOMPARE(prompts.messages.size(), 0);
}

void StaffDirectoryNativeEnglishSaveTests::
deletedTeacherSaveIsSilentAndRetainsEditedRows()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    QVERIFY(seedTeachers(services));
    const auto original = services.databaseSession()
        ->nativeEnglishTeacherRepository()->getAll();
    QVERIFY(original);
    const NativeEnglishTeacher updateTarget =
        teacherNamed(*original, QStringLiteral("Update Target"));
    QVERIFY(updateTarget.id > 0);

    StaffDirectoryPage page(
        &services, StaffDirectoryKind::NativeEnglishTeachers);
    page.setSaveMode(SaveMode::Manual);
    QVERIFY(page.loadDirectory());
    auto* table = page.findChild<QTableView*>(
        QStringLiteral("nativeEnglishTeachersTable"));
    QVERIFY(table);
    const int updateRow = rowForId(*table, updateTarget.id);
    QVERIFY(updateRow >= 0);
    QVERIFY(setStaffDirectoryCellText(
        *table, updateRow, 0, QStringLiteral("Edited Locally")));
    QVERIFY(page.hasUnsavedChanges());

    QSqlQuery externalDelete(services.databaseSession()->database());
    externalDelete.prepare(QStringLiteral(
        "DELETE FROM native_english_teachers WHERE id=?"));
    externalDelete.addBindValue(updateTarget.id);
    QVERIFY2(externalDelete.exec(), qPrintable(externalDelete.lastError().text()));
    QCOMPARE(externalDelete.numRowsAffected(), 1);
    const auto beforeSave = services.databaseSession()
        ->nativeEnglishTeacherRepository()->getAll();
    QVERIFY(beforeSave);

    QSignalSpy saved(&page, &StaffDirectoryPage::directorySaved);
    FakeUserPromptService prompts;
    DialogServices::setUserPromptServiceForTesting(&prompts);
    QVERIFY(!page.saveChanges());

    QVERIFY(page.hasUnsavedChanges());
    QCOMPARE(table->model()->rowCount(), original->size());
    const int editedRow = rowForId(*table, updateTarget.id);
    QVERIFY(editedRow >= 0);
    QCOMPARE(staffDirectoryCellText(*table, editedRow, 0),
        QStringLiteral("Edited Locally"));
    QCOMPARE(saved.count(), 0);
    QCOMPARE(prompts.messages.size(), 0);
    const auto afterSave = services.databaseSession()
        ->nativeEnglishTeacherRepository()->getAll();
    QVERIFY(afterSave);
    QVERIFY(sameTeacherLists(*afterSave, *beforeSave));
    QVERIFY(teacherNamed(*afterSave, QStringLiteral("Update Target")).id <= 0);
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
    auto* table = page.findChild<QTableView*>(
        QStringLiteral("nativeEnglishTeachersTable"));
    QVERIFY(table);
    setStaffDirectoryCellText(*table, 0, 0, QStringLiteral("Edited Locally"));
    FakeUserPromptService prompts;
    prompts.scriptedChoices.enqueue(PromptChoice::Destructive);
    DialogServices::setUserPromptServiceForTesting(&prompts);
    table->selectRow(1);
    QPushButton* const deleteButton = buttonWithText(page, QStringLiteral("Delete"));
    QVERIFY(deleteButton);
    deleteButton->click();
    QCOMPARE(table->model()->rowCount(), 1);

    QSqlQuery drop(services.databaseSession()->database());
    QVERIFY2(drop.exec(QStringLiteral("DROP TABLE native_english_teachers")),
        qPrintable(drop.lastError().text()));
    QSignalSpy saved(&page, &StaffDirectoryPage::directorySaved);

    QVERIFY(!page.saveChanges());

    QVERIFY(page.hasUnsavedChanges());
    QCOMPARE(table->model()->rowCount(), 1);
    QCOMPARE(staffDirectoryCellText(*table, 0, 0), QStringLiteral("Edited Locally"));
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
    auto* table = page.findChild<QTableView*>(
        QStringLiteral("nativeEnglishTeachersTable"));
    QVERIFY(table);

    QSqlQuery drop(services.databaseSession()->database());
    QVERIFY2(drop.exec(QStringLiteral("DROP TABLE native_english_teachers")),
        qPrintable(drop.lastError().text()));

    FakeUserPromptService prompts;
    DialogServices::setUserPromptServiceForTesting(&prompts);
    QSignalSpy saved(&page, &StaffDirectoryPage::directorySaved);
    setStaffDirectoryCellText(*table, 0, 0, QStringLiteral("Automatic Edit"));
    QVERIFY(page.hasUnsavedChanges());

    QTest::qWait(1100);

    QVERIFY(page.hasUnsavedChanges());
    QCOMPARE(staffDirectoryCellText(*table, 0, 0), QStringLiteral("Automatic Edit"));
    QCOMPARE(saved.count(), 0);
    QCOMPARE(prompts.messages.size(), 0);
}

void StaffDirectoryNativeEnglishSaveTests::
automaticSavePersistsEditAndEmitsSaved()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    QVERIFY(seedTeachers(services));
    const auto before = services.databaseSession()
        ->nativeEnglishTeacherRepository()->getAll();
    QVERIFY(before);
    const NativeEnglishTeacher original =
        teacherNamed(*before, QStringLiteral("Update Target"));

    StaffDirectoryPage page(
        &services,
        StaffDirectoryKind::NativeEnglishTeachers);
    QVERIFY(page.loadDirectory());
    auto* table = page.findChild<QTableView*>(
        QStringLiteral("nativeEnglishTeachersTable"));
    QVERIFY(table);
    const int row = rowForId(*table, original.id);
    QVERIFY(row >= 0);

    QSignalSpy saved(&page, &StaffDirectoryPage::directorySaved);
    FakeUserPromptService prompts;
    DialogServices::setUserPromptServiceForTesting(&prompts);
    QVERIFY(setStaffDirectoryCellText(
        *table, row, 0, QStringLiteral("Automatic Rename")));
    QVERIFY(page.hasUnsavedChanges());

    QTRY_COMPARE_WITH_TIMEOUT(saved.count(), 1, 3000);

    QVERIFY(!page.hasUnsavedChanges());
    QCOMPARE(prompts.messages.size(), 0);
    const auto persisted = services.databaseSession()
        ->nativeEnglishTeacherRepository()->getAll();
    QVERIFY(persisted);
    const NativeEnglishTeacher updated =
        teacherNamed(*persisted, QStringLiteral("Automatic Rename"));
    QCOMPARE(updated.id, original.id);
}

QTEST_MAIN(StaffDirectoryNativeEnglishSaveTests)

#include "staff_directory_native_english_save_tests.moc"
