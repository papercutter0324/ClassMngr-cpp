#include "core/application_services.h"
#include "data/database/database_session.h"
#include "data/repositories/gs_team_repository.h"
#include "domain/models/gs_team_member.h"
#include "features/teacher/ui/staff_directory_page.h"
#include "staff_directory_test_utils.h"
#include "fakes/fake_user_prompt_service.h"
#include "ui/shared/dialogs/user_prompt_service.h"

#include <QPushButton>
#include <QSignalSpy>
#include <QTableView>
#include <QTemporaryDir>
#include <QUuid>
#include <QtTest/QtTest>

namespace
{

constexpr int IdRole = Qt::UserRole + 1;

QString databasePath(QTemporaryDir& directory)
{
    return directory.filePath(
        QStringLiteral("staff-directory-gs-team-save-parity-%1.tps").arg(
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

bool seedDirectory(ApplicationServices& services)
{
    return static_cast<bool>(services.databaseSession()
        ->gsTeamRepository()->saveDirectory(
            {
                member(QStringLiteral("Parity Update"),
                    QStringLiteral("\uC218\uC815"), QStringLiteral("M1"),
                    QStringLiteral("010-4000-0001"), QStringLiteral("01-02")),
                member(QStringLiteral("Parity Delete"),
                    QStringLiteral("\uC0AD\uC81C"), QStringLiteral("M2"),
                    QStringLiteral("010-4000-0002"), QStringLiteral("03-04"))
            },
            {}
            ));
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

int rowForName(const QTableView& table, const QString& name)
{
    for (int row = 0; row < table.model()->rowCount(); ++row)
    {
        if (staffDirectoryCellText(table, row, 0) == name)
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

}

class StaffDirectoryGsTeamSaveParityTests final : public QObject
{
    Q_OBJECT

private slots:
    void cleanup();
    void publicPageSaveMatchesPinnedBaselineBehavior();
};

void StaffDirectoryGsTeamSaveParityTests::cleanup()
{
    DialogServices::setUserPromptServiceForTesting(nullptr);
}

void StaffDirectoryGsTeamSaveParityTests::
publicPageSaveMatchesPinnedBaselineBehavior()
{
    // Compare public-page behavior against the pinned legacy baseline
    // 48fc5c5cc7dee78d82f8bf5f1bf8b51725575b99.
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    QVERIFY(seedDirectory(services));
    const auto before = services.databaseSession()
        ->gsTeamRepository()->getAll();
    QVERIFY(before);
    const GsTeamMember updateOriginal =
        memberNamed(*before, QStringLiteral("Parity Update"));
    const GsTeamMember deleteOriginal =
        memberNamed(*before, QStringLiteral("Parity Delete"));
    QVERIFY(updateOriginal.id > 0);
    QVERIFY(deleteOriginal.id > 0);

    StaffDirectoryPage page(&services, StaffDirectoryKind::GsTeam);
    page.setSaveMode(SaveMode::Manual);
    QVERIFY(page.loadDirectory());
    auto* table = page.findChild<QTableView*>(QStringLiteral("gsTeamTable"));
    QVERIFY(table);
    QCOMPARE(table->model()->columnCount(), 5);

    QSignalSpy saved(&page, &StaffDirectoryPage::directorySaved);
    const int updateRow = rowForName(*table, QStringLiteral("Parity Update"));
    const int deleteRow = rowForName(*table, QStringLiteral("Parity Delete"));
    QVERIFY(updateRow >= 0);
    QVERIFY(deleteRow >= 0);
    QCOMPARE(staffDirectoryCellData(*table, updateRow, 0, IdRole).toInt(), updateOriginal.id);
    setStaffDirectoryCellText(*table, updateRow, 0, QStringLiteral("  Renamed   Member  "));
    setStaffDirectoryCellText(*table, updateRow, 1, QStringLiteral("\uC218\uC815 \uB2F4\uB2F9"));
    setStaffDirectoryCellText(*table, updateRow, 2, QStringLiteral(" Team Leader "));
    setStaffDirectoryCellText(*table, updateRow, 3, QStringLiteral("010-4444-5555"));
    setStaffDirectoryCellText(*table, updateRow, 4, QStringLiteral("02-29"));

    FakeUserPromptService prompts;
    prompts.scriptedChoices.enqueue(PromptChoice::Destructive);
    DialogServices::setUserPromptServiceForTesting(&prompts);
    table->selectRow(deleteRow);
    QPushButton* deleteButton = buttonWithText(page, QStringLiteral("Delete"));
    QVERIFY(deleteButton);
    deleteButton->click();

    QPushButton* addButton = buttonWithText(page, QStringLiteral("Add"));
    QVERIFY(addButton);
    addButton->click();
    const int insertedRow = table->model()->rowCount() - 1;
    setStaffDirectoryCellText(*table, insertedRow, 0, QString());
    setStaffDirectoryCellText(*table, insertedRow, 1, QStringLiteral("\uC2E0\uADDC \uAD6C\uC131\uC6D0"));
    setStaffDirectoryCellText(*table, insertedRow, 2, QStringLiteral("M3"));
    setStaffDirectoryCellText(*table, insertedRow, 3, QStringLiteral("010-5555-6666"));
    setStaffDirectoryCellText(*table, insertedRow, 4, QStringLiteral(""));

    QVERIFY(page.hasUnsavedChanges());
    QVERIFY(page.saveChanges());
    QCOMPARE(saved.count(), 1);
    QVERIFY(!page.hasUnsavedChanges());
    QCOMPARE(prompts.messages.size(), 0);

    const auto persisted = services.databaseSession()
        ->gsTeamRepository()->getAll();
    QVERIFY(persisted);
    QCOMPARE(persisted->size(), 2);
    QVERIFY(memberNamed(*persisted, QStringLiteral("Parity Delete")).id <= 0);

    const GsTeamMember updated =
        memberNamed(*persisted, QStringLiteral("Renamed Member"));
    QCOMPARE(updated.id, updateOriginal.id);
    QCOMPARE(updated.koreanName, QStringLiteral("\uC218\uC815 \uB2F4\uB2F9"));
    QCOMPARE(updated.position, QStringLiteral("Team Leader"));
    QCOMPARE(updated.phoneNumber, QStringLiteral("010-4444-5555"));
    QCOMPARE(updated.birthday, QStringLiteral("02-29"));

    const GsTeamMember inserted = memberNamed(*persisted, QString());
    QVERIFY(inserted.id > 0);
    QCOMPARE(inserted.koreanName,
        QStringLiteral("\uC2E0\uADDC \uAD6C\uC131\uC6D0"));
    QCOMPARE(inserted.position, QStringLiteral("M3"));
    QCOMPARE(inserted.phoneNumber, QStringLiteral("010-5555-6666"));
    QCOMPARE(inserted.birthday, QString());

    QVERIFY(page.saveChanges());
    QCOMPARE(saved.count(), 1);
}

QTEST_MAIN(StaffDirectoryGsTeamSaveParityTests)

#include "staff_directory_gs_team_save_parity_tests.moc"
