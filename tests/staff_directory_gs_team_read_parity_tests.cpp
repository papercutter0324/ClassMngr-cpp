#include "app/controllers/navigation_controller.h"
#include "core/application_services.h"
#include "core/resource_packs/resource_pack_manager.h"
#include "data/data_service.h"
#include "data/database/database_session.h"
#include "domain/models/gs_team_member.h"
#include "features/teacher/ui/staff_directory_page.h"
#include "staff_directory_test_utils.h"
#include "fakes/fake_user_prompt_service.h"
#include "ui/shared/dialogs/user_prompt_service.h"
#include "ui/shared/pages/pagemanager.h"
#include "ui/shared/widgets/sidebar/sidebar.h"

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
        QStringLiteral("gs-team-directory-parity-%1.tps").arg(
            QUuid::createUuid().toString(QUuid::WithoutBraces)
            )
        );
}

QList<GsTeamMember> fixtureMembers()
{
    GsTeamMember beta;
    beta.name = QStringLiteral("Beta");
    beta.koreanName = QStringLiteral("\uB098");
    beta.position = QStringLiteral("M1");
    beta.phoneNumber = QStringLiteral("010-4000-0003");
    beta.birthday = QStringLiteral("03-14");

    GsTeamMember manager;
    manager.name = QStringLiteral("Alex");
    manager.koreanName = QStringLiteral("\uC54C\uB809\uC2A4");
    manager.position = QStringLiteral("Branch Manager");
    manager.phoneNumber = QStringLiteral("010-4000-0001");
    manager.birthday = QStringLiteral("01-02");

    GsTeamMember koreanOnly;
    koreanOnly.name = QStringLiteral("");
    koreanOnly.koreanName = QStringLiteral("\uAE40\uD558\uB298");
    koreanOnly.position = QStringLiteral("M1");
    koreanOnly.phoneNumber = QStringLiteral("010-4000-0004");
    koreanOnly.birthday = QStringLiteral("04-15");

    GsTeamMember alpha;
    alpha.name = QStringLiteral("Alpha");
    alpha.koreanName = QStringLiteral("\uAC00");
    alpha.position = QStringLiteral("M1");
    alpha.phoneNumber = QStringLiteral("010-4000-0002");
    alpha.birthday = QStringLiteral("02-03");

    return {beta, manager, koreanOnly, alpha};
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

}

class StaffDirectoryGsTeamReadParityTests final : public QObject
{
    Q_OBJECT

private slots:
    void cleanup();
    void routeDisplaysCommonRowsAndKeepsPreviousPageOnReadFailure();
};

void StaffDirectoryGsTeamReadParityTests::cleanup()
{
    DialogServices::setUserPromptServiceForTesting(nullptr);
}

void StaffDirectoryGsTeamReadParityTests::
routeDisplaysCommonRowsAndKeepsPreviousPageOnReadFailure()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    QVERIFY(services.dataService());
    QVERIFY(services.dataService()->saveGsTeamDirectory(fixtureMembers(), {}));

    PageManager pages;
    pages.initialize(&services, false);
    pages.showPage(PageType::TeacherInfo);
    QVERIFY(pages.isCurrentPage(PageType::TeacherInfo));
    ResourcePackManager resources(
        directory.filePath(QStringLiteral("resources")),
        directory.filePath(QStringLiteral("baseline")));
    Sidebar sidebar;
    NavigationController navigation(&services, &sidebar, &pages, resources);
    FakeUserPromptService prompts;
    DialogServices::setUserPromptServiceForTesting(&prompts);

    navigation.handleNavigation(gsTeamRoute());
    QVERIFY(pages.isCurrentPage(PageType::GsTeam));
    StaffDirectoryPage* const page = pages.gsTeamPage();
    QVERIFY(page);
    auto* table = page->findChild<QTableView*>(QStringLiteral("gsTeamTable"));
    QVERIFY(table);
    QCOMPARE(table->model()->columnCount(), 5);
    QCOMPARE(table->model()->rowCount(), 4);
    QCOMPARE(staffDirectoryCellText(*table, 0, 0), QStringLiteral("Alex"));
    QCOMPARE(staffDirectoryCellText(*table, 0, 1),
        QStringLiteral("\uC54C\uB809\uC2A4"));
    QCOMPARE(staffDirectoryCellText(*table, 0, 2), QStringLiteral("Branch Manager"));
    QCOMPARE(staffDirectoryCellText(*table, 0, 3), QStringLiteral("010-4000-0001"));
    QCOMPARE(staffDirectoryCellText(*table, 0, 4), QStringLiteral("01-02"));
    QVERIFY(staffDirectoryCellData(*table, 0, 0, IdRole).toInt() > 0);
    QCOMPARE(staffDirectoryCellText(*table, 1, 0), QStringLiteral("Alpha"));
    QCOMPARE(staffDirectoryCellText(*table, 2, 0), QStringLiteral("Beta"));
    QCOMPARE(staffDirectoryCellText(*table, 3, 0), QString());

    pages.showPage(PageType::TeacherInfo);
    QVERIFY(pages.isCurrentPage(PageType::TeacherInfo));
    QSqlQuery drop(services.dataService()->databaseSession()->database());
    QVERIFY2(drop.exec(QStringLiteral("DROP TABLE gs_team")),
        qPrintable(drop.lastError().text()));
    navigation.handleNavigation(gsTeamRoute());

    QVERIFY(pages.isCurrentPage(PageType::TeacherInfo));
    QCOMPARE(prompts.messages.size(), 1);
    QCOMPARE(prompts.messages.first().severity, PromptSeverity::Warning);
    QCOMPARE(prompts.messages.first().title, QStringLiteral("Load Directory"));
    QVERIFY(!prompts.messages.first().message.isEmpty());
}

QTEST_MAIN(StaffDirectoryGsTeamReadParityTests)

#include "staff_directory_gs_team_read_parity_tests.moc"
