#include "app/controllers/navigation_controller.h"
#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "core/resource_packs/resource_pack_manager.h"
#include "data/database/database_session.h"
#include "data/repositories/gs_team_repository.h"
#include "data/repositories/native_english_teacher_repository.h"
#include "domain/models/gs_team_member.h"
#include "features/teacher/ui/staff_directory_page.h"
#include "staff_directory_test_utils.h"
#include "fakes/fake_user_prompt_service.h"
#include "ui/shared/dialogs/user_prompt_service.h"
#include "ui/shared/pages/pagemanager.h"
#include "ui/shared/widgets/sidebar/sidebar.h"

#include <QPushButton>
#include <QSqlError>
#include <QSqlQuery>
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
        QStringLiteral("staff-directory-gs-team-read-%1.tps").arg(
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
        member(QStringLiteral("Beta"), QStringLiteral("\uB098"),
            QStringLiteral("M1"), QStringLiteral("010-2000-0003"),
            QStringLiteral("03-14")),
        member(QStringLiteral("Alex"), QStringLiteral("\uC54C\uB809\uC2A4"),
            QStringLiteral("Branch Manager"), QStringLiteral("010-2000-0001"),
            QStringLiteral("01-02")),
        member(QStringLiteral(""), QStringLiteral("\uAE40\uD558\uB298"), QStringLiteral("M1"),
            QStringLiteral("010-2000-0004"), QStringLiteral("04-15")),
        member(QStringLiteral("Alpha"), QStringLiteral("\uAC00"),
            QStringLiteral("M1"), QStringLiteral("010-2000-0002"),
            QStringLiteral("02-03"))
    };
}

bool seedGsTeam(ApplicationServices& services)
{
    return static_cast<bool>(services.databaseSession()
        ->gsTeamRepository()->saveDirectory(fixtureMembers(), {}));
}

NavigationData gsTeamRoute()
{
    return {
        .path = {QStringLiteral("Teacher"), QStringLiteral("GS Team")},
        .keys = {QStringLiteral("gs_team")},
        .routeKey = QStringLiteral("gs_team"),
        .type = NodeType::Page
    };
}

class ClosingDatabasePrompt final : public IUserPromptService
{
public:
    explicit ClosingDatabasePrompt(ApplicationServices& services)
        : m_services(services)
    {
    }

    void showMessage(const PromptRequest& request) override
    {
        messages.append(request);
    }

    void showMessageAsync(const PromptRequest& request) override
    {
        asynchronousMessages.append(request);
    }

    [[nodiscard]] PromptChoice confirm(const PromptRequest& request) override
    {
        confirmations.append(request);
        return PromptChoice::Rejected;
    }

    [[nodiscard]] UnsavedChangesChoice confirmUnsavedChanges(
        const UnsavedChangesRequest& request
        ) override
    {
        unsavedChangesConfirmations.append(request);
        wasAvailableWhenPromptOpened =
            m_services.teacherService()->isAvailable();
        m_services.closeDatabase();
        return UnsavedChangesChoice::Discard;
    }

    [[nodiscard]] QString chooseAction(
        const ActionPromptRequest& request
        ) override
    {
        actionPrompts.append(request);
        return {};
    }

    QVector<PromptRequest> messages;
    QVector<PromptRequest> asynchronousMessages;
    QVector<PromptRequest> confirmations;
    QVector<UnsavedChangesRequest> unsavedChangesConfirmations;
    QVector<ActionPromptRequest> actionPrompts;
    bool wasAvailableWhenPromptOpened = false;

private:
    ApplicationServices& m_services;
};

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

}

class StaffDirectoryGsTeamReadTests final : public QObject
{
    Q_OBJECT

private slots:
    void cleanup();
    void pageMapsFiveFieldsInRepositoryOrderWithIntegerIds();
    void successfulReloadClearsDirtyAndPendingDeletedState();
    void unavailableSessionClearsSilently();
    void repositoryFailureWarnsAndRetainsDirtyState();
    void routeUsesPostConfirmationSessionAndDoesNotFallbackOrShow();
};

void StaffDirectoryGsTeamReadTests::cleanup()
{
    DialogServices::setUserPromptServiceForTesting(nullptr);
}

void StaffDirectoryGsTeamReadTests::
pageMapsFiveFieldsInRepositoryOrderWithIntegerIds()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    QVERIFY(seedGsTeam(services));
    const auto persisted = services.databaseSession()
        ->gsTeamRepository()->getAll();
    QVERIFY(persisted);

    StaffDirectoryPage page(&services, StaffDirectoryKind::GsTeam);
    QVERIFY(page.loadDirectory());
    auto* table = page.findChild<QTableView*>(QStringLiteral("gsTeamTable"));
    QVERIFY(table);
    QCOMPARE(table->model()->columnCount(), 5);
    QCOMPARE(table->model()->rowCount(), persisted->size());

    for (int row = 0; row < persisted->size(); ++row)
    {
        const GsTeamMember& expected = persisted->at(row);
        QCOMPARE(staffDirectoryCellText(*table, row, 0), expected.name);
        QCOMPARE(staffDirectoryCellData(*table, row, 0, IdRole).toInt(), expected.id);
        QCOMPARE(staffDirectoryCellText(*table, row, 1), expected.koreanName);
        QCOMPARE(staffDirectoryCellText(*table, row, 2), expected.position);
        QCOMPARE(staffDirectoryCellText(*table, row, 3), expected.phoneNumber);
        QCOMPARE(staffDirectoryCellText(*table, row, 4), expected.birthday);
    }
    QCOMPARE(staffDirectoryCellText(*table, 0, 0), QStringLiteral("Alex"));
    QCOMPARE(staffDirectoryCellText(*table, 0, 1), QStringLiteral("\uC54C\uB809\uC2A4"));
    QCOMPARE(staffDirectoryCellText(*table, 1, 0), QStringLiteral("Alpha"));
    QCOMPARE(staffDirectoryCellText(*table, 2, 0), QStringLiteral("Beta"));
    QCOMPARE(staffDirectoryCellText(*table, 3, 0), QString());
    QVERIFY(staffDirectoryCellData(*table, 0, 0, IdRole).toInt() > 0);
    QVERIFY(!page.hasUnsavedChanges());
}

void StaffDirectoryGsTeamReadTests::
successfulReloadClearsDirtyAndPendingDeletedState()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    QVERIFY(seedGsTeam(services));

    StaffDirectoryPage page(&services, StaffDirectoryKind::GsTeam);
    page.setSaveMode(SaveMode::Manual);
    QVERIFY(page.loadDirectory());
    auto* table = page.findChild<QTableView*>(QStringLiteral("gsTeamTable"));
    QVERIFY(table);
    QCOMPARE(table->model()->rowCount(), 4);

    FakeUserPromptService prompts;
    prompts.scriptedChoices.enqueue(PromptChoice::Destructive);
    DialogServices::setUserPromptServiceForTesting(&prompts);
    table->selectRow(0);
    QPushButton* deleteButton = buttonWithText(page, QStringLiteral("Delete"));
    QVERIFY(deleteButton);
    deleteButton->click();
    QCOMPARE(table->model()->rowCount(), 3);
    QVERIFY(page.hasUnsavedChanges());

    QVERIFY(page.loadDirectory());
    QCOMPARE(table->model()->rowCount(), 4);
    QVERIFY(!page.hasUnsavedChanges());

    QSignalSpy saved(&page, &StaffDirectoryPage::directorySaved);
    setStaffDirectoryCellText(*table, 0, 0, QStringLiteral("Alex Updated"));
    QVERIFY(page.hasUnsavedChanges());
    QVERIFY(page.saveChanges());
    QCOMPARE(saved.size(), 1);
    QVERIFY(!page.hasUnsavedChanges());

    const auto persisted = services.databaseSession()
        ->gsTeamRepository()->getAll();
    QVERIFY(persisted);
    QCOMPARE(persisted->size(), 4);
}

void StaffDirectoryGsTeamReadTests::unavailableSessionClearsSilently()
{
    ApplicationServices services;
    StaffDirectoryPage page(&services, StaffDirectoryKind::GsTeam);
    page.setSaveMode(SaveMode::Manual);
    auto* table = page.findChild<QTableView*>(QStringLiteral("gsTeamTable"));
    QVERIFY(table);
    QVERIFY(table->model()->insertRows(0, 1));
    QVERIFY(setStaffDirectoryCellText(
        *table, 0, 0, QStringLiteral("Stale row")));
    QVERIFY(page.hasUnsavedChanges());

    FakeUserPromptService prompts;
    DialogServices::setUserPromptServiceForTesting(&prompts);
    QVERIFY(!page.loadDirectory());

    QCOMPARE(table->model()->rowCount(), 0);
    QVERIFY(!page.hasUnsavedChanges());
    QCOMPARE(prompts.messages.size(), 0);
}

void StaffDirectoryGsTeamReadTests::repositoryFailureWarnsAndRetainsDirtyState()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    QVERIFY(seedGsTeam(services));

    StaffDirectoryPage page(&services, StaffDirectoryKind::GsTeam);
    page.setSaveMode(SaveMode::Manual);
    QVERIFY(page.loadDirectory());
    auto* table = page.findChild<QTableView*>(QStringLiteral("gsTeamTable"));
    QVERIFY(table);
    setStaffDirectoryCellText(*table, 0, 0, QStringLiteral("Edited locally"));
    QVERIFY(page.hasUnsavedChanges());

    QSqlQuery query(services.databaseSession()->database());
    QVERIFY2(query.exec(QStringLiteral("DROP TABLE gs_team")),
        qPrintable(query.lastError().text()));

    FakeUserPromptService prompts;
    DialogServices::setUserPromptServiceForTesting(&prompts);
    QVERIFY(!page.loadDirectory());

    QCOMPARE(table->model()->rowCount(), 0);
    QVERIFY(page.hasUnsavedChanges());
    QCOMPARE(prompts.messages.size(), 1);
    QCOMPARE(prompts.messages.first().severity, PromptSeverity::Warning);
    QCOMPARE(prompts.messages.first().title, QStringLiteral("Load Directory"));
    QVERIFY(!prompts.messages.first().message.isEmpty());
}

void StaffDirectoryGsTeamReadTests::
routeUsesPostConfirmationSessionAndDoesNotFallbackOrShow()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    QVERIFY(seedGsTeam(services));

    PageManager pages;
    pages.initialize(&services, false);
    StaffDirectoryPage* nativePage = pages.ensureNativeEnglishTeachersPage();
    QVERIFY(nativePage);
    QVERIFY(nativePage->loadDirectory());
    pages.showPage(PageType::NativeEnglishTeachers);
    nativePage->setSaveMode(SaveMode::Manual);
    QPushButton* addButton = buttonWithText(*nativePage, QStringLiteral("Add"));
    QVERIFY(addButton);
    addButton->click();
    QVERIFY(nativePage->hasUnsavedChanges());
    QVERIFY(pages.isCurrentPage(PageType::NativeEnglishTeachers));

    ResourcePackManager resources(
        directory.filePath(QStringLiteral("resources")),
        directory.filePath(QStringLiteral("baseline")));
    Sidebar sidebar;
    NavigationController navigation(&services, &sidebar, &pages, resources);
    ClosingDatabasePrompt prompts(services);
    DialogServices::setUserPromptServiceForTesting(&prompts);

    navigation.handleNavigation(gsTeamRoute());

    QCOMPARE(prompts.unsavedChangesConfirmations.size(), 1);
    QVERIFY(prompts.wasAvailableWhenPromptOpened);
    QVERIFY(!services.hasOpenDatabase());
    QVERIFY(pages.isCurrentPage(PageType::NativeEnglishTeachers));
    QVERIFY(!pages.isCurrentPage(PageType::GsTeam));
    QCOMPARE(prompts.messages.size(), 0);
    StaffDirectoryPage* gsTeamPage = pages.gsTeamPage();
    QVERIFY(gsTeamPage);
    auto* table = gsTeamPage->findChild<QTableView*>(
        QStringLiteral("gsTeamTable"));
    QVERIFY(table);
    QCOMPARE(table->model()->rowCount(), 0);
    QVERIFY(!gsTeamPage->hasUnsavedChanges());
}

QTEST_MAIN(StaffDirectoryGsTeamReadTests)

#include "staff_directory_gs_team_read_tests.moc"
