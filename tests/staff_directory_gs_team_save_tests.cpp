#include "core/application_services.h"
#include "data/database/database_session.h"
#include "data/repositories/gs_team_repository.h"
#include "domain/models/gs_team_member.h"
#include "features/teacher/ui/staff_directory_page.h"
#include "staff_directory_test_utils.h"
#include "fakes/fake_user_prompt_service.h"
#include "ui/shared/dialogs/user_prompt_service.h"

#include <QLabel>
#include <QPushButton>
#include <QCoreApplication>
#include <QItemSelectionModel>
#include <QSignalSpy>
#include <QSqlError>
#include <QSqlQuery>
#include <QTableView>
#include <QTemporaryDir>
#include <QTranslator>
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

int rowForId(const QTableView& table, const int id)
{
    for (int row = 0; row < table.model()->rowCount(); ++row)
    {
        if (staffDirectoryCellData(table, row, 0, IdRole).toInt() == id)
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

void setEnglishName(QTableView& table, const int row, const QString& name)
{
    setStaffDirectoryCellText(table, row, 0, name);
}

void setKoreanName(QTableView& table, const int row, const QString& name)
{
    setStaffDirectoryCellText(table, row, 1, name);
}

class KoreanStaffDirectoryStatusTranslator final : public QTranslator
{
public:
    QString translate(
        const char* context,
        const char* sourceText,
        const char* disambiguation = nullptr,
        int n = -1
        ) const override
    {
        Q_UNUSED(disambiguation);
        Q_UNUSED(n);
        if (!context || !sourceText
            || qstrcmp(context, "StaffDirectoryPage") != 0)
        {
            return {};
        }

        if (qstrcmp(sourceText, "Save status") == 0)
        {
            return QStringLiteral("저장 상태");
        }
        if (qstrcmp(
                sourceText,
                "Staff directory changes could not be saved automatically. "
                "Your edits are still here.") == 0)
        {
            return QStringLiteral(
                "직원 디렉터리 변경 사항을 자동으로 저장하지 못했습니다. "
                "수정 내용은 그대로 유지됩니다.");
        }
        return {};
    }
};

class ScopedTranslatorInstallation final
{
public:
    explicit ScopedTranslatorInstallation(QTranslator& translator)
        : m_translator(&translator)
        , m_installed(QCoreApplication::installTranslator(m_translator))
    {
    }

    ~ScopedTranslatorInstallation()
    {
        if (m_installed)
        {
            QCoreApplication::removeTranslator(m_translator);
        }
    }

    bool installed() const
    {
        return m_installed;
    }

    bool remove()
    {
        if (!m_installed)
        {
            return false;
        }
        m_installed = false;
        return QCoreApplication::removeTranslator(m_translator);
    }

private:
    QTranslator* m_translator = nullptr;
    bool m_installed = false;
};

}

class StaffDirectoryGsTeamSaveTests final : public QObject
{
    Q_OBJECT

private slots:
    void cleanup();
    void createsUpdatesDeletesAndReloadsFiveFieldsWithTypedIds();
    void requiresEitherNameAndRejectsDuplicateEnglishNamesWithoutWriting();
    void preservesQtCaseFoldedNameNormalizationRules();
    void rejectsDuplicateKoreanNamesWithoutWriting();
    void permitsMatchingKeysAcrossEnglishAndKoreanNamespaces();
    void rejectsInvalidBirthdayAndPreservesDatabaseState();
    void unavailableSessionIsSilentAndRetainsEditedRows();
    void deletedMemberSaveIsSilentAndRetainsEditedRows();
    void repositoryFailureRetainsEditsAndDeletionThenCanBeRetried();
    void automaticRepositoryFailureShowsStatusAndCanBeRetried();
    void automaticSavePersistsEditAndEmitsSaved();
    void deletesOnlySelectedPersistedRowsWithOneConfirmation();
    void manualDiscardRestoresRowsAndClearsPendingDeletes();
    void refreshWhileDirtyPreservesEditsAndPendingDeletes();
    void successfulWriteWithFailedReloadRequiresReloadBeforeEditingOrSaving();
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
    auto* table = page.findChild<QTableView*>(QStringLiteral("gsTeamTable"));
    QVERIFY(table);
    QCOMPARE(table->model()->columnCount(), 5);
    QCOMPARE(table->model()->rowCount(), 2);

    const int updateRow = rowForId(*table, original.id);
    const int deleteRow = rowForId(*table, removed.id);
    QVERIFY(updateRow >= 0);
    QVERIFY(deleteRow >= 0);
    QCOMPARE(staffDirectoryCellData(*table, updateRow, 0, IdRole).toInt(), original.id);
    setStaffDirectoryCellText(*table, updateRow, 0, QStringLiteral("  Updated   Member  "));
    setStaffDirectoryCellText(*table, updateRow, 1, QStringLiteral("  \uC218\uC815    \uB2F4\uB2F9  "));
    setStaffDirectoryCellText(*table, updateRow, 2, QStringLiteral(" Team Leader "));
    setStaffDirectoryCellText(*table, updateRow, 3, QStringLiteral("010-2222-3333"));
    setStaffDirectoryCellText(*table, updateRow, 4, QStringLiteral("02-29"));

    FakeUserPromptService prompts;
    prompts.scriptedChoices.enqueue(PromptChoice::Destructive);
    DialogServices::setUserPromptServiceForTesting(&prompts);
    table->selectRow(deleteRow);
    QPushButton* deleteButton = buttonWithText(page, QStringLiteral("Delete"));
    QVERIFY(deleteButton);
    deleteButton->click();
    QCOMPARE(table->model()->rowCount(), 1);

    QPushButton* addButton = buttonWithText(page, QStringLiteral("Add"));
    QVERIFY(addButton);
    addButton->click();
    const int insertRow = table->model()->rowCount() - 1;
    setStaffDirectoryCellText(*table, insertRow, 0, QStringLiteral("New Member"));
    setStaffDirectoryCellText(*table, insertRow, 1, QStringLiteral("\uC2E0\uADDC"));
    setStaffDirectoryCellText(*table, insertRow, 2, QStringLiteral("M3"));
    setStaffDirectoryCellText(*table, insertRow, 3, QStringLiteral("010-3333-4444"));
    setStaffDirectoryCellText(*table, insertRow, 4, QString());

    QSignalSpy saved(&page, &StaffDirectoryPage::directorySaved);
    QVERIFY(page.hasUnsavedChanges());
    QVERIFY(page.saveChanges());
    QCOMPARE(saved.count(), 1);
    QVERIFY(!page.hasUnsavedChanges());
    QCOMPARE(table->model()->columnCount(), 5);
    QCOMPARE(table->model()->rowCount(), 2);

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
    QCOMPARE(staffDirectoryCellText(*table, reloadedUpdateRow, 0),
        QStringLiteral("Updated Member"));
    QCOMPARE(staffDirectoryCellText(*table, reloadedUpdateRow, 1),
        QStringLiteral("\uC218\uC815 \uB2F4\uB2F9"));
    QCOMPARE(staffDirectoryCellText(*table, reloadedUpdateRow, 2),
        QStringLiteral("Team Leader"));
    QCOMPARE(staffDirectoryCellText(*table, reloadedUpdateRow, 3),
        QStringLiteral("010-2222-3333"));
    QCOMPARE(staffDirectoryCellText(*table, reloadedUpdateRow, 4),
        QStringLiteral("02-29"));
    QCOMPARE(staffDirectoryCellData(*table, reloadedInsertRow, 0, IdRole).toInt(), inserted.id);
    QCOMPARE(staffDirectoryCellText(*table, reloadedInsertRow, 1),
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
    auto* table = page.findChild<QTableView*>(QStringLiteral("gsTeamTable"));
    QVERIFY(table);
    const auto before = services.databaseSession()
        ->gsTeamRepository()->getAll();
    QVERIFY(before);
    QSignalSpy saved(&page, &StaffDirectoryPage::directorySaved);
    FakeUserPromptService prompts;
    DialogServices::setUserPromptServiceForTesting(&prompts);

    setStaffDirectoryCellText(*table, 0, 0, QStringLiteral("   "));
    setStaffDirectoryCellText(*table, 0, 1, QStringLiteral("  \t "));
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
preservesQtCaseFoldedNameNormalizationRules()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    QVERIFY(seedDirectory(services));

    StaffDirectoryPage page(&services, StaffDirectoryKind::GsTeam);
    page.setSaveMode(SaveMode::Manual);
    QVERIFY(page.loadDirectory());
    auto* table = page.findChild<QTableView*>(QStringLiteral("gsTeamTable"));
    QVERIFY(table);

    setEnglishName(*table, 0, QStringLiteral("Stra\u00DFe"));
    setEnglishName(*table, 1, QStringLiteral("STRASSE"));
    QSignalSpy saved(&page, &StaffDirectoryPage::directorySaved);
    FakeUserPromptService prompts;
    DialogServices::setUserPromptServiceForTesting(&prompts);

    QVERIFY(page.saveChanges());
    QCOMPARE(saved.count(), 1);
    QCOMPARE(prompts.messages.size(), 0);
    const auto afterDistinctNames = services.databaseSession()
        ->gsTeamRepository()->getAll();
    QVERIFY(afterDistinctNames);
    QCOMPARE(memberNamed(*afterDistinctNames, QStringLiteral("Stra\u00DFe")).name,
        QStringLiteral("Stra\u00DFe"));
    QCOMPARE(memberNamed(*afterDistinctNames, QStringLiteral("STRASSE")).name,
        QStringLiteral("STRASSE"));

    setEnglishName(*table, 0, QStringLiteral("\u00C4"));
    setEnglishName(*table, 1, QStringLiteral("\u00E4"));
    QVERIFY(!page.saveChanges());
    QCOMPARE(saved.count(), 1);
    QCOMPARE(prompts.messages.size(), 1);
    QCOMPARE(prompts.messages.first().severity, PromptSeverity::Warning);
    QCOMPARE(prompts.messages.first().message, expectedValidationWarning());
    const auto afterEquivalentNames = services.databaseSession()
        ->gsTeamRepository()->getAll();
    QVERIFY(afterEquivalentNames);
    QVERIFY(sameMembers(*afterEquivalentNames, *afterDistinctNames));
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
    auto* table = page.findChild<QTableView*>(QStringLiteral("gsTeamTable"));
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
    auto* table = page.findChild<QTableView*>(QStringLiteral("gsTeamTable"));
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
    auto* table = page.findChild<QTableView*>(QStringLiteral("gsTeamTable"));
    QVERIFY(table);
    const auto before = services.databaseSession()
        ->gsTeamRepository()->getAll();
    QVERIFY(before);
    setStaffDirectoryCellText(*table, 0, 4, QStringLiteral("02-30"));

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
    auto* table = page.findChild<QTableView*>(QStringLiteral("gsTeamTable"));
    QVERIFY(table);
    QPushButton* addButton = buttonWithText(page, QStringLiteral("Add"));
    QVERIFY(addButton);
    addButton->click();
    setStaffDirectoryCellText(*table, 0, 0, QStringLiteral("Valid Name"));
    setStaffDirectoryCellText(*table, 0, 1, QStringLiteral("\uAC00"));

    QSignalSpy saved(&page, &StaffDirectoryPage::directorySaved);
    FakeUserPromptService prompts;
    DialogServices::setUserPromptServiceForTesting(&prompts);
    QVERIFY(!page.saveChanges());

    QVERIFY(page.hasUnsavedChanges());
    QCOMPARE(table->model()->rowCount(), 1);
    QCOMPARE(staffDirectoryCellText(*table, 0, 0), QStringLiteral("Valid Name"));
    QCOMPARE(saved.count(), 0);
    QCOMPARE(prompts.messages.size(), 0);
}

void StaffDirectoryGsTeamSaveTests::
deletedMemberSaveIsSilentAndRetainsEditedRows()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    QVERIFY(seedDirectory(services));
    const auto original = services.databaseSession()
        ->gsTeamRepository()->getAll();
    QVERIFY(original);
    const GsTeamMember updateTarget =
        memberNamed(*original, QStringLiteral("Update Target"));
    QVERIFY(updateTarget.id > 0);

    StaffDirectoryPage page(&services, StaffDirectoryKind::GsTeam);
    page.setSaveMode(SaveMode::Manual);
    QVERIFY(page.loadDirectory());
    auto* table = page.findChild<QTableView*>(QStringLiteral("gsTeamTable"));
    QVERIFY(table);
    const int updateRow = rowForId(*table, updateTarget.id);
    QVERIFY(updateRow >= 0);
    QVERIFY(setStaffDirectoryCellText(
        *table, updateRow, 0, QStringLiteral("Edited Locally")));
    QVERIFY(page.hasUnsavedChanges());

    QSqlQuery externalDelete(services.databaseSession()->database());
    externalDelete.prepare(QStringLiteral("DELETE FROM gs_team WHERE id=?"));
    externalDelete.addBindValue(updateTarget.id);
    QVERIFY2(externalDelete.exec(), qPrintable(externalDelete.lastError().text()));
    QCOMPARE(externalDelete.numRowsAffected(), 1);
    const auto beforeSave = services.databaseSession()
        ->gsTeamRepository()->getAll();
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
        ->gsTeamRepository()->getAll();
    QVERIFY(afterSave);
    QVERIFY(sameMembers(*afterSave, *beforeSave));
    QVERIFY(memberNamed(*afterSave, QStringLiteral("Update Target")).id <= 0);
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
    auto* table = page.findChild<QTableView*>(QStringLiteral("gsTeamTable"));
    QVERIFY(table);
    const int updateRow = rowForId(*table, updateOriginal.id);
    const int deleteRow = rowForId(*table, deleteOriginal.id);
    QVERIFY(updateRow >= 0);
    QVERIFY(deleteRow >= 0);
    setStaffDirectoryCellText(*table, updateRow, 0, QStringLiteral("Edited Locally"));

    FakeUserPromptService prompts;
    prompts.scriptedChoices.enqueue(PromptChoice::Destructive);
    DialogServices::setUserPromptServiceForTesting(&prompts);
    table->selectRow(deleteRow);
    QPushButton* deleteButton = buttonWithText(page, QStringLiteral("Delete"));
    QVERIFY(deleteButton);
    deleteButton->click();
    QCOMPARE(table->model()->rowCount(), 1);

    QSqlQuery createTrigger(services.databaseSession()->database());
    QVERIFY2(createTrigger.exec(QStringLiteral(
        "CREATE TRIGGER reject_gs_team_update BEFORE UPDATE ON gs_team "
        "BEGIN SELECT RAISE(ABORT, 'reject update'); END")),
        qPrintable(createTrigger.lastError().text()));
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
    auto* status = page.findChild<QLabel*>(
        QStringLiteral("staffDirectoryAutosaveFailureStatus"));
    QVERIFY(status);
    QVERIFY(status->isHidden());

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
automaticRepositoryFailureShowsStatusAndCanBeRetried()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    QVERIFY(seedDirectory(services));

    StaffDirectoryPage page(&services, StaffDirectoryKind::GsTeam);
    QVERIFY(page.loadDirectory());
    auto* table = page.findChild<QTableView*>(QStringLiteral("gsTeamTable"));
    QVERIFY(table);
    const int editedMemberId =
        staffDirectoryCellData(*table, 0, 0, IdRole).toInt();
    QVERIFY(editedMemberId > 0);

    QSqlQuery createTrigger(services.databaseSession()->database());
    QVERIFY2(createTrigger.exec(QStringLiteral(
        "CREATE TRIGGER reject_gs_team_update BEFORE UPDATE ON gs_team "
        "BEGIN SELECT RAISE(ABORT, 'reject update'); END")),
        qPrintable(createTrigger.lastError().text()));

    FakeUserPromptService prompts;
    DialogServices::setUserPromptServiceForTesting(&prompts);
    QSignalSpy saved(&page, &StaffDirectoryPage::directorySaved);
    auto* status = page.findChild<QLabel*>(
        QStringLiteral("staffDirectoryAutosaveFailureStatus"));
    QVERIFY(status);
    QVERIFY(status->isHidden());
    setStaffDirectoryCellText(*table, 0, 0, QStringLiteral("Automatic Edit"));
    QVERIFY(page.hasUnsavedChanges());

    QTest::qWait(1000);

    QVERIFY(page.hasUnsavedChanges());
    QCOMPARE(table->model()->rowCount(), 2);
    QCOMPARE(staffDirectoryCellText(*table, 0, 0), QStringLiteral("Automatic Edit"));
    QCOMPARE(saved.count(), 0);
    QCOMPARE(prompts.messages.size(), 0);
    QVERIFY(!status->isHidden());
    QCOMPARE(status->text(), QStringLiteral(
        "Staff directory changes could not be saved automatically. "
        "Your edits are still here."));

    KoreanStaffDirectoryStatusTranslator koreanTranslator;
    ScopedTranslatorInstallation translatorInstallation(koreanTranslator);
    QVERIFY(translatorInstallation.installed());
    page.retranslateUi();
    QCOMPARE(status->text(), QStringLiteral(
        "직원 디렉터리 변경 사항을 자동으로 저장하지 못했습니다. "
        "수정 내용은 그대로 유지됩니다."));
    QCOMPARE(status->accessibleName(), QStringLiteral("저장 상태"));
    QVERIFY(translatorInstallation.remove());
    page.retranslateUi();
    QCOMPARE(status->text(), QStringLiteral(
        "Staff directory changes could not be saved automatically. "
        "Your edits are still here."));

    QSqlQuery dropTrigger(services.databaseSession()->database());
    QVERIFY2(dropTrigger.exec(QStringLiteral(
        "DROP TRIGGER reject_gs_team_update")),
        qPrintable(dropTrigger.lastError().text()));
    QVERIFY(setStaffDirectoryCellText(
        *table, 0, 0, QStringLiteral("Recovered Edit")));
    QTRY_COMPARE_WITH_TIMEOUT(saved.count(), 1, 3000);

    QVERIFY(!page.hasUnsavedChanges());
    QCOMPARE(prompts.messages.size(), 0);
    QVERIFY(status->isHidden());
    QVERIFY(status->text().isEmpty());
    const auto persisted = services.databaseSession()
        ->gsTeamRepository()->getAll();
    QVERIFY(persisted);
    QCOMPARE(persisted->size(), 2);
    QCOMPARE(memberNamed(*persisted, QStringLiteral("Recovered Edit")).id,
        editedMemberId);
}

void StaffDirectoryGsTeamSaveTests::
automaticSavePersistsEditAndEmitsSaved()
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
    QVERIFY(original.id > 0);

    StaffDirectoryPage page(&services, StaffDirectoryKind::GsTeam);
    page.setSaveMode(SaveMode::Automatic);
    QVERIFY(page.loadDirectory());
    auto* table = page.findChild<QTableView*>(QStringLiteral("gsTeamTable"));
    QVERIFY(table);
    const int row = rowForId(*table, original.id);
    QVERIFY(row >= 0);

    QSignalSpy saved(&page, &StaffDirectoryPage::directorySaved);
    FakeUserPromptService prompts;
    DialogServices::setUserPromptServiceForTesting(&prompts);
    QVERIFY(setStaffDirectoryCellText(
        *table, row, 0, QStringLiteral("Automatic Rename")));
    QVERIFY(setStaffDirectoryCellText(
        *table, row, 3, QStringLiteral("010-7777-8888")));
    QVERIFY(page.hasUnsavedChanges());

    QTRY_COMPARE_WITH_TIMEOUT(saved.count(), 1, 3000);

    QVERIFY(!page.hasUnsavedChanges());
    QCOMPARE(prompts.messages.size(), 0);
    const auto persisted = services.databaseSession()
        ->gsTeamRepository()->getAll();
    QVERIFY(persisted);
    QCOMPARE(persisted->size(), before->size());
    const GsTeamMember updated =
        memberNamed(*persisted, QStringLiteral("Automatic Rename"));
    QCOMPARE(updated.id, original.id);
    QCOMPARE(updated.koreanName, original.koreanName);
    QCOMPARE(updated.phoneNumber, QStringLiteral("010-7777-8888"));
}

void StaffDirectoryGsTeamSaveTests::
deletesOnlySelectedPersistedRowsWithOneConfirmation()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    QVERIFY(static_cast<bool>(services.databaseSession()
        ->gsTeamRepository()->saveDirectory(
            {
                member(QStringLiteral("Delete One"), QStringLiteral("\uAC00"),
                    QStringLiteral("M1"), QStringLiteral("010-5000-0001"),
                    QStringLiteral("01-02")),
                member(QStringLiteral("Keep Member"), QStringLiteral("\uB098"),
                    QStringLiteral("M2"), QStringLiteral("010-5000-0002"),
                    QStringLiteral("02-03")),
                member(QStringLiteral("Delete Two"), QStringLiteral("\uB2E4"),
                    QStringLiteral("M3"), QStringLiteral("010-5000-0003"),
                    QStringLiteral("03-04"))
            },
            {}
            )));
    const auto original = services.databaseSession()
        ->gsTeamRepository()->getAll();
    QVERIFY(original);
    const GsTeamMember keepMember =
        memberNamed(*original, QStringLiteral("Keep Member"));
    const GsTeamMember deleteOne =
        memberNamed(*original, QStringLiteral("Delete One"));
    const GsTeamMember deleteTwo =
        memberNamed(*original, QStringLiteral("Delete Two"));

    StaffDirectoryPage page(&services, StaffDirectoryKind::GsTeam);
    page.setSaveMode(SaveMode::Manual);
    QVERIFY(page.loadDirectory());
    auto* table = page.findChild<QTableView*>(QStringLiteral("gsTeamTable"));
    QVERIFY(table);
    const int deleteOneRow = rowForId(*table, deleteOne.id);
    const int deleteTwoRow = rowForId(*table, deleteTwo.id);
    QVERIFY(deleteOneRow >= 0);
    QVERIFY(deleteTwoRow >= 0);

    QPushButton* addButton = buttonWithText(page, QStringLiteral("Add"));
    QVERIFY(addButton);
    addButton->click();
    const int draftRow = table->model()->rowCount() - 1;
    QCOMPARE(staffDirectoryCellData(*table, draftRow, 0, IdRole).toInt(), 0);
    setStaffDirectoryCellText(
        *table, draftRow, 0, QStringLiteral("Draft Member"));
    setStaffDirectoryCellText(
        *table, draftRow, 1, QStringLiteral("\uB77C"));
    setStaffDirectoryCellText(*table, draftRow, 2, QStringLiteral("NET"));
    setStaffDirectoryCellText(
        *table, draftRow, 3, QStringLiteral("010-5000-0004"));

    table->selectionModel()->clearSelection();
    table->selectionModel()->select(
        table->model()->index(deleteOneRow, 0),
        QItemSelectionModel::Select | QItemSelectionModel::Rows);
    table->selectionModel()->select(
        table->model()->index(deleteTwoRow, 0),
        QItemSelectionModel::Select | QItemSelectionModel::Rows);
    QCOMPARE(table->selectionModel()->selectedRows().size(), 2);

    FakeUserPromptService prompts;
    prompts.scriptedChoices.enqueue(PromptChoice::Destructive);
    DialogServices::setUserPromptServiceForTesting(&prompts);
    QPushButton* deleteButton = buttonWithText(page, QStringLiteral("Delete"));
    QVERIFY(deleteButton);
    deleteButton->click();
    QCOMPARE(prompts.confirmations.size(), 1);
    QCOMPARE(table->model()->rowCount(), 2);
    QVERIFY(page.hasUnsavedChanges());

    QSignalSpy saved(&page, &StaffDirectoryPage::directorySaved);
    QVERIFY(page.saveChanges());
    QCOMPARE(saved.count(), 1);
    QVERIFY(!page.hasUnsavedChanges());

    const auto persisted = services.databaseSession()
        ->gsTeamRepository()->getAll();
    QVERIFY(persisted);
    QCOMPARE(persisted->size(), 2);
    QCOMPARE(memberNamed(*persisted, QStringLiteral("Keep Member")).id,
        keepMember.id);
    QVERIFY(memberNamed(*persisted, QStringLiteral("Delete One")).id <= 0);
    QVERIFY(memberNamed(*persisted, QStringLiteral("Delete Two")).id <= 0);
    QVERIFY(memberNamed(*persisted, QStringLiteral("Draft Member")).id > 0);
    QCOMPARE(prompts.confirmations.size(), 1);
}

void StaffDirectoryGsTeamSaveTests::
manualDiscardRestoresRowsAndClearsPendingDeletes()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    QVERIFY(seedDirectory(services));
    const auto original = services.databaseSession()
        ->gsTeamRepository()->getAll();
    QVERIFY(original);
    const GsTeamMember updateTarget =
        memberNamed(*original, QStringLiteral("Update Target"));
    const GsTeamMember deleteTarget =
        memberNamed(*original, QStringLiteral("Delete Target"));

    StaffDirectoryPage page(&services, StaffDirectoryKind::GsTeam);
    page.setSaveMode(SaveMode::Manual);
    QVERIFY(page.loadDirectory());
    auto* table = page.findChild<QTableView*>(QStringLiteral("gsTeamTable"));
    QVERIFY(table);
    setStaffDirectoryCellText(
        *table,
        rowForId(*table, updateTarget.id),
        0,
        QStringLiteral("Edited before discard"));

    FakeUserPromptService prompts;
    prompts.scriptedChoices.enqueue(PromptChoice::Destructive);
    DialogServices::setUserPromptServiceForTesting(&prompts);
    table->selectRow(rowForId(*table, deleteTarget.id));
    QPushButton* deleteButton = buttonWithText(page, QStringLiteral("Delete"));
    QVERIFY(deleteButton);
    deleteButton->click();
    QCOMPARE(prompts.confirmations.size(), 1);
    QCOMPARE(table->model()->rowCount(), 1);

    QPushButton* addButton = buttonWithText(page, QStringLiteral("Add"));
    QVERIFY(addButton);
    addButton->click();
    const int draftRow = table->model()->rowCount() - 1;
    setStaffDirectoryCellText(*table, draftRow, 0, QStringLiteral("Draft"));
    setStaffDirectoryCellText(*table, draftRow, 1, QStringLiteral("\uB77C"));
    QCOMPARE(staffDirectoryCellData(*table, draftRow, 0, IdRole).toInt(), 0);
    QVERIFY(page.hasUnsavedChanges());

    QSignalSpy saved(&page, &StaffDirectoryPage::directorySaved);
    page.discardChanges();
    QVERIFY(!page.hasUnsavedChanges());
    QCOMPARE(table->model()->rowCount(), original->size());
    QCOMPARE(rowForId(*table, updateTarget.id) >= 0, true);
    QCOMPARE(rowForId(*table, deleteTarget.id) >= 0, true);
    QCOMPARE(staffDirectoryCellText(
        *table,
        rowForId(*table, updateTarget.id),
        0),
        QStringLiteral("Update Target"));
    QCOMPARE(saved.count(), 0);

    const auto unchanged = services.databaseSession()
        ->gsTeamRepository()->getAll();
    QVERIFY(unchanged);
    QVERIFY(sameMembers(*unchanged, *original));
}

void StaffDirectoryGsTeamSaveTests::
refreshWhileDirtyPreservesEditsAndPendingDeletes()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    QVERIFY(seedDirectory(services));
    const auto original = services.databaseSession()
        ->gsTeamRepository()->getAll();
    QVERIFY(original);
    const GsTeamMember updateTarget =
        memberNamed(*original, QStringLiteral("Update Target"));
    const GsTeamMember deleteTarget =
        memberNamed(*original, QStringLiteral("Delete Target"));
    QVERIFY(updateTarget.id > 0);
    QVERIFY(deleteTarget.id > 0);

    StaffDirectoryPage page(&services, StaffDirectoryKind::GsTeam);
    page.setSaveMode(SaveMode::Manual);
    QVERIFY(page.loadDirectory());
    auto* table = page.findChild<QTableView*>(QStringLiteral("gsTeamTable"));
    QVERIFY(table);
    const int updateRow = rowForId(*table, updateTarget.id);
    const int deleteRow = rowForId(*table, deleteTarget.id);
    QVERIFY(updateRow >= 0);
    QVERIFY(deleteRow >= 0);
    QVERIFY(setStaffDirectoryCellText(
        *table, updateRow, 0, QStringLiteral("Edited Before Refresh")));

    FakeUserPromptService prompts;
    prompts.scriptedChoices.enqueue(PromptChoice::Destructive);
    DialogServices::setUserPromptServiceForTesting(&prompts);
    table->selectRow(deleteRow);
    QPushButton* deleteButton = buttonWithText(page, QStringLiteral("Delete"));
    QVERIFY(deleteButton);
    deleteButton->click();
    QCOMPARE(prompts.confirmations.size(), 1);
    QCOMPARE(table->model()->rowCount(), 1);
    QVERIFY(page.hasUnsavedChanges());

    page.refresh();

    QVERIFY(page.hasUnsavedChanges());
    QCOMPARE(table->model()->rowCount(), 1);
    const int editedRow = rowForId(*table, updateTarget.id);
    QVERIFY(editedRow >= 0);
    QCOMPARE(rowForId(*table, deleteTarget.id), -1);
    QCOMPARE(staffDirectoryCellText(*table, editedRow, 0),
        QStringLiteral("Edited Before Refresh"));
    const auto beforeSave = services.databaseSession()
        ->gsTeamRepository()->getAll();
    QVERIFY(beforeSave);
    QVERIFY(sameMembers(*beforeSave, *original));

    QSignalSpy saved(&page, &StaffDirectoryPage::directorySaved);
    QVERIFY(page.saveChanges());
    QCOMPARE(saved.count(), 1);
    QVERIFY(!page.hasUnsavedChanges());
    const auto persisted = services.databaseSession()
        ->gsTeamRepository()->getAll();
    QVERIFY(persisted);
    QCOMPARE(persisted->size(), 1);
    const GsTeamMember updated =
        memberNamed(*persisted, QStringLiteral("Edited Before Refresh"));
    QCOMPARE(updated.id, updateTarget.id);
    QVERIFY(memberNamed(*persisted, QStringLiteral("Delete Target")).id <= 0);
}

void StaffDirectoryGsTeamSaveTests::
successfulWriteWithFailedReloadRequiresReloadBeforeEditingOrSaving()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));

    StaffDirectoryPage page(&services, StaffDirectoryKind::GsTeam);
    page.setSaveMode(SaveMode::Automatic);
    QVERIFY(page.loadDirectory());
    auto* table = page.findChild<QTableView*>(QStringLiteral("gsTeamTable"));
    QVERIFY(table);
    QPushButton* addButton = buttonWithText(page, QStringLiteral("Add"));
    QVERIFY(addButton);

    addButton->click();
    QCOMPARE(table->model()->rowCount(), 1);
    QVERIFY(setStaffDirectoryCellText(
        *table, 0, 0, QStringLiteral("Committed Member")));
    QVERIFY(setStaffDirectoryCellText(
        *table, 0, 1, QStringLiteral("\uAE40\uD558\uB298")));
    QVERIFY(setStaffDirectoryCellText(*table, 0, 2, QStringLiteral("M1")));
    QVERIFY(setStaffDirectoryCellText(
        *table, 0, 3, QStringLiteral("010-5555-1234")));
    QVERIFY(page.hasUnsavedChanges());

    page.failNextPostSaveDirectoryReadForTesting();
    FakeUserPromptService prompts;
    DialogServices::setUserPromptServiceForTesting(&prompts);
    QSignalSpy saved(&page, &StaffDirectoryPage::directorySaved);

    QVERIFY(page.saveChanges());
    QCOMPARE(saved.count(), 1);
    QVERIFY(!page.hasUnsavedChanges());
    QCOMPARE(table->model()->rowCount(), 1);
    QVERIFY(staffDirectoryCellData(*table, 0, 0, IdRole).isNull());

    const auto committed = services.databaseSession()
        ->gsTeamRepository()->getAll();
    QVERIFY(committed);
    QCOMPARE(committed->size(), 1);
    const GsTeamMember persisted =
        memberNamed(*committed, QStringLiteral("Committed Member"));
    QVERIFY(persisted.id > 0);
    QCOMPARE(prompts.messages.size(), 1);
    QCOMPARE(prompts.messages.first().severity, PromptSeverity::Warning);
    QCOMPARE(prompts.messages.first().title, QStringLiteral("Save Directory"));
    QVERIFY(prompts.messages.first().message.contains(
        QStringLiteral("GS Team directory was saved")));
    QVERIFY(prompts.messages.first().message.contains(
        QStringLiteral("refresh failed")));

    QVERIFY(!page.saveChanges());
    page.saveData();
    QTest::qWait(900);
    QCOMPARE(saved.count(), 1);
    const auto afterBlockedSaves = services.databaseSession()
        ->gsTeamRepository()->getAll();
    QVERIFY(afterBlockedSaves);
    QCOMPARE(afterBlockedSaves->size(), 1);

    page.setSaveMode(SaveMode::Manual);
    QPushButton* saveButton = buttonWithText(page, QStringLiteral("Save Changes"));
    QPushButton* deleteButton = buttonWithText(page, QStringLiteral("Delete"));
    QVERIFY(saveButton);
    QVERIFY(deleteButton);
    QVERIFY(!addButton->isEnabled());
    QVERIFY(!saveButton->isEnabled());
    table->selectRow(0);
    QVERIFY(!deleteButton->isEnabled());
    QVERIFY(!table->model()->flags(table->model()->index(0, 0))
        .testFlag(Qt::ItemIsEditable));
    QVERIFY(!setStaffDirectoryCellText(
        *table, 0, 0, QStringLiteral("Blocked Edit")));
    addButton->click();
    QCOMPARE(table->model()->rowCount(), 1);
    QCOMPARE(staffDirectoryCellText(*table, 0, 0),
        QStringLiteral("Committed Member"));

    QVERIFY(page.loadDirectory());
    QCOMPARE(prompts.messages.size(), 1);
    QCOMPARE(rowForId(*table, persisted.id), 0);
    QVERIFY(table->model()->flags(table->model()->index(0, 0))
        .testFlag(Qt::ItemIsEditable));
    QVERIFY(addButton->isEnabled());
    QVERIFY(!saveButton->isEnabled());

    QVERIFY(setStaffDirectoryCellText(
        *table, 0, 0, QStringLiteral("Recovered Member")));
    QVERIFY(page.hasUnsavedChanges());
    QVERIFY(saveButton->isEnabled());
    QVERIFY(page.saveChanges());
    QCOMPARE(saved.count(), 2);
    const auto recovered = services.databaseSession()
        ->gsTeamRepository()->getAll();
    QVERIFY(recovered);
    QCOMPARE(recovered->size(), 1);
    const GsTeamMember updated =
        memberNamed(*recovered, QStringLiteral("Recovered Member"));
    QCOMPARE(updated.id, persisted.id);
}

QTEST_MAIN(StaffDirectoryGsTeamSaveTests)

#include "staff_directory_gs_team_save_tests.moc"
