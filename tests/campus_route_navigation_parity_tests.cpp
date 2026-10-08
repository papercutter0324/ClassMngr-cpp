#include "app/controllers/navigation_controller.h"
#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "core/resource_packs/resource_pack_manager.h"
#include "domain/models/teacher.h"
#include "features/campus/ui/campus_dashboard_page.h"
#include "features/teacher/ui/teacher_info_page.h"
#include "fakes/fake_user_prompt_service.h"
#include "ui/shared/dialogs/user_prompt_service.h"
#include "ui/shared/pages/pagemanager.h"
#include "ui/shared/widgets/sidebar/sidebar.h"
#include "ui/shared/widgets/sidebar/sidebar_types.h"

#include <QLineEdit>
#include <QMessageLogContext>
#include <QTemporaryDir>
#include <QTextEdit>
#include <QUuid>
#include <QVector>
#include <QtTest/QtTest>

#include <optional>

namespace
{

constexpr auto PersistedTeacherNotes = "Persisted teacher notes";
constexpr auto UnsavedTeacherNotes = "Exact unsaved teacher route notes";
constexpr auto UnsavedCampusName = "Unsaved campus name draft";

QString databasePath(QTemporaryDir& directory)
{
    return directory.filePath(
        QStringLiteral("campus-route-navigation-%1.tps").arg(
            QUuid::createUuid().toString(QUuid::WithoutBraces)
            )
        );
}

struct DirtyTeacherPage final
{
    Teacher teacher;
    TeacherInfoPage* page = nullptr;
    QTextEdit* notes = nullptr;
};

std::optional<DirtyTeacherPage> prepareDirtyTeacherPage(
    ApplicationServices& services,
    PageManager& pages,
    QString* error
    )
{
    Teacher teacher;
    teacher.teacherKr = QStringLiteral("\uAE40\uC120\uC0DD");
    teacher.teacherEn = QStringLiteral("Route Teacher");
    teacher.preferredRomanization = QStringLiteral("Route Teacher");
    teacher.preferredName = QStringLiteral("Route Teacher");
    teacher.notes = QString::fromLatin1(PersistedTeacherNotes);

    const auto created = services.teacherService()->create(teacher);
    if (!created)
    {
        if (error)
        {
            *error = created.error();
        }
        return std::nullopt;
    }
    teacher.id = *created;

    const auto persisted = services.teacherService()->teacher(teacher.id);
    if (!persisted)
    {
        if (error)
        {
            *error = persisted.error();
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
            *error = QStringLiteral("Teacher Info page was not initialized.");
        }
        return std::nullopt;
    }

    page->setSaveMode(SaveMode::Manual);
    page->loadTeacher(*persisted);
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

    notes->setPlainText(QString::fromLatin1(UnsavedTeacherNotes));
    if (!page->hasUnsavedChanges())
    {
        if (error)
        {
            *error = QStringLiteral("Teacher notes edit was not marked dirty.");
        }
        return std::nullopt;
    }

    return DirtyTeacherPage{
        .teacher = *persisted,
        .page = page,
        .notes = notes
    };
}

QString campusSectionLabel(const QString& sectionKey)
{
    if (sectionKey == QStringLiteral("campus_information"))
    {
        return QStringLiteral("Information");
    }
    if (sectionKey == QStringLiteral("campus_directions"))
    {
        return QStringLiteral("Directions");
    }
    if (sectionKey == QStringLiteral("campus_address"))
    {
        return QStringLiteral("Address");
    }
    if (sectionKey == QStringLiteral("campus_housing"))
    {
        return QStringLiteral("Housing");
    }
    if (sectionKey == QStringLiteral("campus_map"))
    {
        return QStringLiteral("Maps");
    }
    return {};
}

NavigationData campusRootRoute()
{
    return {
        .path = {QStringLiteral("Campus Directory")},
        .keys = {QStringLiteral("campus_info")},
        .routeKey = QStringLiteral("campus_info"),
        .type = NodeType::Root
    };
}

NavigationData campusPageRoute(const QString& sectionKey)
{
    return {
        .path = {
            QStringLiteral("Campus Directory"),
            campusSectionLabel(sectionKey)
        },
        .keys = {QStringLiteral("campus_info"), sectionKey},
        .routeKey = sectionKey,
        .type = NodeType::Page
    };
}

void connectCampusSectionChanges(PageManager& pages, Sidebar& sidebar)
{
    const auto connectCampusPage = [&sidebar](CampusDashboardPage* page)
    {
        if (page)
        {
            QObject::connect(
                page,
                &CampusDashboardPage::sectionChanged,
                &sidebar,
                &Sidebar::selectCampusSection
                );
        }
    };

    connectCampusPage(pages.campusDashboard());
    QObject::connect(
        &pages,
        &PageManager::pageCreated,
        &sidebar,
        [connectCampusPage](const PageType type, BasePage* page)
        {
            if (type == PageType::CampusDashboard)
            {
                connectCampusPage(
                    qobject_cast<CampusDashboardPage*>(page)
                    );
            }
        }
        );
}

void showCampusSection(
    CampusDashboardPage& page,
    const QString& sectionKey
    )
{
    if (sectionKey == QStringLiteral("campus_information"))
    {
        page.showInformation();
    }
    else if (sectionKey == QStringLiteral("campus_directions"))
    {
        page.showDirections();
    }
    else if (sectionKey == QStringLiteral("campus_address"))
    {
        page.showAddress();
    }
    else if (sectionKey == QStringLiteral("campus_housing"))
    {
        page.showHousing();
    }
    else if (sectionKey == QStringLiteral("campus_map"))
    {
        page.showMap();
    }
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

}

class CampusRouteNavigationParityTests final : public QObject
{
    Q_OBJECT

private slots:
    void campusRouteLeaveGuardMatrix_data();
    void campusRouteLeaveGuardMatrix();
};

void CampusRouteNavigationParityTests::campusRouteLeaveGuardMatrix_data()
{
    QTest::addColumn<QString>("origin");
    QTest::addColumn<QString>("routeShape");
    QTest::addColumn<QString>("routeKey");
    QTest::addColumn<QString>("leaveChoice");
    QTest::addColumn<QString>("initialCampusSection");
    QTest::addColumn<QString>("sidebarBeforeSection");
    QTest::addColumn<QString>("expectedSection");

    QTest::newRow("cancel-campus-root")
        << QStringLiteral("teacher")
        << QStringLiteral("root")
        << QStringLiteral("campus_info")
        << QStringLiteral("cancel")
        << QString()
        << QString()
        << QString();
    QTest::newRow("cancel-campus-address-page")
        << QStringLiteral("teacher")
        << QStringLiteral("page")
        << QStringLiteral("campus_address")
        << QStringLiteral("cancel")
        << QString()
        << QString()
        << QString();

    QTest::newRow("discard-campus-root-enters-information")
        << QStringLiteral("teacher")
        << QStringLiteral("root")
        << QStringLiteral("campus_info")
        << QStringLiteral("discard")
        << QString()
        << QString()
        << QStringLiteral("campus_information");
    QTest::newRow("discard-campus-information-page")
        << QStringLiteral("teacher")
        << QStringLiteral("page")
        << QStringLiteral("campus_information")
        << QStringLiteral("discard")
        << QString()
        << QString()
        << QStringLiteral("campus_information");
    QTest::newRow("discard-campus-directions-page")
        << QStringLiteral("teacher")
        << QStringLiteral("page")
        << QStringLiteral("campus_directions")
        << QStringLiteral("discard")
        << QString()
        << QString()
        << QStringLiteral("campus_directions");
    QTest::newRow("discard-campus-address-page")
        << QStringLiteral("teacher")
        << QStringLiteral("page")
        << QStringLiteral("campus_address")
        << QStringLiteral("discard")
        << QString()
        << QString()
        << QStringLiteral("campus_address");
    QTest::newRow("discard-campus-housing-page")
        << QStringLiteral("teacher")
        << QStringLiteral("page")
        << QStringLiteral("campus_housing")
        << QStringLiteral("discard")
        << QString()
        << QString()
        << QStringLiteral("campus_housing");
    QTest::newRow("discard-campus-map-page")
        << QStringLiteral("teacher")
        << QStringLiteral("page")
        << QStringLiteral("campus_map")
        << QStringLiteral("discard")
        << QString()
        << QString()
        << QStringLiteral("campus_map");

    QTest::newRow("current-dirty-campus-root-preserves-housing")
        << QStringLiteral("campus")
        << QStringLiteral("root")
        << QStringLiteral("campus_info")
        << QStringLiteral("none")
        << QStringLiteral("campus_housing")
        << QStringLiteral("campus_information")
        << QStringLiteral("campus_housing");
    QTest::newRow("current-dirty-campus-child-selects-map")
        << QStringLiteral("campus")
        << QStringLiteral("page")
        << QStringLiteral("campus_map")
        << QStringLiteral("none")
        << QStringLiteral("campus_information")
        << QStringLiteral("campus_address")
        << QStringLiteral("campus_map");
}

void CampusRouteNavigationParityTests::campusRouteLeaveGuardMatrix()
{
    QFETCH(QString, origin);
    QFETCH(QString, routeShape);
    QFETCH(QString, routeKey);
    QFETCH(QString, leaveChoice);
    QFETCH(QString, initialCampusSection);
    QFETCH(QString, sidebarBeforeSection);
    QFETCH(QString, expectedSection);

    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));

    PageManager pages;
    std::optional<DirtyTeacherPage> teacherState;
    CampusDashboardPage* campusPage = nullptr;
    QLineEdit* campusNameEdit = nullptr;

    if (origin == QStringLiteral("teacher"))
    {
        QString error;
        teacherState = prepareDirtyTeacherPage(services, pages, &error);
        QVERIFY2(teacherState.has_value(), qPrintable(error));
        QVERIFY(pages.isCurrentPage(PageType::TeacherInfo));
        QVERIFY(!pages.isPageInstantiated(PageType::CampusDashboard));
    }
    else
    {
        QCOMPARE(origin, QStringLiteral("campus"));
        pages.initialize(&services, true);
        pages.showPage(PageType::CampusDashboard);
        campusPage = pages.campusDashboard();
        QVERIFY(campusPage);
        campusPage->setSaveMode(SaveMode::Manual);
        showCampusSection(*campusPage, initialCampusSection);
        QCOMPARE(campusPage->currentSectionKey(), initialCampusSection);

        campusNameEdit = campusPage->findChild<QLineEdit*>(
            QStringLiteral("campusNameEdit")
            );
        QVERIFY(campusNameEdit);
        campusNameEdit->setText(QString::fromLatin1(UnsavedCampusName));
        QVERIFY(QMetaObject::invokeMethod(
            campusPage,
            "handleFieldEdited",
            Qt::DirectConnection
            ));
        QVERIFY(campusPage->hasUnsavedChanges());
    }

    const NavigationData route = routeShape == QStringLiteral("root")
        ? campusRootRoute()
        : campusPageRoute(routeKey);
    if (routeShape == QStringLiteral("root"))
    {
        QVERIFY(route.type == NodeType::Root);
        QCOMPARE(route.path, QStringList({QStringLiteral("Campus Directory")}));
        QCOMPARE(route.keys, QStringList({QStringLiteral("campus_info")}));
        QCOMPARE(route.routeKey, QStringLiteral("campus_info"));
    }
    else
    {
        QCOMPARE(routeShape, QStringLiteral("page"));
        QVERIFY(route.type == NodeType::Page);
        QCOMPARE(route.path.size(), 2);
        QCOMPARE(route.path.first(), QStringLiteral("Campus Directory"));
        QCOMPARE(route.path.last(), campusSectionLabel(routeKey));
        QCOMPARE(
            route.keys,
            QStringList({QStringLiteral("campus_info"), routeKey})
            );
        QCOMPARE(route.routeKey, routeKey);
    }

    Sidebar sidebar;
    connectCampusSectionChanges(pages, sidebar);
    if (!sidebarBeforeSection.isEmpty())
    {
        sidebar.selectCampusSection(sidebarBeforeSection);
        QCOMPARE(
            sidebar.selectedKeys(),
            QStringList({QStringLiteral("campus_info"), sidebarBeforeSection})
            );
    }
    else if (route.type == NodeType::Page)
    {
        // A clicked child is already the Sidebar's current item when the
        // application dispatches its NavigationData to the controller.
        sidebar.selectCampusSection(routeKey);
    }
    const QStringList sidebarKeysBefore = sidebar.selectedKeys();

    ResourcePackManager resources(
        directory.filePath(QStringLiteral("resources")),
        directory.filePath(QStringLiteral("baseline"))
        );
    NavigationController navigation(&services, &sidebar, &pages, resources);
    FakeUserPromptService prompts;
    if (leaveChoice == QStringLiteral("cancel"))
    {
        prompts.scriptedUnsavedChangesChoices.enqueue(
            UnsavedChangesChoice::Cancel
            );
    }
    else if (leaveChoice == QStringLiteral("discard"))
    {
        prompts.scriptedUnsavedChangesChoices.enqueue(
            UnsavedChangesChoice::Discard
            );
    }
    else
    {
        QCOMPARE(leaveChoice, QStringLiteral("none"));
    }
    ScopedPromptService promptOverride(prompts);
    QVector<QString> warnings;
    ScopedWarningCapture warningCapture(warnings);

    navigation.handleNavigation(route);
    QCoreApplication::processEvents();

    const bool expectLeaveConfirmation =
        origin == QStringLiteral("teacher");
    QCOMPARE(
        prompts.unsavedChangesConfirmations.size(),
        expectLeaveConfirmation ? 1 : 0
        );
    if (expectLeaveConfirmation)
    {
        QCOMPARE(
            prompts.unsavedChangesConfirmations.constFirst().title,
            QStringLiteral("Unsaved Teacher Changes")
            );
    }
    QVERIFY(prompts.messages.isEmpty());
    QVERIFY(prompts.asynchronousMessages.isEmpty());
    QVERIFY(prompts.confirmations.isEmpty());
    QVERIFY(prompts.actionPrompts.isEmpty());
    QVERIFY2(
        warnings.isEmpty(),
        qPrintable(warnings.isEmpty() ? QString() : warnings.constFirst())
        );
    QVERIFY(QApplication::activeModalWidget() == nullptr);

    if (leaveChoice == QStringLiteral("cancel"))
    {
        QVERIFY(teacherState.has_value());
        QVERIFY(pages.isCurrentPage(PageType::TeacherInfo));
        QCOMPARE(pages.currentWidget(), teacherState->page);
        QVERIFY(!pages.isCurrentPage(PageType::CampusDashboard));
        QVERIFY(!pages.isPageInstantiated(PageType::CampusDashboard));
        QVERIFY(pages.campusDashboard() == nullptr);
        QCOMPARE(teacherState->page->teacher().id, teacherState->teacher.id);
        QCOMPARE(teacherState->page->teacher().teacherEn,
            teacherState->teacher.teacherEn);
        QCOMPARE(teacherState->notes->toPlainText(),
            QString::fromLatin1(UnsavedTeacherNotes));
        QVERIFY(teacherState->page->hasUnsavedChanges());
        QCOMPARE(sidebar.selectedKeys(), sidebarKeysBefore);
        return;
    }

    if (origin == QStringLiteral("teacher"))
    {
        QVERIFY(teacherState.has_value());
        QVERIFY(pages.isCurrentPage(PageType::CampusDashboard));
        campusPage = pages.campusDashboard();
        QVERIFY(campusPage);
        QCOMPARE(pages.currentWidget(), static_cast<QWidget*>(campusPage));
        QCOMPARE(campusPage->currentSectionKey(), expectedSection);
        QCOMPARE(
            sidebar.selectedKeys(),
            QStringList({QStringLiteral("campus_info"), expectedSection})
            );
        QCOMPARE(teacherState->page->teacher().id, teacherState->teacher.id);
        QCOMPARE(teacherState->notes->toPlainText(),
            QString::fromLatin1(PersistedTeacherNotes));
        QVERIFY(!teacherState->page->hasUnsavedChanges());
    }
    else
    {
        QVERIFY(campusPage);
        QVERIFY(pages.isCurrentPage(PageType::CampusDashboard));
        QCOMPARE(pages.currentWidget(), static_cast<QWidget*>(campusPage));
        QCOMPARE(campusPage->currentSectionKey(), expectedSection);
        QCOMPARE(campusNameEdit->text(), QString::fromLatin1(UnsavedCampusName));
        QVERIFY(campusPage->hasUnsavedChanges());
        QCOMPARE(
            sidebar.selectedKeys(),
            QStringList({QStringLiteral("campus_info"), expectedSection})
            );
        QVERIFY(prompts.unsavedChangesConfirmations.isEmpty());
    }
}

QTEST_MAIN(CampusRouteNavigationParityTests)

#include "campus_route_navigation_parity_tests.moc"
