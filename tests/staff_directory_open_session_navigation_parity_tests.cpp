#include "app/controllers/navigation_controller.h"
#include "app/mainwindow.h"
#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "core/language_service.h"
#include "core/resource_packs/resource_pack_manager.h"
#include "core/settingsmanager.h"
#include "data/database/database_session.h"
#include "data/repositories/gs_team_repository.h"
#include "data/repositories/native_english_teacher_repository.h"
#include "domain/models/gs_team_member.h"
#include "domain/models/native_english_teacher.h"
#include "domain/models/teacher.h"
#include "features/teacher/ui/staff_directory_page.h"
#include "features/teacher/ui/teacher_info_page.h"
#include "fakes/fake_user_prompt_service.h"
#include "ui/shared/dialogs/user_prompt_service.h"
#include "ui/shared/pages/pagemanager.h"
#include "ui/shared/widgets/sidebar/sidebar.h"
#include "ui/shared/widgets/sidebar/sidebar_types.h"

#include <QCoreApplication>
#include <QComboBox>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonObject>
#include <QLineEdit>
#include <QMessageLogContext>
#include <QRect>
#include <QSignalSpy>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QTextEdit>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QUuid>
#include <QVector>
#include <QtTest/QtTest>

#include <utility>

namespace
{

QString databasePath(QTemporaryDir& directory)
{
    return directory.filePath(
        QStringLiteral("staff-directory-open-session-%1.tps").arg(
            QUuid::createUuid().toString(QUuid::WithoutBraces)
            )
        );
}

Teacher persistedTeacher()
{
    Teacher teacher;
    teacher.teacherKr = QStringLiteral("\uAE40\uC120\uC0DD");
    teacher.teacherEn = QStringLiteral("Route Teacher");
    teacher.preferredRomanization = QStringLiteral("Route Roman");
    teacher.preferredName = teacher.teacherEn;
    teacher.roomNumber = QStringLiteral("Room 14");
    teacher.birthday = QStringLiteral("04-12");
    teacher.phoneNumber = QStringLiteral("010-9876-1111");
    teacher.wifiName = QStringLiteral("Persisted Network");
    teacher.wifiPassword = QStringLiteral("Persisted WiFi Password");
    teacher.internetType = QStringLiteral("LAN");
    teacher.zoomId = QStringLiteral("persisted.zoom");
    teacher.zoomPassword = QStringLiteral("Persisted Zoom Password");
    teacher.projectionType = QStringLiteral("HDMI");
    teacher.notes = QStringLiteral("Persisted F396 notes");
    return teacher;
}

NativeEnglishTeacher nativeTeacher(
    const QString& name,
    const QString& suffix
    )
{
    NativeEnglishTeacher teacher;
    teacher.name = name;
    teacher.position = QStringLiteral("NET");
    teacher.phoneNumber = QStringLiteral("010-1000-%1").arg(suffix);
    teacher.email = QStringLiteral("%1@example.test").arg(name.toLower());
    teacher.birthday = QStringLiteral("03-%1").arg(suffix.right(2));
    teacher.nationality = QStringLiteral("Nationality %1").arg(name);
    return teacher;
}

GsTeamMember gsTeamMember(
    const QString& name,
    const QString& koreanName,
    const QString& suffix
    )
{
    GsTeamMember member;
    member.name = name;
    member.koreanName = koreanName;
    member.position = QStringLiteral("M1");
    member.phoneNumber = QStringLiteral("010-2000-%1").arg(suffix);
    member.birthday = QStringLiteral("04-%1").arg(suffix.right(2));
    return member;
}

bool seedStaffDirectories(ApplicationServices& services)
{
    const auto nativeEnglishSaved = services.databaseSession()
        ->nativeEnglishTeacherRepository()
        ->saveDirectory(
            {
                nativeTeacher(QStringLiteral("Zulu Native"), QStringLiteral("0002")),
                nativeTeacher(QStringLiteral("Alpha Native"), QStringLiteral("0001"))
            },
            {}
            );
    const auto gsTeamSaved = services.databaseSession()
        ->gsTeamRepository()
        ->saveDirectory(
            {
                gsTeamMember(
                    QStringLiteral("Zulu GS"),
                    QStringLiteral("\uB098"),
                    QStringLiteral("0002")
                    ),
                gsTeamMember(
                    QStringLiteral("Alpha GS"),
                    QStringLiteral("\uAC00"),
                    QStringLiteral("0001")
                    )
            },
            {}
            );
    return static_cast<bool>(nativeEnglishSaved)
        && static_cast<bool>(gsTeamSaved);
}

QJsonObject teacherSnapshot(const Teacher& teacher)
{
    return {
        {QStringLiteral("id"), teacher.id},
        {QStringLiteral("teacherKr"), teacher.teacherKr},
        {QStringLiteral("teacherEn"), teacher.teacherEn},
        {QStringLiteral("preferredRomanization"), teacher.preferredRomanization},
        {QStringLiteral("preferredName"), teacher.preferredName},
        {QStringLiteral("roomNumber"), teacher.roomNumber},
        {QStringLiteral("birthday"), teacher.birthday},
        {QStringLiteral("phoneNumber"), teacher.phoneNumber},
        {QStringLiteral("wifiName"), teacher.wifiName},
        {QStringLiteral("wifiPassword"), teacher.wifiPassword},
        {QStringLiteral("internetType"), teacher.internetType},
        {QStringLiteral("zoomId"), teacher.zoomId},
        {QStringLiteral("zoomPassword"), teacher.zoomPassword},
        {QStringLiteral("projectionType"), teacher.projectionType},
        {QStringLiteral("notes"), teacher.notes}
    };
}

QJsonObject teacherFormSnapshot(TeacherInfoPage& page)
{
    QJsonObject snapshot;
    for (const QLineEdit* edit : page.findChildren<QLineEdit*>())
    {
        if (!edit->objectName().isEmpty())
        {
            snapshot.insert(
                QStringLiteral("line:%1").arg(edit->objectName()),
                edit->text()
                );
        }
    }
    for (const QComboBox* combo : page.findChildren<QComboBox*>())
    {
        if (!combo->objectName().isEmpty())
        {
            snapshot.insert(
                QStringLiteral("combo:%1").arg(combo->objectName()),
                QJsonObject{
                    {QStringLiteral("text"), combo->currentText()},
                    {QStringLiteral("data"), combo->currentData().toString()}
                }
                );
        }
    }
    for (const QTextEdit* edit : page.findChildren<QTextEdit*>())
    {
        if (!edit->objectName().isEmpty())
        {
            snapshot.insert(
                QStringLiteral("text:%1").arg(edit->objectName()),
                edit->toPlainText()
                );
        }
    }
    return snapshot;
}

QTreeWidgetItem* findSidebarNode(
    QTreeWidgetItem* parent,
    const QString& key
    )
{
    if (
        parent
        && parent->data(0, Qt::UserRole + 4).toString() == key
        )
    {
        return parent;
    }

    if (!parent)
    {
        return nullptr;
    }

    for (int index = 0; index < parent->childCount(); ++index)
    {
        if (QTreeWidgetItem* const found = findSidebarNode(
                parent->child(index),
                key
                ))
        {
            return found;
        }
    }
    return nullptr;
}

QTreeWidgetItem* findSidebarNode(
    QTreeWidget& tree,
    const QString& key
    )
{
    for (int index = 0; index < tree.topLevelItemCount(); ++index)
    {
        if (QTreeWidgetItem* const found = findSidebarNode(
                tree.topLevelItem(index),
                key
                ))
        {
            return found;
        }
    }
    return nullptr;
}

int promptCount(const FakeUserPromptService& prompts)
{
    return static_cast<int>(
        prompts.messages.size()
        + prompts.asynchronousMessages.size()
        + prompts.confirmations.size()
        + prompts.unsavedChangesConfirmations.size()
        + prompts.actionPrompts.size()
        );
}

QVector<QString>* capturedWarnings = nullptr;

void captureQtWarning(
    const QtMsgType type,
    const QMessageLogContext&,
    const QString& message
    )
{
    if (
        capturedWarnings
        && (type == QtWarningMsg || type == QtCriticalMsg)
        )
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

void verifyOpenSessionRoute(
    const QString& routeKey,
    const UnsavedChangesChoice choice
    )
{
    const bool isNativeEnglish =
        routeKey == QStringLiteral("native_english_teachers");
    const PageType destinationType = isNativeEnglish
        ? PageType::NativeEnglishTeachers
        : PageType::GsTeam;

    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    QVERIFY(seedStaffDirectories(services));

    Teacher baseline = persistedTeacher();
    const auto created = services.teacherService()->create(baseline);
    if (!created)
    {
        QFAIL(qPrintable(created.error()));
        return;
    }
    baseline.id = *created;
    const auto persisted = services.teacherService()->teacher(baseline.id);
    QVERIFY(persisted.has_value());
    baseline = *persisted;

    PageManager pages;
    pages.initialize(&services, false);
    pages.showPage(PageType::TeacherInfo);
    TeacherInfoPage* const teacherPage = pages.teacherPage();
    QVERIFY(teacherPage);
    teacherPage->setSaveMode(SaveMode::Manual);
    teacherPage->loadTeacher(baseline);
    const QJsonObject cleanForm = teacherFormSnapshot(*teacherPage);

    auto* teacherEn = teacherPage->findChild<QLineEdit*>(
        QStringLiteral("teacherEnEdit")
        );
    auto* romanization = teacherPage->findChild<QLineEdit*>(
        QStringLiteral("preferredRomanizationEdit")
        );
    auto* wifiName = teacherPage->findChild<QLineEdit*>(
        QStringLiteral("wifiNameEdit")
        );
    auto* notes = teacherPage->findChild<QTextEdit*>(
        QStringLiteral("teacherNotesEdit")
        );
    QVERIFY(teacherEn);
    QVERIFY(romanization);
    QVERIFY(wifiName);
    QVERIFY(notes);

    teacherEn->setText(QStringLiteral("Edited Teacher"));
    romanization->setText(QStringLiteral("Edited Roman"));
    wifiName->setText(QStringLiteral("Edited WiFi"));
    notes->setPlainText(QStringLiteral("Unsaved F396 Teacher Notes"));
    QVERIFY(teacherPage->hasUnsavedChanges());
    const QJsonObject dirtyForm = teacherFormSnapshot(*teacherPage);
    QVERIFY(dirtyForm != cleanForm);

    ResourcePackManager resources(
        directory.filePath(QStringLiteral("resources")),
        directory.filePath(QStringLiteral("baseline"))
        );
    Sidebar sidebar;
    sidebar.resize(360, 640);
    sidebar.show();
    auto* tree = sidebar.findChild<QTreeWidget*>(
        QStringLiteral("sidebarTree")
        );
    QVERIFY(tree);
    QTreeWidgetItem* const campusStaff = findSidebarNode(
        *tree,
        QStringLiteral("campus_staff")
        );
    QTreeWidgetItem* const leaf = findSidebarNode(*tree, routeKey);
    QVERIFY(campusStaff);
    QVERIFY(leaf);
    campusStaff->setExpanded(true);
    tree->scrollToItem(leaf);
    QCoreApplication::processEvents();
    const QRect leafRect = tree->visualItemRect(leaf);
    QVERIFY(leafRect.isValid());
    QVERIFY(!leafRect.isEmpty());

    NavigationController navigation(&services, &sidebar, &pages, resources);
    QSignalSpy routeSpy(&sidebar, &Sidebar::itemSelected);
    QVERIFY(routeSpy.isValid());
    QTest::mouseClick(
        tree->viewport(),
        Qt::LeftButton,
        Qt::NoModifier,
        leafRect.center()
        );
    QCOMPARE(routeSpy.size(), 1);

    const NavigationData emittedRoute =
        qvariant_cast<NavigationData>(routeSpy.takeFirst().at(0));
    const QString expectedLeafLabel = isNativeEnglish
        ? QStringLiteral("Native English Teachers")
        : QStringLiteral("GS Team");
    const QStringList expectedPath{
            QStringLiteral("Campus Staff"),
            expectedLeafLabel
        };
    const QStringList expectedKeys{
            QStringLiteral("campus_staff"),
            routeKey
        };
    QCOMPARE(emittedRoute.path, expectedPath);
    QCOMPARE(emittedRoute.keys, expectedKeys);
    QCOMPARE(emittedRoute.routeKey, routeKey);
    QCOMPARE(emittedRoute.type, NodeType::Page);
    QVERIFY(services.hasOpenDatabase());
    QVERIFY(pages.isCurrentPage(PageType::TeacherInfo));
    QVERIFY(!pages.isPageInstantiated(destinationType));

    FakeUserPromptService prompts;
    prompts.scriptedUnsavedChangesChoices.enqueue(choice);
    DialogServices::setUserPromptServiceForTesting(&prompts);
    QVector<QString> warnings;
    {
        ScopedWarningCapture warningCapture(warnings);
        navigation.handleNavigation(emittedRoute);
    }

    QCOMPARE(prompts.unsavedChangesConfirmations.size(), 1);
    QCOMPARE(promptCount(prompts), 1);
    QCOMPARE(prompts.messages.size(), 0);
    QCOMPARE(prompts.asynchronousMessages.size(), 0);
    QCOMPARE(prompts.confirmations.size(), 0);
    QCOMPARE(prompts.actionPrompts.size(), 0);
    QVERIFY2(warnings.isEmpty(), qPrintable(warnings.join(QLatin1Char('\n'))));
    QVERIFY(services.hasOpenDatabase());
    QVERIFY(services.teacherService()->isAvailable());
    QCOMPARE(teacherPage->teacher().id, baseline.id);

    const auto persistedAfterRoute = services.teacherService()->teacher(
        baseline.id
        );
    QVERIFY(persistedAfterRoute.has_value());
    QCOMPARE(
        teacherSnapshot(*persistedAfterRoute),
        teacherSnapshot(baseline)
        );

    if (choice == UnsavedChangesChoice::Cancel)
    {
        QVERIFY(pages.isCurrentPage(PageType::TeacherInfo));
        QCOMPARE(teacherFormSnapshot(*teacherPage), dirtyForm);
        QVERIFY(teacherPage->hasUnsavedChanges());
        QVERIFY(!pages.isPageInstantiated(destinationType));
        QVERIFY(isNativeEnglish
            ? pages.nativeEnglishTeachersPage() == nullptr
            : pages.gsTeamPage() == nullptr);
        QVERIFY(!pages.isCurrentPage(destinationType));
        return;
    }

    QVERIFY(pages.isPageInstantiated(destinationType));
    QVERIFY(pages.isCurrentPage(destinationType));
    QVERIFY(!teacherPage->hasUnsavedChanges());
    QCOMPARE(teacherFormSnapshot(*teacherPage), cleanForm);

    StaffDirectoryPage* const destination = isNativeEnglish
        ? pages.nativeEnglishTeachersPage()
        : pages.gsTeamPage();
    QVERIFY(destination);
    QTableWidget* const table = destination->findChild<QTableWidget*>(
        isNativeEnglish
            ? QStringLiteral("nativeEnglishTeachersTable")
            : QStringLiteral("gsTeamTable")
        );
    QVERIFY(table);
    QCOMPARE(table->rowCount(), 2);
    QCOMPARE(
        table->item(0, 0)->text(),
        isNativeEnglish
            ? QStringLiteral("Alpha Native")
            : QStringLiteral("Alpha GS")
        );
    QCOMPARE(
        table->item(1, 0)->text(),
        isNativeEnglish
            ? QStringLiteral("Zulu Native")
            : QStringLiteral("Zulu GS")
        );
}

}

class StaffDirectoryOpenSessionNavigationParityTests final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanup();
    void nativeEnglishCancelPreservesDirtyTeacherInfo();
    void nativeEnglishDiscardLoadsDirectoryAndDropsTeacherEdit();
    void gsTeamCancelPreservesDirtyTeacherInfo();
    void gsTeamDiscardLoadsDirectoryAndDropsTeacherEdit();
    void mainWindowRenderedLeavesNavigateWithinOpenSession();

private:
    QTemporaryDir m_settingsDirectory;
};

void StaffDirectoryOpenSessionNavigationParityTests::initTestCase()
{
    QVERIFY(m_settingsDirectory.isValid());
    qputenv(
        "CLASSMNGR_SETTINGS_ROOT",
        m_settingsDirectory.path().toUtf8()
        );
    SettingsManager::instance().clear();
    SettingsManager::instance().sync();
    qRegisterMetaType<NavigationData>();
}

void StaffDirectoryOpenSessionNavigationParityTests::cleanup()
{
    DialogServices::setUserPromptServiceForTesting(nullptr);
}

void StaffDirectoryOpenSessionNavigationParityTests::
nativeEnglishCancelPreservesDirtyTeacherInfo()
{
    verifyOpenSessionRoute(
        QStringLiteral("native_english_teachers"),
        UnsavedChangesChoice::Cancel
        );
}

void StaffDirectoryOpenSessionNavigationParityTests::
nativeEnglishDiscardLoadsDirectoryAndDropsTeacherEdit()
{
    verifyOpenSessionRoute(
        QStringLiteral("native_english_teachers"),
        UnsavedChangesChoice::Discard
        );
}

void StaffDirectoryOpenSessionNavigationParityTests::
gsTeamCancelPreservesDirtyTeacherInfo()
{
    verifyOpenSessionRoute(
        QStringLiteral("gs_team"),
        UnsavedChangesChoice::Cancel
        );
}

void StaffDirectoryOpenSessionNavigationParityTests::
gsTeamDiscardLoadsDirectoryAndDropsTeacherEdit()
{
    verifyOpenSessionRoute(
        QStringLiteral("gs_team"),
        UnsavedChangesChoice::Discard
        );
}

void StaffDirectoryOpenSessionNavigationParityTests::
mainWindowRenderedLeavesNavigateWithinOpenSession()
{
    QTemporaryDir workspaceRoot;
    QVERIFY(workspaceRoot.isValid());

    const QString workspacePath = QFileInfo(
        databasePath(workspaceRoot)
        ).absoluteFilePath();

    ApplicationServices seedServices;
    QVERIFY(seedServices.openDatabase(workspacePath));
    QVERIFY(seedStaffDirectories(seedServices));
    seedServices.closeDatabase();
    QVERIFY(!seedServices.hasOpenDatabase());

    LanguageService languageService;
    QVERIFY(languageService.setLanguage(Language::English));

    MainWindowStartupOptions startupOptions;
    startupOptions.loadMostRecentDatabase = false;
    startupOptions.initialDatabasePath = workspacePath;

    MainWindow window(
        [](const QString&) {},
        false,
        &languageService,
        nullptr,
        std::move(startupOptions)
        );
    window.show();
    QCoreApplication::processEvents();
    QVERIFY(window.isVisible());

    ApplicationServices* const services = window.services();
    QVERIFY(services);
    QVERIFY(services->hasOpenDatabase());
    QCOMPARE(services->currentDatabasePath(), workspacePath);
    DatabaseSession* const activeSession = services->databaseSession();
    QVERIFY(activeSession);

    PageManager* const pages = window.pageManager();
    QVERIFY(pages);

    Sidebar* const sidebar = window.findChild<Sidebar*>();
    QVERIFY(sidebar);
    QTreeWidget* const tree = sidebar->findChild<QTreeWidget*>(
        QStringLiteral("sidebarTree")
        );
    QVERIFY(tree);

    QSignalSpy routeSpy(sidebar, &Sidebar::itemSelected);
    QVERIFY(routeSpy.isValid());

    const auto verifyRenderedRoute = [
        &window,
        services,
        activeSession,
        workspacePath,
        pages,
        sidebar,
        tree,
        &routeSpy
        ](
        const QString& routeKey,
        const QString& leafLabel,
        const PageType pageType,
        const QString& tableObjectName,
        const QString& alphaName,
        const QString& zuluName
        )
    {
        QTreeWidgetItem* const campusStaff = findSidebarNode(
            *tree,
            QStringLiteral("campus_staff")
            );
        QTreeWidgetItem* const leaf = findSidebarNode(*tree, routeKey);
        QVERIFY(campusStaff);
        QVERIFY(leaf);
        campusStaff->setExpanded(true);
        tree->scrollToItem(leaf);
        QCoreApplication::processEvents();

        const QRect leafRect = tree->visualItemRect(leaf);
        QVERIFY(leafRect.isValid());
        QVERIFY(!leafRect.isEmpty());
        QTest::mouseClick(
            tree->viewport(),
            Qt::LeftButton,
            Qt::NoModifier,
            leafRect.center()
            );
        QCoreApplication::processEvents();

        QCOMPARE(routeSpy.size(), 1);
        const NavigationData emittedRoute =
            qvariant_cast<NavigationData>(routeSpy.takeFirst().at(0));
        const QStringList expectedPath{
            QStringLiteral("Campus Staff"),
            leafLabel
        };
        const QStringList expectedKeys{
            QStringLiteral("campus_staff"),
            routeKey
        };
        QCOMPARE(emittedRoute.type, NodeType::Page);
        QCOMPARE(emittedRoute.path, expectedPath);
        QCOMPARE(emittedRoute.keys, expectedKeys);
        QCOMPARE(emittedRoute.routeKey, routeKey);
        QVERIFY(routeSpy.isEmpty());
        QCOMPARE(sidebar->selectedKeys(), expectedKeys);

        QVERIFY(pages->isPageInstantiated(pageType));
        QVERIFY(pages->isCurrentPage(pageType));
        StaffDirectoryPage* const destination =
            pageType == PageType::NativeEnglishTeachers
            ? pages->nativeEnglishTeachersPage()
            : pages->gsTeamPage();
        QVERIFY(destination);
        QTableWidget* const table = destination->findChild<QTableWidget*>(
            tableObjectName
            );
        QVERIFY(table);
        QCOMPARE(table->rowCount(), 2);
        QVERIFY(table->item(0, 0));
        QVERIFY(table->item(1, 0));
        QCOMPARE(table->item(0, 0)->text(), alphaName);
        QCOMPARE(table->item(1, 0)->text(), zuluName);

        QVERIFY(services->hasOpenDatabase());
        QCOMPARE(services->databaseSession(), activeSession);
        QCOMPARE(services->currentDatabasePath(), workspacePath);
        QVERIFY(window.isVisible());
    };

    verifyRenderedRoute(
        QStringLiteral("native_english_teachers"),
        QStringLiteral("Native English Teachers"),
        PageType::NativeEnglishTeachers,
        QStringLiteral("nativeEnglishTeachersTable"),
        QStringLiteral("Alpha Native"),
        QStringLiteral("Zulu Native")
        );
    verifyRenderedRoute(
        QStringLiteral("gs_team"),
        QStringLiteral("GS Team"),
        PageType::GsTeam,
        QStringLiteral("gsTeamTable"),
        QStringLiteral("Alpha GS"),
        QStringLiteral("Zulu GS")
        );
}

QTEST_MAIN(StaffDirectoryOpenSessionNavigationParityTests)

#include "staff_directory_open_session_navigation_parity_tests.moc"
