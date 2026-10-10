#include "app/controllers/navigation_controller.h"
#include "core/application_services.h"
#include "core/resource_packs/resource_pack_manager.h"
#include "data/database/database_session.h"
#include "data/repositories/native_english_teacher_repository.h"
#include "domain/models/native_english_teacher.h"
#include "features/teacher/ui/staff_directory_page.h"
#include "staff_directory_test_utils.h"
#include "fakes/fake_user_prompt_service.h"
#include "ui/shared/dialogs/user_prompt_service.h"
#include "ui/shared/pages/pagemanager.h"
#include "ui/shared/widgets/sidebar/sidebar.h"

#include <QApplication>
#include <QSqlError>
#include <QSqlQuery>
#include <QComboBox>
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
    void nativePositionEditorKeepsRawKnownAndUnknownValues();
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
    auto* table = page.findChild<QTableView*>(
        QStringLiteral("nativeEnglishTeachersTable"));
    QVERIFY(table);
    QCOMPARE(table->model()->columnCount(), 6);
    QCOMPARE(table->model()->rowCount(), persisted->size());

    for (int row = 0; row < persisted->size(); ++row)
    {
        const NativeEnglishTeacher& expected = persisted->at(row);
        QCOMPARE(staffDirectoryCellText(*table, row, 0), expected.name);
        QCOMPARE(staffDirectoryCellData(*table, row, 0, NativeEnglishIdRole).toInt(),
            expected.id);
        QCOMPARE(staffDirectoryCellText(*table, row, 1), expected.position);
        QCOMPARE(staffDirectoryCellText(*table, row, 2), expected.phoneNumber);
        QCOMPARE(staffDirectoryCellText(*table, row, 3), expected.email);
        QCOMPARE(staffDirectoryCellText(*table, row, 4), expected.birthday);
        QCOMPARE(staffDirectoryCellText(*table, row, 5), expected.nationality);
    }
    QCOMPARE(staffDirectoryCellText(*table, 0, 0), QStringLiteral("Coordinator"));
    QCOMPARE(staffDirectoryCellText(*table, 1, 0), QStringLiteral("Alpha"));
    QCOMPARE(staffDirectoryCellText(*table, 2, 0), QStringLiteral("Zulu"));
    QVERIFY(!page.hasUnsavedChanges());
}

void StaffDirectoryNativeEnglishReadTests::
nativePositionEditorKeepsRawKnownAndUnknownValues()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    NativeEnglishTeacher coordinator = teacher(
        QStringLiteral("Coordinator"),
        QStringLiteral("Co-ordinator"),
        QStringLiteral("0001"));
    NativeEnglishTeacher unknown = teacher(
        QStringLiteral("Unknown Position"),
        QStringLiteral("Learning Support"),
        QStringLiteral("0002"));
    QVERIFY(static_cast<bool>(services.databaseSession()
        ->nativeEnglishTeacherRepository()->saveDirectory(
            {coordinator, unknown}, {})));

    StaffDirectoryPage page(
        &services,
        StaffDirectoryKind::NativeEnglishTeachers);
    page.setSaveMode(SaveMode::Manual);
    QVERIFY(page.loadDirectory());
    auto* table = page.findChild<QTableView*>(
        QStringLiteral("nativeEnglishTeachersTable"));
    QVERIFY(table);
    page.resize(1000, 700);
    page.show();
    QApplication::processEvents();

    const auto rowForName = [table](const QString& name) {
        for (int row = 0; row < table->model()->rowCount(); ++row)
        {
            if (staffDirectoryCellText(*table, row, 0) == name)
            {
                return row;
            }
        }
        return -1;
    };

    const int coordinatorRow = rowForName(QStringLiteral("Coordinator"));
    QVERIFY(coordinatorRow >= 0);
    QModelIndex positionIndex = table->model()->index(coordinatorRow, 1);
    table->setCurrentIndex(positionIndex);
    table->edit(positionIndex);
    auto* position = table->findChild<QComboBox*>();
    QTRY_VERIFY(position);
    QCOMPARE(position->currentData().toString(), QStringLiteral("Co-ordinator"));
    const int coordinatorOption = position->findData(
        QStringLiteral("Co-ordinator"));
    QVERIFY(coordinatorOption >= 0);
    QCOMPARE(position->itemText(coordinatorOption), QStringLiteral("Coordinator"));
    QTest::keyClick(position, Qt::Key_Escape);
    QTRY_VERIFY(table->findChildren<QComboBox*>().isEmpty());
    QCOMPARE(staffDirectoryCellText(*table, coordinatorRow, 1),
        QStringLiteral("Co-ordinator"));
    QVERIFY(!page.hasUnsavedChanges());

    const int unknownRow = rowForName(QStringLiteral("Unknown Position"));
    QVERIFY(unknownRow >= 0);
    positionIndex = table->model()->index(unknownRow, 1);
    table->setCurrentIndex(positionIndex);
    table->edit(positionIndex);
    position = table->findChild<QComboBox*>();
    QTRY_VERIFY(position);
    QCOMPARE(position->currentData().toString(), QStringLiteral("Learning Support"));
    QVERIFY(position->findData(QStringLiteral("Learning Support")) >= 0);
    QTest::keyClick(position, Qt::Key_Escape);
    QTRY_VERIFY(table->findChildren<QComboBox*>().isEmpty());
    QCOMPARE(staffDirectoryCellText(*table, unknownRow, 1),
        QStringLiteral("Learning Support"));
    QVERIFY(!page.hasUnsavedChanges());
}

void StaffDirectoryNativeEnglishReadTests::unavailableSessionClearsSilently()
{
    StaffDirectoryPage page(
        nullptr,
        StaffDirectoryKind::NativeEnglishTeachers);
    page.setSaveMode(SaveMode::Manual);
    auto* table = page.findChild<QTableView*>(
        QStringLiteral("nativeEnglishTeachersTable"));
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
    auto* table = page.findChild<QTableView*>(
        QStringLiteral("nativeEnglishTeachersTable"));
    QVERIFY(table);
    setStaffDirectoryCellText(*table, 0, 0, QStringLiteral("Edited locally"));
    QVERIFY(page.hasUnsavedChanges());

    QSqlQuery query(services.databaseSession()->database());
    QVERIFY2(query.exec(QStringLiteral("DROP TABLE native_english_teachers")),
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
    auto* table = page->findChild<QTableView*>(
        QStringLiteral("nativeEnglishTeachersTable"));
    QVERIFY(table);
    QCOMPARE(staffDirectoryCellText(*table, 0, 0), QStringLiteral("Coordinator"));

    setStaffDirectoryCellText(*table, 0, 0, QStringLiteral("Local unsaved edit"));
    QVERIFY(page->hasUnsavedChanges());
    QSqlQuery update(services.databaseSession()->database());
    update.prepare(QStringLiteral(
        "UPDATE native_english_teachers SET name=? WHERE id=?"));
    update.addBindValue(QStringLiteral("Changed in database"));
    update.addBindValue(staffDirectoryCellData(*table, 0, 0, NativeEnglishIdRole));
    QVERIFY2(update.exec(), qPrintable(update.lastError().text()));

    prompts.scriptedUnsavedChangesChoices.enqueue(
        UnsavedChangesChoice::Cancel);
    navigation.handleNavigation(route);
    QCOMPARE(prompts.unsavedChangesConfirmations.size(), 1);
    QVERIFY(pages.isCurrentPage(PageType::NativeEnglishTeachers));
    QCOMPARE(staffDirectoryCellText(*table, 0, 0), QStringLiteral("Local unsaved edit"));
    QVERIFY(page->hasUnsavedChanges());

    prompts.scriptedUnsavedChangesChoices.enqueue(
        UnsavedChangesChoice::Discard);
    navigation.handleNavigation(route);
    QVERIFY(pages.isCurrentPage(PageType::NativeEnglishTeachers));
    QCOMPARE(staffDirectoryCellText(*table, 0, 0), QStringLiteral("Changed in database"));
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
