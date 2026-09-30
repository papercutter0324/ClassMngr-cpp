#include "app/controllers/navigation_controller.h"
#include "core/application_services.h"
#include "core/resource_packs/resource_pack_manager.h"
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
        QStringLiteral("staff-directory-native-read-%1.tps").arg(
            QUuid::createUuid().toString(QUuid::WithoutBraces)
            )
        );
}

NativeEnglishTeacher teacher(
    const QString& name,
    const QString& position,
    const QString& suffix
    )
{
    NativeEnglishTeacher value;
    value.name = name;
    value.position = position;
    value.phoneNumber = QStringLiteral("010-1000-%1").arg(suffix);
    value.email = QStringLiteral("%1@example.test").arg(name.toLower());
    value.birthday = QStringLiteral("03-%1").arg(suffix.right(2));
    value.nationality = QStringLiteral("Nationality %1").arg(name);
    return value;
}

QList<NativeEnglishTeacher> fixtureTeachers()
{
    return {
        teacher(QStringLiteral("Zulu"), QStringLiteral("NET"),
            QStringLiteral("0003")),
        teacher(QStringLiteral("Coordinator"), QStringLiteral("Co-ordinator"),
            QStringLiteral("0001")),
        teacher(QStringLiteral("Alpha"), QStringLiteral("NET"),
            QStringLiteral("0002"))
    };
}

bool seedTeachers(ApplicationServices& services)
{
    return static_cast<bool>(services.databaseSession()
        ->nativeEnglishTeacherRepository()->saveDirectory(fixtureTeachers(), {}));
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

class StaffDirectoryNativeEnglishReadTests final : public QObject
{
    Q_OBJECT

private slots:
    void cleanup();
    void nativePageMapsSixFieldsInRepositoryOrderWithTypedRowIds();
    void unavailableSessionClearsSilently();
    void repositoryFailureWarnsAndRetainsDirtyState();
    void routeConfirmsBeforeReadAndShowsOnlyAfterSuccessfulLoad();
};

void StaffDirectoryNativeEnglishReadTests::cleanup()
{
    DialogServices::setUserPromptServiceForTesting(nullptr);
}

void StaffDirectoryNativeEnglishReadTests::
nativePageMapsSixFieldsInRepositoryOrderWithTypedRowIds()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    QVERIFY(seedTeachers(services));
    const auto persisted = services.databaseSession()
        ->nativeEnglishTeacherRepository()->getAll();
    QVERIFY(persisted);

    StaffDirectoryPage page(
        &services,
        StaffDirectoryKind::NativeEnglishTeachers);
    QVERIFY(page.loadDirectory());
    auto* table = page.findChild<QTableWidget*>(
        QStringLiteral("nativeEnglishTeachersTable"));
    QVERIFY(table);
    QCOMPARE(table->columnCount(), 6);
    QCOMPARE(table->rowCount(), persisted->size());

    for (int row = 0; row < persisted->size(); ++row)
    {
        const NativeEnglishTeacher& expected = persisted->at(row);
        QCOMPARE(table->item(row, 0)->text(), expected.name);
        QCOMPARE(table->item(row, 0)->data(NativeEnglishIdRole).toInt(),
            expected.id);
        QCOMPARE(table->item(row, 1)->text(), expected.position);
        QCOMPARE(table->item(row, 2)->text(), expected.phoneNumber);
        QCOMPARE(table->item(row, 3)->text(), expected.email);
        QCOMPARE(table->item(row, 4)->text(), expected.birthday);
        QCOMPARE(table->item(row, 5)->text(), expected.nationality);
    }
    QCOMPARE(table->item(0, 0)->text(), QStringLiteral("Coordinator"));
    QCOMPARE(table->item(1, 0)->text(), QStringLiteral("Alpha"));
    QCOMPARE(table->item(2, 0)->text(), QStringLiteral("Zulu"));
    QVERIFY(!page.hasUnsavedChanges());
}

void StaffDirectoryNativeEnglishReadTests::unavailableSessionClearsSilently()
{
    StaffDirectoryPage page(
        nullptr,
        StaffDirectoryKind::NativeEnglishTeachers);
    page.setSaveMode(SaveMode::Manual);
    auto* table = page.findChild<QTableWidget*>(
        QStringLiteral("nativeEnglishTeachersTable"));
    QVERIFY(table);
    table->setRowCount(1);
    table->setItem(0, 0, new QTableWidgetItem(QStringLiteral("Stale row")));
    QVERIFY(page.hasUnsavedChanges());

    FakeUserPromptService prompts;
    DialogServices::setUserPromptServiceForTesting(&prompts);
    QVERIFY(!page.loadDirectory());

    QCOMPARE(table->rowCount(), 0);
    QVERIFY(!page.hasUnsavedChanges());
    QCOMPARE(prompts.messages.size(), 0);
}

void StaffDirectoryNativeEnglishReadTests::
repositoryFailureWarnsAndRetainsDirtyState()
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
    table->item(0, 0)->setText(QStringLiteral("Edited locally"));
    QVERIFY(page.hasUnsavedChanges());

    QSqlQuery query(services.databaseSession()->database());
    QVERIFY2(query.exec(QStringLiteral("DROP TABLE native_english_teachers")),
        qPrintable(query.lastError().text()));

    FakeUserPromptService prompts;
    DialogServices::setUserPromptServiceForTesting(&prompts);
    QVERIFY(!page.loadDirectory());

    QCOMPARE(table->rowCount(), 0);
    QVERIFY(page.hasUnsavedChanges());
    QCOMPARE(prompts.messages.size(), 1);
    QCOMPARE(prompts.messages.first().severity, PromptSeverity::Warning);
    QCOMPARE(prompts.messages.first().title, QStringLiteral("Load Directory"));
    QVERIFY(!prompts.messages.first().message.isEmpty());
}

void StaffDirectoryNativeEnglishReadTests::
routeConfirmsBeforeReadAndShowsOnlyAfterSuccessfulLoad()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    QVERIFY(seedTeachers(services));

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

    const NavigationData route = nativeEnglishDirectoryRoute();
    navigation.handleNavigation(route);
    QVERIFY(pages.isCurrentPage(PageType::NativeEnglishTeachers));
    auto* page = pages.nativeEnglishTeachersPage();
    QVERIFY(page);
    page->setSaveMode(SaveMode::Manual);
    auto* table = page->findChild<QTableWidget*>(
        QStringLiteral("nativeEnglishTeachersTable"));
    QVERIFY(table);
    QCOMPARE(table->item(0, 0)->text(), QStringLiteral("Coordinator"));

    table->item(0, 0)->setText(QStringLiteral("Local unsaved edit"));
    QVERIFY(page->hasUnsavedChanges());
    QSqlQuery update(services.databaseSession()->database());
    update.prepare(QStringLiteral(
        "UPDATE native_english_teachers SET name=? WHERE id=?"));
    update.addBindValue(QStringLiteral("Changed in database"));
    update.addBindValue(table->item(0, 0)->data(NativeEnglishIdRole));
    QVERIFY2(update.exec(), qPrintable(update.lastError().text()));

    prompts.scriptedUnsavedChangesChoices.enqueue(
        UnsavedChangesChoice::Cancel);
    navigation.handleNavigation(route);
    QCOMPARE(prompts.unsavedChangesConfirmations.size(), 1);
    QVERIFY(pages.isCurrentPage(PageType::NativeEnglishTeachers));
    QCOMPARE(table->item(0, 0)->text(), QStringLiteral("Local unsaved edit"));
    QVERIFY(page->hasUnsavedChanges());

    prompts.scriptedUnsavedChangesChoices.enqueue(
        UnsavedChangesChoice::Discard);
    navigation.handleNavigation(route);
    QVERIFY(pages.isCurrentPage(PageType::NativeEnglishTeachers));
    QCOMPARE(table->item(0, 0)->text(), QStringLiteral("Changed in database"));
    QVERIFY(!page->hasUnsavedChanges());

    pages.showPage(PageType::TeacherInfo);
    QVERIFY(pages.isCurrentPage(PageType::TeacherInfo));
    QSqlQuery drop(services.databaseSession()->database());
    QVERIFY2(drop.exec(QStringLiteral("DROP TABLE native_english_teachers")),
        qPrintable(drop.lastError().text()));
    navigation.handleNavigation(route);
    QVERIFY(pages.isCurrentPage(PageType::TeacherInfo));
    QCOMPARE(prompts.messages.size(), 1);
    QCOMPARE(prompts.messages.first().severity, PromptSeverity::Warning);
}

QTEST_MAIN(StaffDirectoryNativeEnglishReadTests)

#include "staff_directory_native_english_read_tests.moc"
