#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "core/process_memory_snapshot.h"
#include "data/database/workspace_store.h"
#include "features/my_info/ui/my_workspace_page.h"
#include "features/calendar/ui/calendar_page.h"
#include "features/schedule/ui/schedule_page.h"
#include "next/platform/workspace_store_my_workspace_schedule_read_port.h"
#include "next/platform/workspace_store_calendar_actions.h"
#include "next/platform/workspace_store_personal_details_port.h"
#include "next/resources/resource_catalog.h"
#include "next/resources/resource_loader.h"
#include "next/ui/my_workspace_page.h"
#include "next/ui/page_host.h"
#include "preferences_test_ports.h"

#include <QApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QPalette>
#include <QPushButton>
#include <QQuickWidget>
#include <QScrollArea>
#include <QSqlQuery>
#include <QSqlError>
#include <QStackedWidget>
#include <QTabBar>
#include <QTableView>
#include <QTemporaryDir>
#include <QTranslator>

#include <memory>
#include <optional>
#include <string>
#include <utility>

using namespace ClassMngr;
using namespace ClassMngr::Next;

namespace
{

constexpr QSize Viewport{1024, 768};

QPalette themePalette(const QString& theme)
{
    QPalette palette;
    const bool dark = theme == QStringLiteral("dark");
    palette.setColor(QPalette::Window,
        QColor(dark ? QStringLiteral("#202326") : QStringLiteral("#f5f6f7")));
    palette.setColor(QPalette::WindowText,
        QColor(dark ? QStringLiteral("#f1f3f4") : QStringLiteral("#202326")));
    palette.setColor(QPalette::Base,
        QColor(dark ? QStringLiteral("#292c30") : QStringLiteral("#ffffff")));
    palette.setColor(QPalette::AlternateBase,
        QColor(dark ? QStringLiteral("#30343a") : QStringLiteral("#f4f6f8")));
    palette.setColor(QPalette::Button,
        QColor(dark ? QStringLiteral("#292c30") : QStringLiteral("#fcfcfc")));
    palette.setColor(QPalette::ButtonText,
        QColor(dark ? QStringLiteral("#fcfcfc") : QStringLiteral("#232629")));
    palette.setColor(QPalette::Text,
        QColor(dark ? QStringLiteral("#f1f3f4") : QStringLiteral("#202326")));
    palette.setColor(QPalette::Mid,
        QColor(dark ? QStringLiteral("#4b5056") : QStringLiteral("#b0b5bb")));
    palette.setColor(QPalette::Highlight, QColor(QStringLiteral("#3daee9")));
    palette.setColor(QPalette::HighlightedText, QColor(QStringLiteral("#ffffff")));
    return palette;
}

QString readText(const QString& path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly | QIODevice::Text)
        ? QString::fromUtf8(file.readAll())
        : QString{};
}

int widgetCount(const QWidget* root)
{
    return root ? root->findChildren<QWidget*>().size() + 1 : 0;
}

QJsonObject geometryJson(const QWidget* widget, const QWidget* origin)
{
    if (!widget || !origin)
    {
        return {};
    }
    const QPoint topLeft = widget->mapTo(origin, QPoint{});
    return {
        {QStringLiteral("x"), topLeft.x()},
        {QStringLiteral("y"), topLeft.y()},
        {QStringLiteral("width"), widget->width()},
        {QStringLiteral("height"), widget->height()}
    };
}

bool geometryMatches(
    const QJsonObject& geometry,
    const int x,
    const int y,
    const int width,
    const int height
    )
{
    return geometry.value(QStringLiteral("x")).toInt(-1) == x
        && geometry.value(QStringLiteral("y")).toInt(-1) == y
        && geometry.value(QStringLiteral("width")).toInt(-1) == width
        && geometry.value(QStringLiteral("height")).toInt(-1) == height;
}

QJsonObject memoryJson(const ProcessMemorySnapshot& snapshot)
{
    return {
        {QStringLiteral("available"), snapshot.isAvailable},
        {QStringLiteral("platform"), snapshot.platform},
        {QStringLiteral("workingSetBytes"),
         static_cast<double>(snapshot.workingSetBytes)},
        {QStringLiteral("peakWorkingSetBytes"),
         static_cast<double>(snapshot.peakWorkingSetBytes)},
        {QStringLiteral("privateUsageBytes"),
         static_cast<double>(snapshot.privateUsageBytes)},
        {QStringLiteral("privateWorkingSetBytes"),
         static_cast<double>(snapshot.privateWorkingSetBytes)},
        {QStringLiteral("capturedAt"),
         snapshot.capturedAt.toString(Qt::ISODateWithMs)}
    };
}

bool executeSql(WorkspaceStore& store, const QString& sql)
{
    QSqlQuery query(store.database());
    return query.exec(sql);
}

bool createWorkspaceFixture(
    const QString& path,
    QString* error,
    const int classCount = 6
    )
{
    WorkspaceStore store;
    const RepositoryStatus opened = store.open(path);
    if (!opened)
    {
        *error = QStringLiteral("Could not create the visual-capture workspace.");
        return false;
    }
    QFile personalFixture(QStringLiteral(CLASSMNGR_SOURCE_DIR)
        + QStringLiteral("/tests/fixtures/workspaces/initial_setup_personal_details.sql"));
    if (!personalFixture.open(QIODevice::ReadOnly)
        || !executeSql(store, QString::fromUtf8(personalFixture.readAll())))
    {
        *error = QStringLiteral("Could not seed personal details for capture.");
        return false;
    }

    const QStringList days{
        QStringLiteral("Monday"), QStringLiteral("Tuesday"),
        QStringLiteral("Wednesday"), QStringLiteral("Thursday"),
        QStringLiteral("Friday"), QStringLiteral("Saturday")
    };
    for (int index = 0; index < classCount; ++index)
    {
        QSqlQuery teacher(store.database());
        teacher.prepare(QStringLiteral(R"(
            INSERT INTO teachers (
                id, teacher_kr, teacher_en, preferred_name, room_number
            ) VALUES (?, ?, ?, ?, ?)
        )"));
        const int id = index + 1;
        teacher.addBindValue(id);
        teacher.addBindValue(QStringLiteral("선생님 %1").arg(id));
        teacher.addBindValue(QStringLiteral("Teacher %1").arg(id));
        teacher.addBindValue(QStringLiteral("Teacher %1").arg(id));
        teacher.addBindValue(QStringLiteral("Room %1").arg(100 + id));
        if (!teacher.exec())
        {
            *error = teacher.lastError().text();
            return false;
        }

        QSqlQuery classroom(store.database());
        classroom.prepare(QStringLiteral("INSERT INTO classes (id, name) VALUES (?, ?)"));
        classroom.addBindValue(101 + index);
        classroom.addBindValue(QStringLiteral("Level %1").arg(index + 1));
        if (!classroom.exec())
        {
            *error = classroom.lastError().text();
            return false;
        }

        QSqlQuery information(store.database());
        information.prepare(QStringLiteral(R"(
            INSERT INTO class_info (
                class_id, teacher_id, class_grade, class_level
            ) VALUES (?, ?, 'Grade 4', ?)
        )"));
        information.addBindValue(101 + index);
        information.addBindValue(id);
        information.addBindValue(QStringLiteral("Level %1").arg(index + 1));
        if (!information.exec())
        {
            *error = information.lastError().text();
            return false;
        }

        QSqlQuery time(store.database());
        time.prepare(QStringLiteral(R"(
            INSERT INTO class_times (class_id, day, start_time, end_time)
            VALUES (?, ?, ?, ?)
        )"));
        time.addBindValue(101 + index);
        time.addBindValue(days.at(index % days.size()));
        const int startHour = 9 + (index % 8);
        time.addBindValue(QStringLiteral("%1:00").arg(startHour, 2, 10, QLatin1Char('0')));
        time.addBindValue(QStringLiteral("%1:00").arg(startHour + 1, 2, 10, QLatin1Char('0')));
        if (!time.exec())
        {
            *error = time.lastError().text();
            return false;
        }
    }

    const QDate today = QDate::currentDate();
    for (int index = 0; index < 12; ++index)
    {
        const QString date = today.addDays(index).toString(Qt::ISODate);
        QSqlQuery event(store.database());
        event.prepare(QStringLiteral(R"(
            INSERT INTO calendar_events (
                title, event_type, time_status, all_day, start_date, start_time,
                end_date, end_time
            ) VALUES (?, 'Meeting', 'Timed', 0, ?, '09:00', ?, '10:00')
        )"));
        event.addBindValue(QStringLiteral("Workspace Event %1").arg(index + 1));
        event.addBindValue(date);
        event.addBindValue(date);
        if (!event.exec())
        {
            *error = event.lastError().text();
            return false;
        }
    }
    return true;
}

bool saveCapture(QWidget& page, const QString& path, QJsonObject* state, QString* error)
{
    page.resize(Viewport);
    page.show();
    QApplication::processEvents();
    if (QWidget* const focused = QApplication::focusWidget())
    {
        focused->clearFocus();
    }
    QApplication::processEvents();
    const QImage image = page.grab().toImage();
    const bool saved = image.size() == Viewport && image.save(path, "PNG");
    if (!saved)
    {
        *error = QStringLiteral(
            "Could not save a 1024x768 workspace capture at %1 (grabbed %2x%3, null=%4).")
            .arg(path)
            .arg(image.width())
            .arg(image.height())
            .arg(image.isNull());
        return false;
    }
    QFile captureFile(path);
    if (!captureFile.open(QIODevice::ReadOnly))
    {
        *error = QStringLiteral("Could not read the saved workspace capture.");
        return false;
    }
    const QByteArray sha256 = QCryptographicHash::hash(
        captureFile.readAll(), QCryptographicHash::Sha256).toHex();
    *state = {
        {QStringLiteral("image"), QFileInfo(path).fileName()},
        {QStringLiteral("sha256"), QString::fromLatin1(sha256)},
        {QStringLiteral("width"), image.width()},
        {QStringLiteral("height"), image.height()},
        {QStringLiteral("widgetCount"), widgetCount(&page)}
    };
    return true;
}

bool captureLegacySchedule(
    const QString& databasePath,
    const QString& outputPath,
    QJsonObject* state,
    QString* error
    )
{
    PlatformProcessMemorySnapshotProvider memoryProvider;
    const ProcessMemorySnapshot before = memoryProvider.snapshot();
    ApplicationServices services;
    const RepositoryStatus opened = services.openDatabase(databasePath);
    if (!opened)
    {
        *error = opened.error().diagnostic;
        return false;
    }
    MyWorkspacePage page(&services);
    page.setDatabaseOpen(true);
    page.openTab(WorkspaceTab::Schedule);
    if (!saveCapture(page, outputPath, state, error))
    {
        return false;
    }
    const ProcessMemorySnapshot active = memoryProvider.snapshot();
    const ScheduleWidgetRuntimeMetrics metrics = page.schedulePage()->runtimeMetrics();
    page.deactivate();
    QApplication::processEvents();
    const ProcessMemorySnapshot suspended = memoryProvider.snapshot();
    (*state).insert(QStringLiteral("route"), QStringLiteral("legacy_schedule"));
    (*state).insert(QStringLiteral("scheduleModelRowCount"), metrics.modelRowCount);
    (*state).insert(QStringLiteral("scheduleTableRowCount"), metrics.tableRowCount);
    (*state).insert(QStringLiteral("scheduleTableCellWidgetCount"),
        metrics.tableCellWidgetCount);
    (*state).insert(QStringLiteral("memoryProcessWide"), QJsonObject{
        {QStringLiteral("before"), memoryJson(before)},
        {QStringLiteral("active"), memoryJson(active)},
        {QStringLiteral("suspended"), memoryJson(suspended)}
    });
    return true;
}

bool captureLegacyCalendar(
    const QString& databasePath,
    const QString& outputPath,
    QJsonObject* state,
    QString* error
    )
{
    PlatformProcessMemorySnapshotProvider memoryProvider;
    const ProcessMemorySnapshot before = memoryProvider.snapshot();
    ApplicationServices services;
    const RepositoryStatus opened = services.openDatabase(databasePath);
    if (!opened)
    {
        *error = opened.error().diagnostic;
        return false;
    }
    MyWorkspacePage page(&services);
    page.setDatabaseOpen(true);
    page.openTab(WorkspaceTab::Calendar);
    QApplication::processEvents();
    ::CalendarPage* const calendar = page.calendarPage();
    if (!calendar)
    {
        *error = QStringLiteral(
            "The Legacy My Workspace Calendar tab did not construct its page.");
        return false;
    }
    const ::CalendarPageRuntimeMetrics metrics = calendar->runtimeMetrics();
    if (metrics.calendarViewObjectCount < 1)
    {
        *error = QStringLiteral(
            "The Legacy embedded Calendar month grid was not constructed.");
        return false;
    }
    if (!saveCapture(page, outputPath, state, error))
    {
        return false;
    }
    const auto* const calendarViewport = calendar->findChild<QScrollArea*>();
    const auto* const calendarGrid = calendar->findChild<QQuickWidget*>();
    const auto* const calendarCard = calendarGrid
        ? calendarGrid->parentWidget()
        : nullptr;
    (*state).insert(QStringLiteral("calendarViewportRect"),
        geometryJson(calendarViewport, &page));
    (*state).insert(QStringLiteral("calendarGridRect"),
        geometryJson(calendarGrid, &page));
    (*state).insert(QStringLiteral("calendarCardRect"),
        geometryJson(calendarCard, &page));
    if (!geometryMatches(state->value(QStringLiteral("calendarViewportRect")).toObject(),
                         44, 185, 936, 495)
        || !geometryMatches(state->value(QStringLiteral("calendarGridRect")).toObject(),
                            90, 239, 834, 840))
    {
        *error = QStringLiteral("The Legacy embedded Calendar capture geometry changed.");
        return false;
    }
    const ProcessMemorySnapshot active = memoryProvider.snapshot();
    page.deactivate();
    QApplication::processEvents();
    const ProcessMemorySnapshot suspended = memoryProvider.snapshot();
    (*state).insert(QStringLiteral("route"),
        QStringLiteral("legacy_my_workspace_calendar"));
    (*state).insert(QStringLiteral("calendarCacheEventCount"),
        metrics.cacheEventCount);
    (*state).insert(QStringLiteral("calendarViewObjectCount"),
        metrics.calendarViewObjectCount);
    (*state).insert(QStringLiteral("calendarCacheLoading"),
        metrics.cacheLoading);
    (*state).insert(QStringLiteral("memoryProcessWide"), QJsonObject{
        {QStringLiteral("before"), memoryJson(before)},
        {QStringLiteral("active"), memoryJson(active)},
        {QStringLiteral("suspended"), memoryJson(suspended)}
    });
    return true;
}

bool captureNextSurface(
    const QString& databasePath,
    Resources::ResourceLoader& loader,
    Application::PreferencesModel& preferences,
    const QString& locale,
    const int tabIndex,
    const QString& outputPath,
    QJsonObject* state,
    QString* error
    )
{
    WorkspaceStore store;
    const RepositoryStatus opened = store.openExisting(databasePath);
    if (!opened)
    {
        *error = opened.error().diagnostic;
        return false;
    }

    PlatformProcessMemorySnapshotProvider memoryProvider;
    const ProcessMemorySnapshot before = memoryProvider.snapshot();
    Ui::PageHost host(loader, preferences);
    Ui::MyWorkspacePage* nextPage = nullptr;
    int detailsReadCount = 0;
    Ui::PageDescriptor workspace{
        .id = *Ui::PageId::fromString(QStringLiteral("my_workspace")),
        .label = QStringLiteral("My Workspace"),
        .iconResourceId = std::nullopt,
        .requiredPermissions = {},
        .factory = [&store, &nextPage, &detailsReadCount, databasePath](
            const Ui::PageContext& context)
            -> Ui::PageOperationResult<std::unique_ptr<Ui::Page>> {
            Ui::MyWorkspacePageActions actions;
            actions.workspaceIdentity = [] { return QStringLiteral("capture-workspace"); };
            actions.readDetails = [&store, &detailsReadCount] {
                ++detailsReadCount;
                return Platform::WorkspaceStorePersonalDetailsPort(store).read();
            };
            actions.readSchedule = [&store] {
                const Platform::WorkspaceStoreMyWorkspaceScheduleReadPort port(store);
                return Application::MyWorkspaceScheduleReadUseCase::execute(port);
            };
            actions.createCalendarActions = [databasePath](
                const Ui::PageContext&) {
                Ui::CalendarPageActions calendar;
                calendar.activeWorkspacePath = [databasePath] {
                    return databasePath;
                };
                calendar.readDisplayPreferences = [](const QString& path) {
                    return Platform::readCalendarEventDisplayPreferencesAtPath(path);
                };
                calendar.readCurrentCampus = [](const QString& path) {
                    return Platform::readCurrentCampusAtPath(path);
                };
                calendar.readCampusDirectory = [] {
                    return std::vector<Application::CalendarPageCampusMetadata>{};
                };
                return calendar;
            };
            auto page = std::make_unique<Ui::MyWorkspacePage>(context, std::move(actions));
            nextPage = page.get();
            return std::unique_ptr<Ui::Page>(std::move(page));
        }
    };
    Ui::PageDescriptor other{
        .id = *Ui::PageId::fromString(QStringLiteral("other")),
        .label = QStringLiteral("Other"),
        .factory = [](const Ui::PageContext& context)
            -> Ui::PageOperationResult<std::unique_ptr<Ui::Page>> {
            return std::unique_ptr<Ui::Page>(std::make_unique<Ui::Page>(context));
        }
    };
    if (!host.registerPage(std::move(workspace))
        || !host.registerPage(std::move(other))
        || !host.activatePage(QStringLiteral("my_workspace"))
        || !nextPage)
    {
        *error = QStringLiteral("The Next My Workspace page could not activate.");
        return false;
    }

    host.resize(Viewport);
    host.show();
    QApplication::processEvents();
    auto* const tabs = nextPage->findChild<QTabBar*>(
        QStringLiteral("nextMyWorkspaceTabBar"));
    if (!tabs)
    {
        *error = QStringLiteral("The Next workspace tab selector was not found.");
        return false;
    }
    tabs->setCurrentIndex(tabIndex);
    QApplication::processEvents();
    if (!saveCapture(host, outputPath, state, error))
    {
        return false;
    }

    const ProcessMemorySnapshot active = memoryProvider.snapshot();
    const auto* const scheduleTable = nextPage->findChild<QTableView*>(
        QStringLiteral("nextMyWorkspaceScheduleTable"));
    const auto* const embeddedCalendar = nextPage->findChild<Ui::CalendarPage*>(
        QStringLiteral("nextMyWorkspaceEmbeddedCalendar"));
    Ui::CalendarPageRuntimeMetrics activeCalendarMetrics;
    QJsonObject calendarViewportRect;
    QJsonObject calendarCardRect;
    QJsonObject calendarGridRect;
    QJsonObject workspaceTabPagesRect;
    QJsonObject workspaceTabBarRect;
    if (embeddedCalendar)
    {
        activeCalendarMetrics = embeddedCalendar->runtimeMetrics();
        if (tabIndex == 2)
        {
            const auto* const calendarViewport = nextPage->findChild<QScrollArea*>(
                QStringLiteral("nextMyWorkspaceCalendarScrollArea"));
            const auto* const calendarGrid = embeddedCalendar->calendarView();
            const auto* const calendarCard = calendarGrid
                ? calendarGrid->parentWidget()
                : nullptr;
            calendarViewportRect = geometryJson(calendarViewport, nextPage);
            calendarGridRect = geometryJson(calendarGrid, nextPage);
            calendarCardRect = geometryJson(calendarCard, nextPage);
            workspaceTabPagesRect = geometryJson(
                nextPage->findChild<QStackedWidget*>(
                    QStringLiteral("nextMyWorkspaceTabPages")),
                nextPage);
            workspaceTabBarRect = geometryJson(
                nextPage->findChild<QTabBar*>(
                    QStringLiteral("nextMyWorkspaceTabBar")),
                nextPage);
        }
    }
    if (tabIndex == 1 && scheduleTable && scheduleTable->model())
    {
        const QString firstScheduleDay = scheduleTable->model()->data(
            scheduleTable->model()->index(0, 0)).toString();
        const QString expectedDay = locale == QStringLiteral("ko_KR")
            ? QStringLiteral("월요일") : QStringLiteral("Monday");
        if (firstScheduleDay != expectedDay)
        {
            *error = QStringLiteral("Schedule weekday translation mismatch: %1")
                .arg(firstScheduleDay);
            return false;
        }
        (*state).insert(QStringLiteral("firstScheduleDayLabel"), firstScheduleDay);
    }
    const int rows = tabIndex == 1 && scheduleTable && scheduleTable->model()
        ? scheduleTable->model()->rowCount()
        : (embeddedCalendar
            ? activeCalendarMetrics.upcomingResultCount
            : 0);
    if (tabIndex == 2
        && (!embeddedCalendar || !embeddedCalendar->calendarView()
            || !embeddedCalendar->calendarView()->rootObject()))
    {
        *error = QStringLiteral(
            "The embedded Next My Workspace Calendar content did not activate.");
        return false;
    }
    const int modelCount = static_cast<int>(nextPage->findChildren<QTableView*>().size());
    static_cast<void>(host.activatePage(QStringLiteral("other")));
    QApplication::processEvents();
    const ProcessMemorySnapshot suspended = memoryProvider.snapshot();
    (*state).insert(QStringLiteral("route"), tabIndex == 1
        ? QStringLiteral("next_schedule")
        : QStringLiteral("next_my_workspace_calendar"));
    (*state).insert(QStringLiteral("activeTabIndex"), tabIndex);
    (*state).insert(QStringLiteral("activeSurfaceRowCount"), rows);
    (*state).insert(QStringLiteral("ownedTableModelViewCount"), modelCount);
    if (tabIndex == 2 && embeddedCalendar)
    {
        if (!geometryMatches(calendarViewportRect, 44, 185, 936, 495)
            || !geometryMatches(calendarGridRect, 90, 239, 834, 840))
        {
            *error = QStringLiteral(
                "The Next embedded Calendar viewport or grid no longer aligns with Legacy.");
            return false;
        }
        (*state).insert(QStringLiteral("workspaceTabPagesRect"),
            workspaceTabPagesRect);
        (*state).insert(QStringLiteral("workspaceTabBarRect"),
            workspaceTabBarRect);
        (*state).insert(QStringLiteral("calendarViewportRect"),
            calendarViewportRect);
        (*state).insert(QStringLiteral("calendarGridRect"), calendarGridRect);
        (*state).insert(QStringLiteral("calendarCardRect"), calendarCardRect);
        (*state).insert(QStringLiteral("calendarViewCount"),
            activeCalendarMetrics.calendarViewCount);
        (*state).insert(QStringLiteral("calendarRootObjectCount"),
            activeCalendarMetrics.calendarRootObjectCount);
        (*state).insert(QStringLiteral("calendarCacheEventCount"),
            activeCalendarMetrics.cacheEventCount);
        (*state).insert(QStringLiteral("calendarUpcomingRangeDayCount"),
            activeCalendarMetrics.upcomingRangeDayCount);
        (*state).insert(QStringLiteral("liveHeavyCalendarSessionCount"),
            activeCalendarMetrics.liveHeavySessionCount);
    }
    (*state).insert(QStringLiteral("personalDetailsReadCount"), detailsReadCount);
    (*state).insert(QStringLiteral("scheduleRowsAfterSuspension"),
        scheduleTable && scheduleTable->model() ? scheduleTable->model()->rowCount() : 0);
    (*state).insert(QStringLiteral("calendarViewCountAfterSuspension"),
        embeddedCalendar
            ? embeddedCalendar->runtimeMetrics().calendarViewCount
            : 0);
    (*state).insert(QStringLiteral("memoryProcessWide"), QJsonObject{
        {QStringLiteral("before"), memoryJson(before)},
        {QStringLiteral("active"), memoryJson(active)},
        {QStringLiteral("suspended"), memoryJson(suspended)}
    });
    return true;
}

} // namespace

int main(int argc, char* argv[])
{
    QApplication application(argc, argv);
    QApplication::setStyle(QStringLiteral("Fusion"));
    if (argc != 2)
    {
        qCritical("Usage: ClassMngrNextMyWorkspaceVisualCapture OUTPUT_DIR");
        return 2;
    }
    const QString outputDirectory = QString::fromLocal8Bit(argv[1]);
    if (!QDir().mkpath(outputDirectory))
    {
        qCritical("Could not create capture output directory");
        return 2;
    }
    QTemporaryDir temporaryDirectory;
    QString error;
    const QString sourceDatabase = temporaryDirectory.filePath(QStringLiteral("workspace.tps"));
    if (!temporaryDirectory.isValid()
        || !createWorkspaceFixture(sourceDatabase, &error))
    {
        qCritical().noquote() << error;
        return 1;
    }

    const QString resourceRoot = QStringLiteral(CLASSMNGR_NEXT_TEST_RESOURCE_ROOT);
    const auto loadedCatalog = Resources::ResourceCatalog::load(
        QStringLiteral(CLASSMNGR_NEXT_TEST_RESOURCE_CATALOG), resourceRoot);
    if (!loadedCatalog)
    {
        qCritical().noquote() << "Could not load Next resource catalog:"
                              << loadedCatalog.error().message;
        return 1;
    }
    Resources::ResourceLoader loader(std::move(*loadedCatalog));
    Test::MemoryPreferences preferences;
    const QString translations = QStringLiteral(
        CLASSMNGR_NEXT_TEST_TRANSLATION_DIRECTORY);
    const QString styles = QDir(resourceRoot).filePath(QStringLiteral("assets/styles"));
    const QStringList locales{QStringLiteral("en_US"), QStringLiteral("ko_KR")};
    const QStringList themes{QStringLiteral("light"), QStringLiteral("dark")};
    QJsonArray captures;

    for (const QString& locale : locales)
    {
        for (const QString& theme : themes)
        {
            QLocale::setDefault(QLocale(locale));
            application.setPalette(themePalette(theme));
            application.setStyleSheet(readText(
                QDir(styles).filePath(theme + QStringLiteral(".qss"))));
            std::unique_ptr<QTranslator> translator;
            if (locale == QStringLiteral("ko_KR"))
            {
                translator = std::make_unique<QTranslator>();
                if (!translator->load(QDir(translations).filePath(
                        QStringLiteral("ClassMngr_ko_KR.qm")))
                    || !application.installTranslator(translator.get()))
                {
                    qCritical("Could not install Korean translation");
                    return 1;
                }
            }

            for (int tabIndex : {1, 2})
            {
                const QString suffix = locale + QLatin1Char('_') + theme;
                const QString name = tabIndex == 1
                    ? QStringLiteral("next_schedule_") + suffix + QStringLiteral(".png")
                    : QStringLiteral("next_my_workspace_calendar_")
                        + suffix + QStringLiteral(".png");
                QJsonObject nextState;
                if (!captureNextSurface(
                        sourceDatabase,
                        loader,
                        preferences.model,
                        locale,
                        tabIndex,
                        QDir(outputDirectory).filePath(name),
                        &nextState,
                        &error))
                {
                    qCritical().noquote() << error;
                    return 1;
                }
                nextState.insert(QStringLiteral("locale"), locale);
                nextState.insert(QStringLiteral("theme"), theme);
                captures.append(nextState);

                if (tabIndex == 2)
                {
                    const QString legacyName = QStringLiteral("legacy_my_workspace_calendar_")
                        + suffix + QStringLiteral(".png");
                    const QString legacyDatabase = temporaryDirectory.filePath(
                        QStringLiteral("legacy-") + suffix + QStringLiteral(".tps"));
                    QFile::remove(legacyDatabase);
                    if (!QFile::copy(sourceDatabase, legacyDatabase))
                    {
                        qCritical("Could not copy the legacy capture database");
                        return 1;
                    }
                    QJsonObject legacyState;
                    if (!captureLegacyCalendar(
                            legacyDatabase,
                            QDir(outputDirectory).filePath(legacyName),
                            &legacyState,
                            &error))
                    {
                        qCritical().noquote() << error;
                        return 1;
                    }
                    legacyState.insert(QStringLiteral("locale"), locale);
                    legacyState.insert(QStringLiteral("theme"), theme);
                    captures.append(legacyState);
                }
            }
            if (translator)
            {
                application.removeTranslator(translator.get());
            }
        }
    }

    QLocale::setDefault(QLocale(QStringLiteral("en_US")));
    application.setPalette(themePalette(QStringLiteral("light")));
    application.setStyleSheet(readText(
        QDir(styles).filePath(QStringLiteral("light.qss"))));
    const QString largeDatabase = temporaryDirectory.filePath(
        QStringLiteral("workspace-96-classes.tps"));
    if (!createWorkspaceFixture(largeDatabase, &error, 96))
    {
        qCritical().noquote() << error;
        return 1;
    }
    QJsonObject largeState;
    if (!captureNextSurface(
            largeDatabase,
            loader,
            preferences.model,
            QStringLiteral("en_US"),
            1,
            QDir(outputDirectory).filePath(
                QStringLiteral("next_schedule_96_classes_en_US_light.png")),
            &largeState,
            &error))
    {
        qCritical().noquote() << error;
        return 1;
    }
    if (largeState.value(QStringLiteral("activeSurfaceRowCount")).toInt() != 96
        || largeState.value(QStringLiteral("ownedTableModelViewCount")).toInt() != 1)
    {
        qCritical("The 96-class schedule capture did not retain its bounded model shape");
        return 1;
    }
    largeState.insert(QStringLiteral("locale"), QStringLiteral("en_US"));
    largeState.insert(QStringLiteral("theme"), QStringLiteral("light"));
    largeState.insert(QStringLiteral("scaleScenario"), QStringLiteral("96 classes"));
    captures.append(largeState);

    QJsonObject manifest{
        {QStringLiteral("viewport"), QStringLiteral("1024x768")},
        {QStringLiteral("style"), QStringLiteral("Fusion")},
        {QStringLiteral("checksumAlgorithm"), QStringLiteral("SHA-256")},
        {QStringLiteral("scenarioCount"), captures.size()},
        {QStringLiteral("memoryScope"), QStringLiteral(
            "Process-wide snapshots; differences include the full Qt application, "
            "workspace stores, repositories, translations, widgets, and allocator behavior.")},
        {QStringLiteral("calendarCapture"), QStringLiteral(
            "Paired Legacy and Next captures show the actual Calendar tab inside each "
            "My Workspace page, at a 1024x768 embedded viewport. The visible viewport "
            "and month grid share identical rectangles: viewport 44,185 936x495; grid "
            "90,239 834x840.")},
        {QStringLiteral("localizedDivergence"), QStringLiteral(
            "In ko_KR, the Legacy My Workspace parent title, subtitle, and tab labels "
            "remain English while its Calendar labels are Korean. Next keeps the Korean "
            "My Workspace and Calendar translations, as required for Phase 7.")},
        {QStringLiteral("captures"), captures}
    };
    QFile manifestFile(QDir(outputDirectory).filePath(QStringLiteral("manifest.json")));
    if (!manifestFile.open(QIODevice::WriteOnly | QIODevice::Truncate)
        || manifestFile.write(QJsonDocument(manifest).toJson(QJsonDocument::Indented)) < 0)
    {
        qCritical("Could not write the workspace capture manifest");
        return 1;
    }
    return 0;
}
