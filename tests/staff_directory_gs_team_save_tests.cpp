#include "core/application_services.h"
#include "data/database/database_session.h"
#include "data/repositories/gs_team_repository.h"
#include "domain/models/gs_team_member.h"
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

constexpr int IdRole = Qt::UserRole + 1;

QString databasePath(QTemporaryDir& directory)
{
    return directory.filePath(
        QStringLiteral("staff-directory-gs-team-save-%1.tps").arg(
            QUuid::createUuid().toString(QUuid::WithoutBraces)
            )
        );
}

GsTeamMember member(
    const QString& name,
    const QString& koreanName,
    const QString& position,
    const QString& phone,
    const QString& birthday
    )
{
    GsTeamMember value;
    value.name = name;
    value.koreanName = koreanName;
    value.position = position;
    value.phoneNumber = phone;
    value.birthday = birthday;
    return value;
}

QList<GsTeamMember> fixtureMembers()
{
    return {
        member(QStringLiteral("Update Target"),
            QStringLiteral("\uC218\uC815 \uB300\uC0C1"),
            QStringLiteral("M1"), QStringLiteral("010-1000-0001"),
            QStringLiteral("01-02")),
        member(QStringLiteral("Delete Target"),
            QStringLiteral("\uC0AD\uC81C \uB300\uC0C1"),
            QStringLiteral("M2"), QStringLiteral("010-1000-0002"),
            QStringLiteral("03-04"))
    };
}

bool seedDirectory(ApplicationServices& services)
{
    return static_cast<bool>(services.databaseSession()
        ->gsTeamRepository()->saveDirectory(fixtureMembers(), {}));
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
        if (item && item->data(IdRole).toInt() == id)
        {
            return row;
        }
    }
    return -1;
}

GsTeamMember memberNamed(
    const QList<GsTeamMember>& members,
    const QString& name
    )
{
    for (const GsTeamMember& value : members)
    {
        if (value.name == name)
        {
            return value;
        }
    }
    return {};
}

bool sameMembers(
    const QList<GsTeamMember>& left,
    const QList<GsTeamMember>& right
    )
{
    if (left.size() != right.size())
    {
        return false;
    }
    for (qsizetype index = 0; index < left.size(); ++index)
    {
        const GsTeamMember& a = left.at(index);
        const GsTeamMember& b = right.at(index);
        if (a.id != b.id
            || a.name != b.name
            || a.koreanName != b.koreanName
            || a.position != b.position
            || a.phoneNumber != b.phoneNumber
            || a.birthday != b.birthday)
        {
            return false;
        }
    }
    return true;
}

QString expectedValidationWarning()
{
    return QStringLiteral(
        "Each GS Team member needs a unique name or Korean name and a valid MM-dd birthday.");
}

void setEnglishName(QTableWidget& table, const int row, const QString& name)
{
    table.item(row, 0)->setText(name);
}

void setKoreanName(QTableWidget& table, const int row, const QString& name)
{
    table.item(row, 1)->setText(name);
}

}

class StaffDirectoryGsTeamSaveTests final : public QObject
{
    Q_OBJECT

private slots:
    void cleanup();
    void createsUpdatesDeletesAndReloadsFiveFieldsWithTypedIds();
    void requiresEitherNameAndRejectsDuplicateEnglishNamesWithoutWriting();
    void rejectsDuplicateKoreanNamesWithoutWriting();
    void permitsMatchingKeysAcrossEnglishAndKoreanNamespaces();
    void rejectsInvalidBirthdayAndPreservesDatabaseState();
    void unavailableSessionIsSilentAndRetainsEditedRows();
    void repositoryFailureRetainsEditsAndDeletionThenCanBeRetried();
    void automaticRepositoryFailureIsQuietAndKeepsDirtyState();
};

void StaffDirectoryGsTeamSaveTests::cleanup()
{
    DialogServices::setUserPromptServiceForTesting(nullptr);
}

void StaffDirectoryGsTeamSaveTests::
createsUpdatesDeletesAndReloadsFiveFieldsWithTypedIds()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    QVERIFY(seedDirectory(services));
    const auto before = services.databaseSession()
        ->gsTeamRepository()->getAll();
    QVERIFY(before);
    const GsTeamMember original =
        memberNamed(*before, QStringLiteral("Update Target"));
    const GsTeamMember removed =
        memberNamed(*before, QStringLiteral("Delete Target"));
    QVERIFY(original.id > 0);
    QVERIFY(removed.id > 0);

    StaffDirectoryPage page(&services, StaffDirectoryKind::GsTeam);
    page.setSaveMode(SaveMode::Manual);
    QVERIFY(page.loadDirectory());
    auto* table = page.findChild<QTableWidget*>(QStringLiteral("gsTeamTable"));
    QVERIFY(table);
    QCOMPARE(table->columnCount(), 5);
    QCOMPARE(table->rowCount(), 2);

    const int updateRow = rowForId(*table, original.id);
    const int deleteRow = rowForId(*table, removed.id);
    QVERIFY(updateRow >= 0);
    QVERIFY(deleteRow >= 0);
    QCOMPARE(table->item(updateRow, 0)->data(IdRole).toInt(), original.id);
    table->item(updateRow, 0)->setText(QStringLiteral("  Updated   Member  "));
    table->item(updateRow, 1)->setText(QStringLiteral("  \uC218\uC815    \uB2F4\uB2F9  "));
    table->item(updateRow, 2)->setText(QStringLiteral(" Team Leader "));
    table->item(updateRow, 3)->setText(QStringLiteral("010-2222-3333"));
    table->item(updateRow, 4)->setText(QStringLiteral("02-29"));

    FakeUserPromptService prompts;
    prompts.scriptedChoices.enqueue(PromptChoice::Destructive);
    DialogServices::setUserPromptServiceForTesting(&prompts);
    table->selectRow(deleteRow);
    QPushButton* deleteButton = buttonWithText(page, QStringLiteral("Delete"));
    QVERIFY(deleteButton);
    deleteButton->click();
    QCOMPARE(table->rowCount(), 1);

    QPushButton* addButton = buttonWithText(page, QStringLiteral("Add"));
    QVERIFY(addButton);
    addButton->click();
    const int insertRow = table->rowCount() - 1;
    table->item(insertRow, 0)->setText(QStringLiteral("New Member"));
    table->item(insertRow, 1)->setText(QStringLiteral("\uC2E0\uADDC"));
    table->item(insertRow, 2)->setText(QStringLiteral("M3"));
    table->item(insertRow, 3)->setText(QStringLiteral("010-3333-4444"));
    table->item(insertRow, 4)->setText(QString());

    QSignalSpy saved(&page, &StaffDirectoryPage::directorySaved);
    QVERIFY(page.hasUnsavedChanges());
    QVERIFY(page.saveChanges());
    QCOMPARE(saved.count(), 1);
    QVERIFY(!page.hasUnsavedChanges());
    QCOMPARE(table->columnCount(), 5);
    QCOMPARE(table->rowCount(), 2);

    const auto persisted = services.databaseSession()
        ->gsTeamRepository()->getAll();
    QVERIFY(persisted);
    QCOMPARE(persisted->size(), 2);
    const GsTeamMember updated =
        memberNamed(*persisted, QStringLiteral("Updated Member"));
    QCOMPARE(updated.id, original.id);
    QCOMPARE(updated.koreanName, QStringLiteral("\uC218\uC815 \uB2F4\uB2F9"));
    QCOMPARE(updated.position, QStringLiteral("Team Leader"));
    QCOMPARE(updated.phoneNumber, QStringLiteral("010-2222-3333"));
    QCOMPARE(updated.birthday, QStringLiteral("02-29"));
    QVERIFY(memberNamed(*persisted, QStringLiteral("Delete Target")).id <= 0);

    const GsTeamMember inserted =
        memberNamed(*persisted, QStringLiteral("New Member"));
    QVERIFY(inserted.id > 0);
    QVERIFY(inserted.id != original.id);
    QCOMPARE(inserted.koreanName, QStringLiteral("\uC2E0\uADDC"));
    QCOMPARE(inserted.position, QStringLiteral("M3"));
    QCOMPARE(inserted.phoneNumber, QStringLiteral("010-3333-4444"));
    QCOMPARE(inserted.birthday, QString());

    const int reloadedUpdateRow = rowForId(*table, original.id);
    const int reloadedInsertRow = rowForId(*table, inserted.id);
    QVERIFY(reloadedUpdateRow >= 0);
    QVERIFY(reloadedInsertRow >= 0);
    QCOMPARE(table->item(reloadedUpdateRow, 0)->text(),
        QStringLiteral("Updated Member"));
    QCOMPARE(table->item(reloadedUpdateRow, 1)->text(),
        QStringLiteral("\uC218\uC815 \uB2F4\uB2F9"));
    QCOMPARE(table->item(reloadedUpdateRow, 2)->text(),
        QStringLiteral("Team Leader"));
    QCOMPARE(table->item(reloadedUpdateRow, 3)->text(),
        QStringLiteral("010-2222-3333"));
    QCOMPARE(table->item(reloadedUpdateRow, 4)->text(),
        QStringLiteral("02-29"));
    QCOMPARE(table->item(reloadedInsertRow, 0)->data(IdRole).toInt(), inserted.id);
    QCOMPARE(table->item(reloadedInsertRow, 1)->text(),
        QStringLiteral("\uC2E0\uADDC"));
    QVERIFY(page.saveChanges());
    QCOMPARE(saved.count(), 1);
}

void StaffDirectoryGsTeamSaveTests::
requiresEitherNameAndRejectsDuplicateEnglishNamesWithoutWriting()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    QVERIFY(seedDirectory(services));

    StaffDirectoryPage page(&services, StaffDirectoryKind::GsTeam);
    page.setSaveMode(SaveMode::Manual);
    QVERIFY(page.loadDirectory());
    auto* table = page.findChild<QTableWidget*>(QStringLiteral("gsTeamTable"));
    QVERIFY(table);
    const auto before = services.databaseSession()
        ->gsTeamRepository()->getAll();
    QVERIFY(before);
    QSignalSpy saved(&page, &StaffDirectoryPage::directorySaved);
    FakeUserPromptService prompts;
    DialogServices::setUserPromptServiceForTesting(&prompts);

    table->item(0, 0)->setText(QStringLiteral("   "));
    table->item(0, 1)->setText(QStringLiteral("  \t "));
    QVERIFY(!page.saveChanges());
    QCOMPARE(prompts.messages.size(), 1);
    QCOMPARE(prompts.messages.first().message, expectedValidationWarning());
    QCOMPARE(saved.count(), 0);
    QVERIFY(page.hasUnsavedChanges());
    const auto afterEmpty = services.databaseSession()
        ->gsTeamRepository()->getAll();
    QVERIFY(afterEmpty);
    QVERIFY(sameMembers(*afterEmpty, *before));

    DialogServices::setUserPromptServiceForTesting(nullptr);
    prompts.messages.clear();
    setEnglishName(*table, 0, QStringLiteral("\u00C4"));
    setEnglishName(*table, 1, QStringLiteral("\u00E4"));
    DialogServices::setUserPromptServiceForTesting(&prompts);
    QVERIFY(!page.saveChanges());
    QCOMPARE(prompts.messages.size(), 1);
    QCOMPARE(prompts.messages.first().severity, PromptSeverity::Warning);
    QCOMPARE(prompts.messages.first().title, QStringLiteral("Save Directory"));
    QCOMPARE(prompts.messages.first().message, expectedValidationWarning());
    QCOMPARE(saved.count(), 0);
    const auto afterDuplicate = services.databaseSession()
        ->gsTeamRepository()->getAll();
    QVERIFY(afterDuplicate);
    QVERIFY(sameMembers(*afterDuplicate, *before));
}

void StaffDirectoryGsTeamSaveTests::
rejectsDuplicateKoreanNamesWithoutWriting()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    QVERIFY(seedDirectory(services));

    StaffDirectoryPage page(&services, StaffDirectoryKind::GsTeam);
    page.setSaveMode(SaveMode::Manual);
    QVERIFY(page.loadDirectory());
    auto* table = page.findChild<QTableWidget*>(QStringLiteral("gsTeamTable"));
    QVERIFY(table);
    const auto before = services.databaseSession()
        ->gsTeamRepository()->getAll();
    QVERIFY(before);
    setKoreanName(*table, 0, QStringLiteral("  \uD55C   \uAE00 "));
    setKoreanName(*table, 1, QStringLiteral("\uD55C \uAE00"));

    QSignalSpy saved(&page, &StaffDirectoryPage::directorySaved);
    FakeUserPromptService prompts;
    DialogServices::setUserPromptServiceForTesting(&prompts);
    QVERIFY(!page.saveChanges());

    QCOMPARE(saved.count(), 0);
    QCOMPARE(prompts.messages.size(), 1);
    QCOMPARE(prompts.messages.first().severity, PromptSeverity::Warning);
    QCOMPARE(prompts.messages.first().message, expectedValidationWarning());
    QVERIFY(page.hasUnsavedChanges());
    const auto after = services.databaseSession()
        ->gsTeamRepository()->getAll();
    QVERIFY(after);
    QVERIFY(sameMembers(*after, *before));
}

void StaffDirectoryGsTeamSaveTests::
permitsMatchingKeysAcrossEnglishAndKoreanNamespaces()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    QVERIFY(seedDirectory(services));

    StaffDirectoryPage page(&services, StaffDirectoryKind::GsTeam);
    page.setSaveMode(SaveMode::Manual);
    QVERIFY(page.loadDirectory());
    auto* table = page.findChild<QTableWidget*>(QStringLiteral("gsTeamTable"));
    QVERIFY(table);
    setEnglishName(*table, 0, QStringLiteral("shared"));
    setKoreanName(*table, 0, QStringLiteral("\uAC00"));
    setEnglishName(*table, 1, QStringLiteral("different"));
    setKoreanName(*table, 1, QStringLiteral("shared"));

    QSignalSpy saved(&page, &StaffDirectoryPage::directorySaved);
    FakeUserPromptService prompts;
    DialogServices::setUserPromptServiceForTesting(&prompts);
    QVERIFY(page.saveChanges());

    QCOMPARE(saved.count(), 1);
    QCOMPARE(prompts.messages.size(), 0);
    QVERIFY(!page.hasUnsavedChanges());
    const auto persisted = services.databaseSession()
        ->gsTeamRepository()->getAll();
    QVERIFY(persisted);
    QCOMPARE(memberNamed(*persisted, QStringLiteral("shared")).name,
        QStringLiteral("shared"));
    QCOMPARE(memberNamed(*persisted, QStringLiteral("different")).koreanName,
        QStringLiteral("shared"));
}

void StaffDirectoryGsTeamSaveTests::
rejectsInvalidBirthdayAndPreservesDatabaseState()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    QVERIFY(seedDirectory(services));

    StaffDirectoryPage page(&services, StaffDirectoryKind::GsTeam);
    page.setSaveMode(SaveMode::Manual);
    QVERIFY(page.loadDirectory());
    auto* table = page.findChild<QTableWidget*>(QStringLiteral("gsTeamTable"));
    QVERIFY(table);
    const auto before = services.databaseSession()
        ->gsTeamRepository()->getAll();
    QVERIFY(before);
    table->item(0, 4)->setText(QStringLiteral("02-30"));

    QSignalSpy saved(&page, &StaffDirectoryPage::directorySaved);
    FakeUserPromptService prompts;
    DialogServices::setUserPromptServiceForTesting(&prompts);
    QVERIFY(!page.saveChanges());

    QCOMPARE(saved.count(), 0);
    QCOMPARE(prompts.messages.size(), 1);
    QCOMPARE(prompts.messages.first().message, expectedValidationWarning());
    QVERIFY(page.hasUnsavedChanges());
    const auto after = services.databaseSession()
        ->gsTeamRepository()->getAll();
    QVERIFY(after);
    QVERIFY(sameMembers(*after, *before));
}

void StaffDirectoryGsTeamSaveTests::
unavailableSessionIsSilentAndRetainsEditedRows()
{
    ApplicationServices services;
    QVERIFY(services.dataService());
    QVERIFY(!services.hasOpenDatabase());

    StaffDirectoryPage page(&services, StaffDirectoryKind::GsTeam);
    page.setSaveMode(SaveMode::Manual);
    auto* table = page.findChild<QTableWidget*>(QStringLiteral("gsTeamTable"));
    QVERIFY(table);
    QPushButton* addButton = buttonWithText(page, QStringLiteral("Add"));
    QVERIFY(addButton);
    addButton->click();
    table->item(0, 0)->setText(QStringLiteral("Valid Name"));
    table->item(0, 1)->setText(QStringLiteral("\uAC00"));

    QSignalSpy saved(&page, &StaffDirectoryPage::directorySaved);
    FakeUserPromptService prompts;
    DialogServices::setUserPromptServiceForTesting(&prompts);
    QVERIFY(!page.saveChanges());

    QVERIFY(page.hasUnsavedChanges());
    QCOMPARE(table->rowCount(), 1);
    QCOMPARE(table->item(0, 0)->text(), QStringLiteral("Valid Name"));
    QCOMPARE(saved.count(), 0);
    QCOMPARE(prompts.messages.size(), 0);
}

void StaffDirectoryGsTeamSaveTests::
repositoryFailureRetainsEditsAndDeletionThenCanBeRetried()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    QVERIFY(seedDirectory(services));
    const auto original = services.databaseSession()
        ->gsTeamRepository()->getAll();
    QVERIFY(original);
    const GsTeamMember updateOriginal =
        memberNamed(*original, QStringLiteral("Update Target"));
    const GsTeamMember deleteOriginal =
        memberNamed(*original, QStringLiteral("Delete Target"));

    StaffDirectoryPage page(&services, StaffDirectoryKind::GsTeam);
    page.setSaveMode(SaveMode::Manual);
    QVERIFY(page.loadDirectory());
    auto* table = page.findChild<QTableWidget*>(QStringLiteral("gsTeamTable"));
    QVERIFY(table);
    const int updateRow = rowForId(*table, updateOriginal.id);
    const int deleteRow = rowForId(*table, deleteOriginal.id);
    QVERIFY(updateRow >= 0);
    QVERIFY(deleteRow >= 0);
    table->item(updateRow, 0)->setText(QStringLiteral("Edited Locally"));

    FakeUserPromptService prompts;
    prompts.scriptedChoices.enqueue(PromptChoice::Destructive);
    DialogServices::setUserPromptServiceForTesting(&prompts);
    table->selectRow(deleteRow);
    QPushButton* deleteButton = buttonWithText(page, QStringLiteral("Delete"));
    QVERIFY(deleteButton);
    deleteButton->click();
    QCOMPARE(table->rowCount(), 1);

    QSqlQuery createTrigger(services.databaseSession()->database());
    QVERIFY2(createTrigger.exec(QStringLiteral(
        "CREATE TRIGGER reject_gs_team_update BEFORE UPDATE ON gs_team "
        "BEGIN SELECT RAISE(ABORT, 'reject update'); END")),
        qPrintable(createTrigger.lastError().text()));
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

    const auto rolledBack = services.databaseSession()
        ->gsTeamRepository()->getAll();
    QVERIFY(rolledBack);
    QVERIFY(memberNamed(*rolledBack, QStringLiteral("Update Target")).id > 0);
    QVERIFY(memberNamed(*rolledBack, QStringLiteral("Delete Target")).id > 0);

    QSqlQuery dropTrigger(services.databaseSession()->database());
    QVERIFY2(dropTrigger.exec(QStringLiteral(
        "DROP TRIGGER reject_gs_team_update")),
        qPrintable(dropTrigger.lastError().text()));
    prompts.messages.clear();
    QVERIFY(page.saveChanges());
    QCOMPARE(saved.count(), 1);
    QVERIFY(!page.hasUnsavedChanges());
    QCOMPARE(prompts.messages.size(), 0);
    const auto persisted = services.databaseSession()
        ->gsTeamRepository()->getAll();
    QVERIFY(persisted);
    QCOMPARE(persisted->size(), 1);
    const GsTeamMember updated =
        memberNamed(*persisted, QStringLiteral("Edited Locally"));
    QCOMPARE(updated.id, updateOriginal.id);
    QVERIFY(memberNamed(*persisted, QStringLiteral("Delete Target")).id <= 0);
}

void StaffDirectoryGsTeamSaveTests::
automaticRepositoryFailureIsQuietAndKeepsDirtyState()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    QVERIFY(seedDirectory(services));

    StaffDirectoryPage page(&services, StaffDirectoryKind::GsTeam);
    QVERIFY(page.loadDirectory());
    auto* table = page.findChild<QTableWidget*>(QStringLiteral("gsTeamTable"));
    QVERIFY(table);

    QSqlQuery createTrigger(services.databaseSession()->database());
    QVERIFY2(createTrigger.exec(QStringLiteral(
        "CREATE TRIGGER reject_gs_team_update BEFORE UPDATE ON gs_team "
        "BEGIN SELECT RAISE(ABORT, 'reject update'); END")),
        qPrintable(createTrigger.lastError().text()));

    FakeUserPromptService prompts;
    DialogServices::setUserPromptServiceForTesting(&prompts);
    QSignalSpy saved(&page, &StaffDirectoryPage::directorySaved);
    table->item(0, 0)->setText(QStringLiteral("Automatic Edit"));
    QVERIFY(page.hasUnsavedChanges());

    QTest::qWait(1000);

    QVERIFY(page.hasUnsavedChanges());
    QCOMPARE(table->item(0, 0)->text(), QStringLiteral("Automatic Edit"));
    QCOMPARE(saved.count(), 0);
    QCOMPARE(prompts.messages.size(), 0);
}

QTEST_MAIN(StaffDirectoryGsTeamSaveTests)

#include "staff_directory_gs_team_save_tests.moc"
