#include "app/controllers/sidebar_controller.h"
#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "data/database/database_session.h"
#include "data/repositories/teacher_repository.h"
#include "data/repositories/teacher_import_repository.h"
#include "data/repositories/native_english_teacher_repository.h"
#include "data/repositories/gs_team_repository.h"
#include "domain/models/native_english_teacher.h"
#include "domain/models/gs_team_member.h"
#include "features/setup/ui/initial_setup_wizard.h"
#include "features/teacher/ui/teacher_import_dialog.h"
#include "features/teacher/ui/staff_directory_page.h"
#include "features/teacher/ui/teacher_info_page.h"
#include "fakes/fake_user_prompt_service.h"
#include "next/application/teacher_profile_read_port.h"
#include "next/platform/application_services_teacher_profile_read_port.h"
#include "ui/shared/actions/action_registry.h"
#include "ui/shared/dialogs/user_prompt_service.h"
#include "ui/shared/pages/pagemanager.h"
#include "ui/shared/widgets/sidebar/sidebar.h"

#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QItemSelectionModel>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSqlError>
#include <QSqlQuery>
#include <QTableView>
#include <QTemporaryDir>
#include <QTimer>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QUuid>
#include <QSignalSpy>
#include <QTextEdit>
#include <QtTest/QtTest>

#include <cstdio>
#include <cstddef>
#include <functional>

namespace
{
QString databasePath(QTemporaryDir& directory)
{
    return directory.filePath(QStringLiteral("teacher-import-ui-parity-%1.tps")
        .arg(QUuid::createUuid().toString(QUuid::WithoutBraces)));
}

QString fixturePath()
{
    return QDir(QFileInfo(QString::fromUtf8(__FILE__)).absolutePath()).filePath(
        QStringLiteral("fixtures/teacher_import/sectioned_review.xlsx"));
}

int countRows(QSqlDatabase database, const QString& table)
{
    QSqlQuery query(database);
    if (!query.exec(QStringLiteral("SELECT COUNT(*) FROM %1").arg(table))
        || !query.next())
    {
        return -1;
    }
    return query.value(0).toInt();
}

bool installLateWriteFailure(ApplicationServices& services)
{
    QSqlQuery query(services.databaseSession()->database());
    return query.exec(QStringLiteral(
        "CREATE TRIGGER parity_reject_gs BEFORE INSERT ON gs_team "
        "BEGIN SELECT RAISE(ABORT, 'forced parity failure'); END"));
}

struct DialogRun final
{
    bool found = false;
    bool accepted = false;
};

bool runWithTeacherDialog(
    QWidget* owner,
    const bool accept,
    const std::function<void()>& invoke,
    DialogRun& observation
    )
{
    QTimer poll;
    poll.setInterval(10);
    QTimer timeout;
    timeout.setSingleShot(true);

    QObject::connect(&poll, &QTimer::timeout, owner, [&]
    {
        auto* dialog = qobject_cast<TeacherImportDialog*>(
            QApplication::activeModalWidget());
        if (!dialog)
        {
            for (QWidget* widget : QApplication::topLevelWidgets())
            {
                dialog = qobject_cast<TeacherImportDialog*>(widget);
                if (dialog)
                {
                    break;
                }
            }
        }
        if (!dialog)
        {
            return;
        }

        observation.found = true;
        if (!accept)
        {
            poll.stop();
            dialog->reject();
            return;
        }

        auto* path = dialog->findChild<QLineEdit*>(
            QStringLiteral("teacherImportFilePath"));
        auto* button = dialog->findChild<QPushButton*>(
            QStringLiteral("teacherImportAcceptButton"));
        if (!path || !button)
        {
            poll.stop();
            dialog->reject();
            return;
        }
        if (path->text().isEmpty())
        {
            dialog->setFilePath(fixturePath());
        }
        if (button->isEnabled())
        {
            observation.accepted = true;
            poll.stop();
            button->click();
        }
    });
    QObject::connect(&timeout, &QTimer::timeout, owner, [&]
    {
        if (auto* dialog = qobject_cast<TeacherImportDialog*>(
                QApplication::activeModalWidget()))
        {
            dialog->reject();
        }
    });

    poll.start();
    timeout.start(15000);
    invoke();
    poll.stop();
    timeout.stop();
    return observation.found && (!accept || observation.accepted);
}

QString tableCounts(ApplicationServices& services)
{
    const QSqlDatabase database = services.databaseSession()->database();
    return QStringLiteral("teachers=%1,native=%2,gs=%3")
        .arg(countRows(database, QStringLiteral("teachers")))
        .arg(countRows(database, QStringLiteral("native_english_teachers")))
        .arg(countRows(database, QStringLiteral("gs_team")));
}

int sidebarKoreanTeacherCount(Sidebar& sidebar)
{
    auto* const tree = sidebar.findChild<QTreeWidget*>();
    if (!tree)
    {
        return -1;
    }
    QTreeWidgetItem* campusStaff = nullptr;
    for (int index = 0; index < tree->topLevelItemCount(); ++index)
    {
        auto* const item = tree->topLevelItem(index);
        if (item->data(0, Qt::UserRole + 4).toString()
            == QStringLiteral("campus_staff"))
        {
            campusStaff = item;
            break;
        }
    }
    if (!campusStaff)
    {
        return -1;
    }
    for (int index = 0; index < campusStaff->childCount(); ++index)
    {
        auto* const item = campusStaff->child(index);
        if (item->data(0, Qt::UserRole + 4).toString()
            == QStringLiteral("teachers_all_korean"))
        {
            return item->childCount();
        }
    }
    return -1;
}

QString singleLine(QString value)
{
    value.replace(QStringLiteral("\r"), QStringLiteral("\\r"));
    value.replace(QStringLiteral("\n"), QStringLiteral("\\n"));
    return value;
}

int rowForName(const QTableView& table, const QString& name)
{
    for (int row = 0; row < table.model()->rowCount(); ++row)
    {
        if (table.model()->index(row, 0).data(Qt::EditRole).toString() == name)
        {
            return row;
        }
    }
    return -1;
}

QString persistedDirectoryPosition(
    ApplicationServices& services,
    const bool nativeEnglish,
    const QString& name
    )
{
    QSqlQuery query(services.databaseSession()->database());
    query.prepare(nativeEnglish
        ? QStringLiteral(
              "SELECT position FROM native_english_teachers WHERE name=?")
        : QStringLiteral("SELECT position FROM gs_team WHERE name=?"));
    query.addBindValue(name);
    return query.exec() && query.next()
        ? query.value(0).toString()
        : QString();
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

struct ScopedPromptService final
{
    explicit ScopedPromptService(IUserPromptService& service)
    {
        DialogServices::setUserPromptServiceForTesting(&service);
    }

    ~ScopedPromptService()
    {
        DialogServices::setUserPromptServiceForTesting(nullptr);
    }
};

class FailingTeacherProfileReadPort final
    : public ClassMngr::Next::Application::TeacherProfileReadPort
{
public:
    [[nodiscard]] ClassMngr::Next::Application::TeacherProfileReadResult
    readTeacherProfile(
        const ClassMngr::Next::Domain::TeacherId& id
        ) const override
    {
        Q_UNUSED(id);
        return ClassMngr::Next::Application::TeacherProfileReadResult::failure({
            .code = ClassMngr::Next::Domain::ErrorCode::Technical,
            .message = "Injected teacher profile read failure.",
            .recoverable = true
        });
    }
};

void emitTranscript(const QString& entry, const QString& scenario,
    const QString& result)
{
    const QByteArray row = QStringLiteral("TEACHER_IMPORT_UI|%1|%2|%3")
        .arg(entry, scenario, result).toUtf8();
    std::fwrite(row.constData(), 1, static_cast<std::size_t>(row.size()), stdout);
    std::fwrite("\n", 1, 1, stdout);
    std::fflush(stdout);
}
}

class TeacherImportUiApplyParityTests final : public QObject
{
    Q_OBJECT

private slots:
    void sidebarSuccessTranscript();
    void importRefreshesActiveTeacherPageBeforeFurtherEdit();
    void failedImportRefreshRequiresFreshTeacherProfileLoad();
    void failedStaffDirectoryImportRefreshLocksStalePage_data();
    void failedStaffDirectoryImportRefreshLocksStalePage();
    void sidebarCancelTranscript();
    void sidebarErrorTranscript();
    void wizardSuccessTranscript();
    void wizardCancelTranscript();
    void wizardErrorTranscript();
};

void TeacherImportUiApplyParityTests::sidebarSuccessTranscript()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QVERIFY(QFileInfo::exists(fixturePath()));
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    PageManager pages;
    Sidebar sidebar;
    SidebarController controller(&services, &sidebar, &pages);
    ActionRegistry actions;
    actions.createActions();
    controller.connectActions(actions);
    FakeUserPromptService prompts;
    ScopedPromptService promptScope(prompts);

    DialogRun dialog;
    QVERIFY(runWithTeacherDialog(&sidebar, true,
        [&] { actions.importTeachers->trigger(); }, dialog));

    QCOMPARE(prompts.messages.size(), 1);
    QCOMPARE(prompts.messages.first().severity, PromptSeverity::Information);
    const QString summary = singleLine(prompts.messages.first().message);
    const QString counts = tableCounts(services);
    QVERIFY(counts.contains(QStringLiteral("teachers=")));
    const int refreshedKoreanTeachers = sidebarKoreanTeacherCount(sidebar);
    QVERIFY(refreshedKoreanTeachers > 0);
    emitTranscript(QStringLiteral("sidebar"), QStringLiteral("success"),
        counts + QStringLiteral("|sidebarKoreanTeachers=%1|notice=information|summary=")
            .arg(refreshedKoreanTeachers) + summary);
}

void TeacherImportUiApplyParityTests::
failedStaffDirectoryImportRefreshLocksStalePage_data()
{
    QTest::addColumn<bool>("nativeEnglish");
    QTest::addColumn<bool>("automaticSave");

    QTest::newRow("native-english-manual") << true << false;
    QTest::newRow("native-english-automatic") << true << true;
    QTest::newRow("gs-team-manual") << false << false;
    QTest::newRow("gs-team-automatic") << false << true;
}

void TeacherImportUiApplyParityTests::
failedStaffDirectoryImportRefreshLocksStalePage()
{
    QFETCH(bool, nativeEnglish);
    QFETCH(bool, automaticSave);

    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QVERIFY(QFileInfo::exists(fixturePath()));
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));

    NativeEnglishTeacher alex;
    alex.name = QStringLiteral("Alex");
    alex.position = QStringLiteral("Prior Native Position");
    alex.phoneNumber = QStringLiteral("010-1234-0001");
    alex.email = QStringLiteral("alex@example.test");
    alex.birthday = QStringLiteral("01-02");
    alex.nationality = QStringLiteral("Australia");
    QVERIFY(services.databaseSession()->nativeEnglishTeacherRepository()
        ->saveDirectory({alex}, {}));

    GsTeamMember taylor;
    taylor.name = QStringLiteral("Taylor");
    taylor.koreanName = QStringLiteral("\uD14C\uC77C\uB7EC");
    taylor.position = QStringLiteral("Prior GS Position");
    taylor.phoneNumber = QStringLiteral("010-1234-0002");
    taylor.birthday = QStringLiteral("02-03");
    QVERIFY(services.databaseSession()->gsTeamRepository()
        ->saveDirectory({taylor}, {}));

    PageManager pages;
    pages.initialize(&services, false);
    StaffDirectoryPage* const nativePage =
        pages.ensureNativeEnglishTeachersPage();
    StaffDirectoryPage* const gsPage = pages.ensureGsTeamPage();
    QVERIFY(nativePage);
    QVERIFY(gsPage);
    QVERIFY(nativePage->loadDirectory());
    QVERIFY(gsPage->loadDirectory());

    StaffDirectoryPage* const page = nativeEnglish ? nativePage : gsPage;
    const QString targetName = nativeEnglish
        ? QStringLiteral("Alex")
        : QStringLiteral("Taylor");
    const QString priorPosition = nativeEnglish
        ? QStringLiteral("Prior Native Position")
        : QStringLiteral("Prior GS Position");
    const QString importedPosition = nativeEnglish
        ? QStringLiteral("Team Leader")
        : QStringLiteral("M2");
    const int positionColumn = nativeEnglish ? 1 : 2;
    page->setSaveMode(
        automaticSave ? SaveMode::Automatic : SaveMode::Manual);

    QTableView* const table = page->findChild<QTableView*>(
        nativeEnglish
            ? QStringLiteral("nativeEnglishTeachersTable")
            : QStringLiteral("gsTeamTable"));
    QVERIFY(table);
    const int originalRow = rowForName(*table, targetName);
    QVERIFY(originalRow >= 0);
    QCOMPARE(
        table->model()->index(originalRow, positionColumn)
            .data(Qt::EditRole).toString(),
        priorPosition);

    QPushButton* const addButton = buttonWithText(*page, QStringLiteral("Add"));
    QPushButton* const deleteButton = buttonWithText(
        *page, QStringLiteral("Delete"));
    QPushButton* const saveButton = buttonWithText(
        *page, QStringLiteral("Save Changes"));
    QVERIFY(addButton);
    QVERIFY(deleteButton);
    QVERIFY(saveButton);
    page->failNextImportRefreshDirectoryReadForTesting();

    Sidebar sidebar;
    SidebarController controller(&services, &sidebar, &pages);
    ActionRegistry actions;
    actions.createActions();
    controller.connectActions(actions);
    FakeUserPromptService prompts;
    ScopedPromptService promptScope(prompts);

    DialogRun dialog;
    QVERIFY(runWithTeacherDialog(&sidebar, true,
        [&] { actions.importTeachers->trigger(); }, dialog));

    QCOMPARE(prompts.messages.size(), 2);
    QCOMPARE(prompts.messages.at(0).severity, PromptSeverity::Information);
    QCOMPARE(prompts.messages.at(1).severity, PromptSeverity::Warning);
    QCOMPARE(
        prompts.messages.at(1).title,
        QStringLiteral("Import Teachers"));
    const QString warning = prompts.messages.at(1).message;
    const QString failureDetail = nativeEnglish
        ? QStringLiteral(
              "Native English Teachers: Injected Native English directory read failure.")
        : QStringLiteral("GS Team: Injected GS Team directory read failure.");
    QCOMPARE(
        warning,
        QStringLiteral(
            "The teacher import committed its changes, but a staff directory refresh failed. "
            "Reload the affected directory before editing or saving.\n\n%1")
            .arg(failureDetail));

    QCOMPARE(
        persistedDirectoryPosition(services, nativeEnglish, targetName),
        importedPosition);
    QCOMPARE(table->model()->rowCount(), 1);
    QCOMPARE(rowForName(*table, targetName), originalRow);
    QCOMPARE(
        table->model()->index(originalRow, positionColumn)
            .data(Qt::EditRole).toString(),
        priorPosition);
    QVERIFY(!(table->model()->flags(
        table->model()->index(originalRow, positionColumn))
        & Qt::ItemIsEditable));
    QVERIFY(!table->model()->setData(
        table->model()->index(originalRow, positionColumn),
        QStringLiteral("Stale overwrite"), Qt::EditRole));
    QVERIFY(!table->model()->insertRows(table->model()->rowCount(), 1));
    QVERIFY(!table->model()->removeRows(originalRow, 1));
    QVERIFY(!addButton->isEnabled());
    table->selectionModel()->select(
        table->model()->index(originalRow, 0),
        QItemSelectionModel::Select | QItemSelectionModel::Rows);
    QVERIFY(!deleteButton->isEnabled());
    QCOMPARE(saveButton->isHidden(), automaticSave);
    QVERIFY(!saveButton->isEnabled());
    QVERIFY(!page->hasUnsavedChanges());
    QVERIFY(!page->saveChanges());
    page->saveData();
    QTest::qWait(800);
    QCOMPARE(
        persistedDirectoryPosition(services, nativeEnglish, targetName),
        importedPosition);

    QVERIFY(page->loadDirectory());
    const int reloadedRow = rowForName(*table, targetName);
    QVERIFY(reloadedRow >= 0);
    QCOMPARE(
        table->model()->index(reloadedRow, positionColumn)
            .data(Qt::EditRole).toString(),
        importedPosition);
    QVERIFY(table->model()->flags(
        table->model()->index(reloadedRow, positionColumn))
        & Qt::ItemIsEditable);
    QVERIFY(!page->hasUnsavedChanges());
    QCOMPARE(saveButton->isHidden(), automaticSave);

    const QString localPosition = QStringLiteral("Local edit after reload");
    QVERIFY(table->model()->setData(
        table->model()->index(reloadedRow, positionColumn),
        localPosition,
        Qt::EditRole));
    QVERIFY(page->hasUnsavedChanges());
    if (automaticSave)
    {
        QVERIFY(saveButton->isHidden());
        QTRY_COMPARE(
            persistedDirectoryPosition(services, nativeEnglish, targetName),
            localPosition);
    }
    else
    {
        QVERIFY(!saveButton->isHidden());
        QVERIFY(saveButton->isEnabled());
        QCOMPARE(
            persistedDirectoryPosition(services, nativeEnglish, targetName),
            importedPosition);
        page->saveData();
        QCOMPARE(
            persistedDirectoryPosition(services, nativeEnglish, targetName),
            localPosition);
    }
}

void TeacherImportUiApplyParityTests::sidebarCancelTranscript()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    PageManager pages;
    Sidebar sidebar;
    SidebarController controller(&services, &sidebar, &pages);
    ActionRegistry actions;
    actions.createActions();
    controller.connectActions(actions);
    FakeUserPromptService prompts;
    ScopedPromptService promptScope(prompts);

    DialogRun dialog;
    QVERIFY(runWithTeacherDialog(&sidebar, false,
        [&] { actions.importTeachers->trigger(); }, dialog));

    QVERIFY(prompts.messages.isEmpty());
    emitTranscript(QStringLiteral("sidebar"), QStringLiteral("cancel"),
        tableCounts(services) + QStringLiteral("|notice=none"));
}

void TeacherImportUiApplyParityTests::
importRefreshesActiveTeacherPageBeforeFurtherEdit()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QVERIFY(QFileInfo::exists(fixturePath()));
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));

    Teacher original;
    original.teacherKr = QStringLiteral("홍길동D");
    original.teacherEn = QStringLiteral("Manual English");
    original.preferredRomanization = QStringLiteral("Manual Romanization");
    original.preferredName = QStringLiteral("Manual English");
    original.roomNumber = QStringLiteral("Old Room");
    original.birthday = QStringLiteral("01-01");
    original.phoneNumber = QStringLiteral("010-1111-1111");
    original.wifiName = QStringLiteral("Manual WiFi");
    original.wifiPassword = QStringLiteral("Manual WiFi Password");
    original.internetType = QStringLiteral("LAN");
    original.zoomId = QStringLiteral("manual.zoom");
    original.zoomPassword = QStringLiteral("manual.zoom.password");
    original.projectionType = QStringLiteral("Any");
    original.notes = QStringLiteral("Manual notes");
    const RepositoryResult<int> created = services.databaseSession()
        ->teacherRepository()->createTeacher(original);
    QVERIFY(created);
    original.id = *created;

    PageManager pages;
    pages.initialize(&services, false);
    pages.showPage(PageType::TeacherInfo);
    TeacherInfoPage* const page = pages.teacherPage();
    QVERIFY(page);
    page->setSaveMode(SaveMode::Manual);
    page->loadTeacher(original, original.id);
    QCOMPARE(pages.currentWidget(), page);
    QCOMPARE(page->teacherId(), static_cast<qint64>(original.id));
    QVERIFY(!page->hasUnsavedChanges());

    Sidebar sidebar;
    SidebarController controller(&services, &sidebar, &pages);
    ActionRegistry actions;
    actions.createActions();
    controller.connectActions(actions);
    FakeUserPromptService prompts;
    ScopedPromptService promptScope(prompts);

    DialogRun dialog;
    QVERIFY(runWithTeacherDialog(&sidebar, true,
        [&] { actions.importTeachers->trigger(); }, dialog));

    QVERIFY(pages.isCurrentPage(PageType::TeacherInfo));
    QCOMPARE(pages.currentWidget(), page);
    QCOMPARE(page->teacherId(), static_cast<qint64>(original.id));
    QCOMPARE(page->teacher().teacherKr, QStringLiteral("홍길동"));
    QCOMPARE(page->teacher().roomNumber, QStringLiteral("413"));
    QCOMPARE(page->teacher().birthday, QStringLiteral("02-29"));
    QCOMPARE(page->teacher().phoneNumber, QStringLiteral("010-1111-1111"));
    QCOMPARE(page->teacher().teacherEn, QStringLiteral("Manual English"));
    QVERIFY(!page->hasUnsavedChanges());
    QCOMPARE(prompts.messages.size(), 1);
    QCOMPARE(prompts.messages.first().severity, PromptSeverity::Information);
    QVERIFY(prompts.messages.first().message.contains(
        QStringLiteral("1 updated")));

    auto* notes = page->findChild<QTextEdit*>(
        QStringLiteral("teacherNotesEdit"));
    QVERIFY(notes);
    notes->setPlainText(QStringLiteral("Edited after teacher import"));
    QVERIFY(page->hasUnsavedChanges());
    QVERIFY(page->saveChanges());
    QVERIFY(!page->hasUnsavedChanges());

    const RepositoryResult<Teacher> persisted = services.databaseSession()
        ->teacherRepository()->getTeacher(original.id);
    QVERIFY(persisted);
    QCOMPARE(persisted->teacherKr, QStringLiteral("홍길동"));
    QCOMPARE(persisted->roomNumber, QStringLiteral("413"));
    QCOMPARE(persisted->birthday, QStringLiteral("02-29"));
    QCOMPARE(persisted->phoneNumber, QStringLiteral("010-1111-1111"));
    QCOMPARE(persisted->teacherEn, QStringLiteral("Manual English"));
    QCOMPARE(persisted->notes, QStringLiteral("Edited after teacher import"));
}

void TeacherImportUiApplyParityTests::
failedImportRefreshRequiresFreshTeacherProfileLoad()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));

    Teacher original;
    original.teacherKr = QStringLiteral("김수진");
    original.teacherEn = QStringLiteral("Original English");
    original.preferredName = original.teacherEn;
    original.roomNumber = QStringLiteral("Old Room");
    original.notes = QStringLiteral("Original notes");
    const RepositoryResult<int> created = services.databaseSession()
        ->teacherRepository()->createTeacher(original);
    QVERIFY(created);
    original.id = *created;

    TeacherInfoPage page(&services);
    page.setSaveMode(SaveMode::Manual);
    page.loadTeacher(original, original.id);
    auto* korean = page.findChild<QLineEdit*>(
        QStringLiteral("teacherKrEdit"));
    auto* room = page.findChild<QLineEdit*>(
        QStringLiteral("roomNumberEdit"));
    auto* save = page.findChild<QPushButton*>(
        QStringLiteral("teacherInfoSaveButton"));
    QVERIFY(korean);
    QVERIFY(room);
    QVERIFY(save);

    QSqlQuery imported(services.databaseSession()->database());
    imported.prepare(QStringLiteral(
        "UPDATE teachers SET teacher_kr=?, room_number=? WHERE id=?"));
    imported.addBindValue(QStringLiteral("Imported Name"));
    imported.addBindValue(QStringLiteral("413"));
    imported.addBindValue(original.id);
    QVERIFY(imported.exec());

    FailingTeacherProfileReadPort failingRead;
    QVERIFY(!page.reloadTeacherProfile(failingRead));
    QCOMPARE(page.teacherId(), static_cast<qint64>(original.id));
    QCOMPARE(page.teacher().teacherKr, QStringLiteral("김수진"));
    QCOMPARE(korean->text(), QStringLiteral("김수진"));
    QVERIFY(!korean->isEnabled());
    QVERIFY(!room->isEnabled());
    QVERIFY(!save->isEnabled());
    QVERIFY(!page.hasUnsavedChanges());

    page.saveData();
    const RepositoryResult<Teacher> untouched = services.databaseSession()
        ->teacherRepository()->getTeacher(original.id);
    QVERIFY(untouched);
    QCOMPARE(untouched->teacherKr, QStringLiteral("Imported Name"));
    QCOMPARE(untouched->roomNumber, QStringLiteral("413"));

    ClassMngr::Next::Platform::ApplicationServicesTeacherProfileReadPort readPort(
        &services);
    QVERIFY(page.reloadTeacherProfile(readPort));
    QCOMPARE(page.teacherId(), static_cast<qint64>(original.id));
    QCOMPARE(page.teacher().teacherKr, QStringLiteral("Imported Name"));
    QCOMPARE(page.teacher().roomNumber, QStringLiteral("413"));
    QVERIFY(korean->isEnabled());
    QVERIFY(room->isEnabled());
    QVERIFY(!page.hasUnsavedChanges());
}

void TeacherImportUiApplyParityTests::sidebarErrorTranscript()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    QVERIFY(installLateWriteFailure(services));
    PageManager pages;
    Sidebar sidebar;
    SidebarController controller(&services, &sidebar, &pages);
    ActionRegistry actions;
    actions.createActions();
    controller.connectActions(actions);
    FakeUserPromptService prompts;
    ScopedPromptService promptScope(prompts);

    DialogRun dialog;
    QVERIFY(runWithTeacherDialog(&sidebar, true,
        [&] { actions.importTeachers->trigger(); }, dialog));

    QCOMPARE(prompts.messages.size(), 1);
    QCOMPARE(prompts.messages.first().severity, PromptSeverity::Warning);
    emitTranscript(QStringLiteral("sidebar"), QStringLiteral("error"),
        tableCounts(services) + QStringLiteral("|notice=warning|text=")
            + singleLine(prompts.messages.first().message));
}

void TeacherImportUiApplyParityTests::wizardSuccessTranscript()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QVERIFY(QFileInfo::exists(fixturePath()));
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    InitialSetupWizard wizard(&services);
    wizard.setStartId(InitialSetupWizard::TeacherImportPage);
    wizard.show();
    QApplication::processEvents();
    auto* button = wizard.findChild<QPushButton*>(
        QStringLiteral("setupImportTeachersButton"));
    auto* status = wizard.findChild<QLabel*>(
        QStringLiteral("setupTeacherImportStatus"));
    QVERIFY(button);
    QVERIFY(status);
    QWizardPage* const importPage = wizard.currentPage();
    QVERIFY(importPage);
    QSignalSpy completeChanged(importPage, &QWizardPage::completeChanged);
    FakeUserPromptService prompts;
    ScopedPromptService promptScope(prompts);

    DialogRun dialog;
    QVERIFY(runWithTeacherDialog(&wizard, true,
        [&] { button->click(); }, dialog));

    QVERIFY(status->text().startsWith(QStringLiteral("Import complete:")));
    QCOMPARE(prompts.messages.size(), 0);
    QCOMPARE(completeChanged.size(), 1);
    QVERIFY(importPage->isComplete());
    wizard.next();
    QVERIFY(wizard.currentId() != InitialSetupWizard::TeacherImportPage);
    emitTranscript(QStringLiteral("wizard"), QStringLiteral("success"),
        tableCounts(services) + QStringLiteral("|complete=true|completeChanged=%1|status=")
            .arg(completeChanged.size())
            + singleLine(status->text()));
    wizard.reject();
}

void TeacherImportUiApplyParityTests::wizardCancelTranscript()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    InitialSetupWizard wizard(&services);
    wizard.setStartId(InitialSetupWizard::TeacherImportPage);
    wizard.show();
    QApplication::processEvents();
    auto* button = wizard.findChild<QPushButton*>(
        QStringLiteral("setupImportTeachersButton"));
    QVERIFY(button);
    FakeUserPromptService prompts;
    ScopedPromptService promptScope(prompts);

    DialogRun dialog;
    QVERIFY(runWithTeacherDialog(&wizard, false,
        [&] { button->click(); }, dialog));

    QVERIFY(!wizard.isVisible());
    QVERIFY(prompts.messages.isEmpty());
    emitTranscript(QStringLiteral("wizard"), QStringLiteral("cancel"),
        tableCounts(services) + QStringLiteral("|result=rejected"));
}

void TeacherImportUiApplyParityTests::wizardErrorTranscript()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    QVERIFY(installLateWriteFailure(services));
    InitialSetupWizard wizard(&services);
    wizard.setStartId(InitialSetupWizard::TeacherImportPage);
    wizard.show();
    QApplication::processEvents();
    auto* button = wizard.findChild<QPushButton*>(
        QStringLiteral("setupImportTeachersButton"));
    auto* status = wizard.findChild<QLabel*>(
        QStringLiteral("setupTeacherImportStatus"));
    QVERIFY(button);
    QVERIFY(status);
    QWizardPage* const importPage = wizard.currentPage();
    QVERIFY(importPage);
    QSignalSpy completeChanged(importPage, &QWizardPage::completeChanged);
    const QString initialStatus = status->text();
    FakeUserPromptService prompts;
    ScopedPromptService promptScope(prompts);

    DialogRun dialog;
    QVERIFY(runWithTeacherDialog(&wizard, true,
        [&] { button->click(); }, dialog));

    QCOMPARE(status->text(), initialStatus);
    QCOMPARE(prompts.messages.size(), 1);
    QCOMPARE(prompts.messages.first().severity, PromptSeverity::Warning);
    QCOMPARE(completeChanged.size(), 0);
    wizard.next();
    QCOMPARE(wizard.currentId(), InitialSetupWizard::TeacherImportPage);
    emitTranscript(QStringLiteral("wizard"), QStringLiteral("error"),
        tableCounts(services) + QStringLiteral("|notice=warning|text=")
            + singleLine(prompts.messages.first().message)
            + QStringLiteral("|status=unchanged|completeChanged=%1")
                .arg(completeChanged.size()));
    wizard.reject();
}

QTEST_MAIN(TeacherImportUiApplyParityTests)

#include "teacher_import_ui_apply_parity_tests.moc"
