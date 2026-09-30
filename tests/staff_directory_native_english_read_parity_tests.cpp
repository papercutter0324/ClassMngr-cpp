#include "app/controllers/navigation_controller.h"
#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "core/resource_packs/resource_pack_manager.h"
#include "data/data_service.h"
#include "data/database/database_session.h"
#include "data/repositories/native_english_teacher_repository.h"
#include "domain/models/native_english_teacher.h"
#include "features/teacher/ui/staff_directory_page.h"
#include "fakes/fake_user_prompt_service.h"
#include "ui/shared/dialogs/user_prompt_service.h"
#include "ui/shared/pages/pagemanager.h"
#include "ui/shared/widgets/sidebar/sidebar.h"

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
        QStringLiteral("native-english-directory-parity-%1.tps").arg(
            QUuid::createUuid().toString(QUuid::WithoutBraces)
            )
        );
}

QList<NativeEnglishTeacher> fixtureTeachers()
{
    NativeEnglishTeacher zulu;
    zulu.name = QStringLiteral("Zulu");
    zulu.position = QStringLiteral("NET");
    zulu.phoneNumber = QStringLiteral("010-3000-0002");
    zulu.email = QStringLiteral("zulu@example.test");
    zulu.birthday = QStringLiteral("12-31");
    zulu.nationality = QStringLiteral("New Zealand");

    NativeEnglishTeacher coordinator;
    coordinator.name = QStringLiteral("Coordinator");
    coordinator.position = QStringLiteral("Co-ordinator");
    coordinator.phoneNumber = QStringLiteral("010-3000-0001");
    coordinator.email = QStringLiteral("coordinator@example.test");
    coordinator.birthday = QStringLiteral("01-02");
    coordinator.nationality = QStringLiteral("Korea");

    return {zulu, coordinator};
}

NavigationData nativeEnglishDirectoryRoute()
{
    return {
        .path = {QStringLiteral("Teacher"), QStringLiteral("Native English Teachers")},
        .keys = {QStringLiteral("native_english_teachers")},
        .routeKey = QStringLiteral("native_english_teachers"),
        .type = NodeType::Page
    };
}

}

class StaffDirectoryNativeEnglishReadParityTests final : public QObject
{
    Q_OBJECT

private slots:
    void cleanup();
    void routeDisplaysRepositoryRowsAndStaysPutWhenLoadFails();
};

void StaffDirectoryNativeEnglishReadParityTests::cleanup()
{
    DialogServices::setUserPromptServiceForTesting(nullptr);
}

void StaffDirectoryNativeEnglishReadParityTests::
routeDisplaysRepositoryRowsAndStaysPutWhenLoadFails()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    const Status seeded = services.dataService()->databaseSession()
        ->nativeEnglishTeacherRepository()->saveDirectory(
            fixtureTeachers(), {});
    QVERIFY(seeded);

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

    navigation.handleNavigation(nativeEnglishDirectoryRoute());
    QVERIFY(pages.isCurrentPage(PageType::NativeEnglishTeachers));
    StaffDirectoryPage* const page = pages.nativeEnglishTeachersPage();
    QVERIFY(page);
    auto* table = page->findChild<QTableWidget*>(
        QStringLiteral("nativeEnglishTeachersTable"));
    QVERIFY(table);
    QCOMPARE(table->columnCount(), 6);
    QCOMPARE(table->rowCount(), 2);
    QCOMPARE(table->item(0, 0)->text(), QStringLiteral("Coordinator"));
    QCOMPARE(table->item(0, 1)->text(), QStringLiteral("Co-ordinator"));
    QCOMPARE(table->item(0, 2)->text(), QStringLiteral("010-3000-0001"));
    QCOMPARE(table->item(0, 3)->text(),
        QStringLiteral("coordinator@example.test"));
    QCOMPARE(table->item(0, 4)->text(), QStringLiteral("01-02"));
    QCOMPARE(table->item(0, 5)->text(), QStringLiteral("Korea"));
    QVERIFY(table->item(0, 0)->data(NativeEnglishIdRole).toInt() > 0);
    QCOMPARE(table->item(1, 0)->text(), QStringLiteral("Zulu"));

    pages.showPage(PageType::TeacherInfo);
    QVERIFY(pages.isCurrentPage(PageType::TeacherInfo));
    QSqlQuery drop(services.dataService()->databaseSession()->database());
    QVERIFY2(drop.exec(QStringLiteral("DROP TABLE native_english_teachers")),
        qPrintable(drop.lastError().text()));
    navigation.handleNavigation(nativeEnglishDirectoryRoute());

    QVERIFY(pages.isCurrentPage(PageType::TeacherInfo));
    QCOMPARE(prompts.messages.size(), 1);
    QCOMPARE(prompts.messages.first().severity, PromptSeverity::Warning);
    QCOMPARE(prompts.messages.first().title, QStringLiteral("Load Directory"));
    QVERIFY(!prompts.messages.first().message.isEmpty());
}

QTEST_MAIN(StaffDirectoryNativeEnglishReadParityTests)

#include "staff_directory_native_english_read_parity_tests.moc"
