#include "app/controllers/navigation_controller.h"
#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "core/resource_packs/resource_pack_manager.h"
#include "domain/models/teacher.h"
#include "features/my_info/ui/my_workspace_page.h"
#include "features/my_info/ui/personal_details_page.h"
#include "data/data_service.h"
#include "ui/shared/pages/autosave_coordinator.h"
#include "ui/shared/widgets/navigation_tab_widget.h"
#include "features/teacher/ui/teacher_info_page.h"
#include "fakes/fake_user_prompt_service.h"
#include "ui/shared/dialogs/user_prompt_service.h"
#include "ui/shared/pages/pagemanager.h"
#include "ui/shared/widgets/sidebar/sidebar.h"
#include "ui/shared/widgets/sidebar/sidebar_types.h"

#include <QLineEdit>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTextEdit>
#include <QUuid>
#include <QtTest/QtTest>

#include <cstdio>
#include <optional>

namespace
{

constexpr auto UnsavedNotes = "Unsaved My Info route notes";

QString databasePath(QTemporaryDir& directory)
{
    return directory.filePath(
        QStringLiteral("my-info-navigation-%1.tps").arg(
            QUuid::createUuid().toString(QUuid::WithoutBraces)
            )
        );
}

int createTeacher(ApplicationServices& services, QString* error)
{
    Teacher teacher;
    teacher.teacherKr = QStringLiteral("\uAE40\uC120\uC0DD");
    teacher.teacherEn = QStringLiteral("Route Teacher");
    teacher.preferredRomanization = QStringLiteral("Route Teacher");
    teacher.preferredName = QStringLiteral("Route Teacher");

    const auto created = services.teacherService()->create(teacher);
    if (!created)
    {
        if (error)
        {
            *error = created.error();
        }
        return -1;
    }
    return *created;
}

struct DirtyTeacherPage final
{
    int teacherId = -1;
    TeacherInfoPage* page = nullptr;
    QTextEdit* notes = nullptr;
};

std::optional<DirtyTeacherPage> prepareDirtyTeacherPage(
    ApplicationServices& services,
    PageManager& pages,
    QString* error
    )
{
    const int teacherId = createTeacher(services, error);
    if (teacherId <= 0)
    {
        return std::nullopt;
    }

    const auto teacher = services.teacherService()->teacher(teacherId);
    if (!teacher)
    {
        if (error)
        {
            *error = QStringLiteral("Created teacher could not be reloaded.");
        }
        return std::nullopt;
    }

    pages.initialize(&services, false);
    pages.showPage(PageType::TeacherInfo);
    TeacherInfoPage* const page = pages.teacherPage();
    if (!page)
    {
        if (error)
        {
            *error = QStringLiteral("Teacher page was not initialized.");
        }
        return std::nullopt;
    }

    page->setSaveMode(SaveMode::Manual);
    page->loadTeacher(*teacher);
    QTextEdit* const notes = page->findChild<QTextEdit*>(
        QStringLiteral("teacherNotesEdit")
        );
    if (!notes)
    {
        if (error)
        {
            *error = QStringLiteral("Teacher notes editor was not found.");
        }
        return std::nullopt;
    }

    notes->setPlainText(QString::fromLatin1(UnsavedNotes));
    if (!page->hasUnsavedChanges())
    {
        if (error)
        {
            *error = QStringLiteral("Teacher notes edit was not marked dirty.");
        }
        return std::nullopt;
    }

    return DirtyTeacherPage{
        .teacherId = teacherId,
        .page = page,
        .notes = notes
    };
}

NavigationData myInfoRoute(const QString& routeKey)
{
    QStringList keys{
        QStringLiteral("my_workspace")
    };
    QStringList path{
        QStringLiteral("my_workspace")
    };
    if (routeKey != QStringLiteral("my_workspace"))
    {
        keys.append(routeKey);
        path.append(routeKey);
    }

    return {
        .path = path,
        .keys = keys,
        .routeKey = routeKey,
        .type = NodeType::Page
    };
}

QVector<QString>* capturedWarnings = nullptr;

void captureQtWarning(
    const QtMsgType type,
    const QMessageLogContext&,
    const QString& message
    )
{
    if (capturedWarnings
        && (type == QtWarningMsg || type == QtCriticalMsg))
    {
        capturedWarnings->append(message);
    }
}

class ScopedWarningCapture final
{
public:
    explicit ScopedWarningCapture(QVector<QString>& warnings)
        : m_previous(qInstallMessageHandler(captureQtWarning))
    {
        capturedWarnings = &warnings;
    }

    ~ScopedWarningCapture()
    {
        qInstallMessageHandler(m_previous);
        capturedWarnings = nullptr;
    }

    ScopedWarningCapture(const ScopedWarningCapture&) = delete;
    ScopedWarningCapture& operator=(const ScopedWarningCapture&) = delete;

private:
    QtMessageHandler m_previous = nullptr;
};

class ScopedPromptService final
{
public:
    explicit ScopedPromptService(IUserPromptService& service)
    {
        DialogServices::setUserPromptServiceForTesting(&service);
    }

    ~ScopedPromptService()
    {
        DialogServices::setUserPromptServiceForTesting(nullptr);
    }

    ScopedPromptService(const ScopedPromptService&) = delete;
    ScopedPromptService& operator=(const ScopedPromptService&) = delete;
};

QString workspaceTabName(const WorkspaceTab tab)
{
    switch (tab)
    {
    case WorkspaceTab::Details:
        return QStringLiteral("details");
    case WorkspaceTab::Calendar:
        return QStringLiteral("calendar");
    case WorkspaceTab::Schedule:
    default:
        return QStringLiteral("schedule");
    }
}

QJsonObject observation(
    const QString& route,
    const QString& expectedTab,
    const bool sessionOpen,
    const bool workspacePageCreatedBeforeRoute,
    const int workspaceCreationEventsDuringRoute,
    const bool workspacePageUnchanged,
    PageManager& pages,
    const DirtyTeacherPage& teacherState,
    const FakeUserPromptService& prompts,
    const QVector<QString>& warnings
    )
{
    QJsonObject row;
    row.insert(QStringLiteral("route"), route);
    row.insert(QStringLiteral("expectedTab"), expectedTab);
    row.insert(QStringLiteral("sessionOpen"), sessionOpen);
    row.insert(QStringLiteral("teacherInfoCurrent"),
        pages.isCurrentPage(PageType::TeacherInfo));
    row.insert(QStringLiteral("workspaceCurrent"),
        pages.isCurrentPage(PageType::MyWorkspace));
    row.insert(QStringLiteral("workspacePageCreatedBeforeRoute"),
        workspacePageCreatedBeforeRoute);
    row.insert(QStringLiteral("workspaceCreationEventsDuringRoute"),
        workspaceCreationEventsDuringRoute);
    row.insert(QStringLiteral("workspacePageUnchanged"),
        workspacePageUnchanged);
    row.insert(QStringLiteral("workspacePagePresentAfterRoute"),
        pages.myWorkspacePage() != nullptr);
    row.insert(QStringLiteral("teacherPageUnchanged"),
        pages.teacherPage() == teacherState.page);
    row.insert(QStringLiteral("teacherId"), teacherState.page->teacher().id);
    row.insert(QStringLiteral("teacherNotes"), teacherState.notes->toPlainText());
    row.insert(QStringLiteral("teacherDirty"),
        teacherState.page->hasUnsavedChanges());
    row.insert(QStringLiteral("currentTab"),
        pages.myWorkspacePage()
            ? workspaceTabName(pages.myWorkspacePage()->currentTab())
            : QString());
    row.insert(QStringLiteral("leaveConfirmCount"),
        prompts.unsavedChangesConfirmations.size());
    row.insert(QStringLiteral("otherPromptCount"),
        prompts.messages.size()
            + prompts.asynchronousMessages.size()
            + prompts.confirmations.size()
            + prompts.actionPrompts.size());
    row.insert(QStringLiteral("qtWarningCount"), warnings.size());
    return row;
}

void emitTranscript(const QJsonObject& row)
{
    QByteArray bytes = QJsonDocument(row).toJson(QJsonDocument::Compact);
    bytes.append('\n');
    std::fwrite(
        bytes.constData(),
        1,
        static_cast<std::size_t>(bytes.size()),
        stdout
        );
    std::fflush(stdout);
}

}

class MyInfoRouteNavigationParityTests final : public QObject
{
    Q_OBJECT

private slots:
    void routeAvailabilityMatrix_data();
    void routeAvailabilityMatrix();
    void informationRouteCreatesDetailsOnceAndRetainsManualDraft();
};

void MyInfoRouteNavigationParityTests::routeAvailabilityMatrix_data()
{
    QTest::addColumn<QString>("route");
    QTest::addColumn<QString>("expectedTab");
    QTest::addColumn<bool>("sessionOpen");

    QTest::newRow("workspace-closed")
        << QStringLiteral("my_workspace")
        << QStringLiteral("schedule")
        << false;
    QTest::newRow("workspace-open")
        << QStringLiteral("my_workspace")
        << QStringLiteral("schedule")
        << true;
    QTest::newRow("information-closed")
        << QStringLiteral("my_info_information")
        << QStringLiteral("details")
        << false;
    QTest::newRow("information-open")
        << QStringLiteral("my_info_information")
        << QStringLiteral("details")
        << true;
    QTest::newRow("schedule-closed")
        << QStringLiteral("my_info_schedule")
        << QStringLiteral("schedule")
        << false;
    QTest::newRow("schedule-open")
        << QStringLiteral("my_info_schedule")
        << QStringLiteral("schedule")
        << true;
    QTest::newRow("calendar-closed")
        << QStringLiteral("my_info_calendar")
        << QStringLiteral("calendar")
        << false;
    QTest::newRow("calendar-open")
        << QStringLiteral("my_info_calendar")
        << QStringLiteral("calendar")
        << true;
}

void MyInfoRouteNavigationParityTests::routeAvailabilityMatrix()
{
    QFETCH(QString, route);
    QFETCH(QString, expectedTab);
    QFETCH(bool, sessionOpen);

    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));

    QString error;
    PageManager pages;
    const auto teacherState = prepareDirtyTeacherPage(
        services,
        pages,
        &error
        );
    QVERIFY2(teacherState.has_value(), qPrintable(error));

    Sidebar sidebar;
    NavigationController navigation(
        &services,
        &sidebar,
        &pages,
        ResourcePackManager::instance()
        );
    FakeUserPromptService prompts;
    if (sessionOpen)
    {
        prompts.scriptedUnsavedChangesChoices.enqueue(
            UnsavedChangesChoice::Discard
            );
    }
    ScopedPromptService promptOverride(prompts);
    QVector<QString> warnings;
    ScopedWarningCapture warningCapture(warnings);
    MyWorkspacePage* const workspaceBeforeNavigation =
        pages.myWorkspacePage();
    int workspaceCreationEventsDuringRoute = 0;
    QObject::connect(
        &pages,
        &PageManager::pageCreated,
        &pages,
        [&workspaceCreationEventsDuringRoute](
            const PageType type,
            BasePage*
            )
        {
            if (type == PageType::MyWorkspace)
            {
                ++workspaceCreationEventsDuringRoute;
            }
        }
        );

    if (!sessionOpen)
    {
        services.closeDatabase();
        QVERIFY(!services.hasOpenDatabase());
    }

    navigation.handleNavigation(myInfoRoute(route));

    const QJsonObject row = observation(
        route,
        expectedTab,
        sessionOpen,
        workspaceBeforeNavigation != nullptr,
        workspaceCreationEventsDuringRoute,
        pages.myWorkspacePage() == workspaceBeforeNavigation,
        pages,
        *teacherState,
        prompts,
        warnings
        );
    emitTranscript(row);

    QVERIFY2(warnings.isEmpty(),
        qPrintable(warnings.isEmpty() ? QString() : warnings.constFirst()));
    QVERIFY(prompts.messages.isEmpty());
    QVERIFY(prompts.asynchronousMessages.isEmpty());
    QVERIFY(prompts.confirmations.isEmpty());
    QVERIFY(prompts.actionPrompts.isEmpty());

    if (!sessionOpen)
    {
        QVERIFY(pages.isCurrentPage(PageType::TeacherInfo));
        QVERIFY(pages.currentWidget() == teacherState->page);
        QVERIFY(pages.teacherPage() == teacherState->page);
        QVERIFY(!pages.isCurrentPage(PageType::MyWorkspace));
        QCOMPARE(workspaceCreationEventsDuringRoute, 0);
        QCOMPARE(pages.myWorkspacePage(), workspaceBeforeNavigation);
        QCOMPARE(teacherState->page->teacher().id, teacherState->teacherId);
        QCOMPARE(teacherState->notes->toPlainText(),
            QString::fromLatin1(UnsavedNotes));
        QVERIFY(teacherState->page->hasUnsavedChanges());
        QVERIFY(prompts.unsavedChangesConfirmations.isEmpty());
        return;
    }

    QVERIFY(pages.isCurrentPage(PageType::MyWorkspace));
    QVERIFY(pages.myWorkspacePage());
    QVERIFY(pages.teacherPage() == teacherState->page);
    QCOMPARE(workspaceTabName(pages.myWorkspacePage()->currentTab()), expectedTab);
    QCOMPARE(prompts.unsavedChangesConfirmations.size(), 1);
    QCOMPARE(teacherState->notes->toPlainText(), QString());
    QVERIFY(!teacherState->page->hasUnsavedChanges());
}

void MyInfoRouteNavigationParityTests::informationRouteCreatesDetailsOnceAndRetainsManualDraft()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    const QString storedName = QStringLiteral("Route stored name");
    QVERIFY(services.dataService()->saveSetting(QStringLiteral("myInfo/name"), storedName));
    PageManager pages;
    pages.initialize(&services, false);
    pages.setDatabaseOpen(true);
    pages.setSaveMode(SaveMode::Manual);
    auto* workspace = pages.myWorkspacePage();
    QVERIFY(workspace);
    QCOMPARE(workspace->currentTab(), WorkspaceTab::Schedule);
    QVERIFY(!workspace->personalDetailsPage());
    QVERIFY(workspace->findChildren<PersonalDetailsPage*>().isEmpty());
    Sidebar sidebar;
    NavigationController navigation(&services, &sidebar, &pages, ResourcePackManager::instance());
    FakeUserPromptService prompts;
    ScopedPromptService promptOverride(prompts);
    navigation.handleNavigation(myInfoRoute(QStringLiteral("my_info_information")));
    auto* details = workspace->personalDetailsPage();
    QVERIFY(details);
    QCOMPARE(workspace->currentTab(), WorkspaceTab::Details);
    QCOMPARE(pages.personalDetailsPage(), details);
    QCOMPARE(workspace->findChildren<PersonalDetailsPage*>().size(), 1);
    const auto editors = details->findChildren<QLineEdit*>();
    QVERIFY(!editors.isEmpty());
    auto* name = editors.constFirst();
    QCOMPARE(name->text(), storedName);
    QVERIFY(!details->needsRefresh());
    auto* autosave = details->findChild<AutosaveCoordinator*>();
    QVERIFY(autosave);
    QCOMPARE(autosave->saveMode(), SaveMode::Manual);
    const QString draft = QStringLiteral("Information route unsaved draft");
    name->setText(draft);
    QVERIFY(workspace->hasUnsavedChanges());
    navigation.handleNavigation(myInfoRoute(QStringLiteral("my_info_schedule")));
    QCOMPARE(workspace->currentTab(), WorkspaceTab::Schedule);
    navigation.handleNavigation(myInfoRoute(QStringLiteral("my_info_information")));
    QCOMPARE(workspace->personalDetailsPage(), details);
    QCOMPARE(workspace->findChildren<PersonalDetailsPage*>().size(), 1);
    QCOMPARE(name->text(), draft);
    QVERIFY(workspace->hasUnsavedChanges());
    const auto saved = services.dataService()->loadSetting(QStringLiteral("myInfo/name"));
    QVERIFY(saved);
    QCOMPARE(saved.value().toString(), storedName);
    auto* tabs = workspace->findChild<NavigationTabWidget*>(QStringLiteral("myWorkspaceTabs"));
    QVERIFY(tabs);
    QCOMPARE(tabs->count(), 3);
    QCOMPARE(tabs->tabText(0), QStringLiteral("My Details"));
    QCOMPARE(tabs->tabText(1), QStringLiteral("My Schedule"));
    QCOMPARE(tabs->tabText(2), QStringLiteral("Calendar"));
    QVERIFY(!workspace->calendarPage());
    QVERIFY(prompts.messages.isEmpty());
    QVERIFY(prompts.asynchronousMessages.isEmpty());
    QVERIFY(prompts.confirmations.isEmpty());
    QVERIFY(prompts.unsavedChangesConfirmations.isEmpty());
    QVERIFY(prompts.actionPrompts.isEmpty());
}

QTEST_MAIN(MyInfoRouteNavigationParityTests)

#include "my_info_route_navigation_parity_tests.moc"
