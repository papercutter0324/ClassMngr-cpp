#include "app/controllers/navigation_controller.h"
#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "core/resource_packs/resource_pack_manager.h"
#include "data/data_service.h"
#include "data/database/database_session.h"
#include "data/repositories/teacher_repository.h"
#include "domain/models/teacher.h"
#include "features/teacher/ui/teacher_info_page.h"
#include "ui/shared/pages/page_header.h"
#include "ui/shared/pages/pagemanager.h"
#include "ui/shared/widgets/sidebar/sidebar.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QUuid>
#include <QtTest/QtTest>

namespace
{

QString databasePath(QTemporaryDir& directory)
{
    return directory.filePath(
        QStringLiteral("navigation-teacher-parity-%1.tps").arg(
            QUuid::createUuid().toString(QUuid::WithoutBraces)
            )
        );
}

int createTeacher(
    ApplicationServices& services,
    const QString& koreanName,
    const QString& englishName,
    const QString& preferredName
    )
{
    Teacher teacher;
    teacher.teacherKr = koreanName;
    teacher.teacherEn = englishName;
    teacher.preferredName = preferredName;
    teacher.roomNumber = QStringLiteral("Room 8");
    teacher.phoneNumber = QStringLiteral("010-9876-5432");
    teacher.internetType = QStringLiteral("LAN");
    teacher.wifiName = QStringLiteral("Network %1").arg(englishName);
    teacher.wifiPassword = QStringLiteral("Profile password");
    teacher.projectionType = QStringLiteral("HDMI");
    teacher.zoomId = QStringLiteral("profile.zoom");
    teacher.zoomPassword = QStringLiteral("Zoom secret");
    teacher.notes = QStringLiteral("Navigation profile notes");
    const auto saved = services.dataService()->databaseSession()
        ->teacherRepository()
        ->createTeacher(teacher);
    return saved ? *saved : -1;
}

}

class NavigationTeacherReadParityTests final : public QObject
{
    Q_OBJECT

private slots:
    void selectedTeacherRouteKeepsVisibleProfileOnReadFailureAndLoadsOnSuccess();
};

void NavigationTeacherReadParityTests::
selectedTeacherRouteKeepsVisibleProfileOnReadFailureAndLoadsOnSuccess()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    const int initiallySelected = createTeacher(
        services,
        QStringLiteral("김선택"),
        QStringLiteral("Selected Teacher"),
        QStringLiteral("Selected Teacher")
        );
    const int targetTeacher = createTeacher(
        services,
        QStringLiteral("박대상"),
        QStringLiteral("Target Teacher"),
        QStringLiteral("Target Display")
        );
    QVERIFY(initiallySelected > 0);
    QVERIFY(targetTeacher > 0);

    PageManager pages;
    pages.initialize(&services, false);
    pages.showPage(PageType::TeacherInfo);
    TeacherInfoPage* const page = pages.teacherPage();
    QVERIFY(page);
    auto* header = page->findChild<PageHeader*>();
    QVERIFY(header);
    const auto initial = services.teacherService()->teacher(initiallySelected);
    QVERIFY(initial);
    page->loadTeacher(*initial);
    const QString initialTitle = header->title();
    const QString initialDisplay = page->teacher().preferredDisplayName();

    ResourcePackManager resources(
        directory.filePath(QStringLiteral("resources")),
        directory.filePath(QStringLiteral("baseline"))
        );
    Sidebar sidebar;
    NavigationController navigation(&services, &sidebar, &pages, resources);

    navigation.handleNavigation({
        .type = NodeType::Teacher,
        .teacherId = targetTeacher + 1000
    });
    QVERIFY(pages.isCurrentPage(PageType::TeacherInfo));
    QCOMPARE(page->teacher().id, initiallySelected);
    QCOMPARE(page->teacher().preferredDisplayName(), initialDisplay);
    QCOMPARE(header->title(), initialTitle);

    navigation.handleNavigation({
        .type = NodeType::Teacher,
        .teacherId = targetTeacher
    });
    QVERIFY(pages.isCurrentPage(PageType::TeacherInfo));
    QCOMPARE(page->teacher().id, targetTeacher);
    QCOMPARE(page->teacher().teacherEn, QStringLiteral("Target Teacher"));
    QCOMPARE(page->teacher().preferredName, QStringLiteral("Target Display"));
    QVERIFY(header->title().contains(QStringLiteral("Target Display")));
    QVERIFY(header->title() != initialTitle);

    QSqlQuery query(
        services.dataService()->databaseSession()->database());
    QVERIFY2(query.exec(QStringLiteral("DROP TABLE teachers")),
        qPrintable(query.lastError().text()));
    const QString targetTitle = header->title();
    const QString targetDisplay = page->teacher().preferredDisplayName();
    navigation.handleNavigation({
        .type = NodeType::Teacher,
        .teacherId = targetTeacher
    });
    QVERIFY(pages.isCurrentPage(PageType::TeacherInfo));
    QCOMPARE(page->teacher().id, targetTeacher);
    QCOMPARE(page->teacher().preferredDisplayName(), targetDisplay);
    QCOMPARE(header->title(), targetTitle);
}

QTEST_MAIN(NavigationTeacherReadParityTests)

#include "navigation_teacher_read_parity_tests.moc"
