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
#include "ui/shared/pages/page_header.h"
#include "ui/shared/widgets/navigation_tab_widget.h"

#include <QApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLocale>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPalette>
#include <QPushButton>
#include <QQuickWidget>
#include <QScrollArea>
#include <QScrollBar>
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
#include <functional>

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

QRect widgetRectIn(const QWidget* widget, const QWidget* origin)
{
    if (!widget || !origin)
    {
        return {};
    }
    return QRect(widget->mapTo(origin, QPoint{}), widget->size());
}

QJsonObject rectJson(const QRect& rect)
{
    return {
        {QStringLiteral("x"), rect.x()},
        {QStringLiteral("y"), rect.y()},
        {QStringLiteral("width"), rect.width()},
        {QStringLiteral("height"), rect.height()}
    };
}

void writeCaptureFailureJson(
    const QString& outputImagePath,
    const QString& failureName,
    const QJsonObject& details
    )
{
    QFile file(QDir(QFileInfo(outputImagePath).absolutePath()).filePath(failureName));
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate))
    {
        file.write(QJsonDocument(details).toJson(QJsonDocument::Indented));
    }
}

QJsonArray legacyTabLabels(const NavigationTabWidget* tabs)
{
    QJsonArray result;
    if (tabs)
    {
        for (int index = 0; index < tabs->count(); ++index)
        {
            result.append(tabs->tabText(index));
        }
    }
    return result;
}

QJsonArray nextTabLabels(const QTabBar* tabs)
{
    QJsonArray result;
    if (tabs)
    {
        for (int index = 0; index < tabs->count(); ++index)
        {
            result.append(tabs->tabText(index));
        }
    }
    return result;
}

QJsonObject legacyWorkspaceShellLabels(const MyWorkspacePage& page)
{
    PageHeader* header = nullptr;
    for (PageHeader* candidate : page.findChildren<PageHeader*>())
    {
        if (candidate->parentWidget()
            && candidate->parentWidget()->parentWidget() == &page)
        {
            header = candidate;
            break;
        }
    }
    const auto* tabs = page.findChild<NavigationTabWidget*>(
        QStringLiteral("myWorkspaceTabs"));
    return {
        {QStringLiteral("title"), header ? header->title() : QString{}},
        {QStringLiteral("subtitle"), header ? header->subtitle() : QString{}},
        {QStringLiteral("tabs"), legacyTabLabels(tabs)}
    };
}

QJsonObject nextWorkspaceShellLabels(const Ui::MyWorkspacePage& page)
{
    const auto* title = page.findChild<QLabel*>(
        QStringLiteral("pageTitle"), Qt::FindDirectChildrenOnly);
    const auto* subtitle = page.findChild<QLabel*>(
        QStringLiteral("pageSubtitle"), Qt::FindDirectChildrenOnly);
    const auto* tabs = page.findChild<QTabBar*>(
        QStringLiteral("nextMyWorkspaceTabBar"));
    return {
        {QStringLiteral("title"), title ? title->text() : QString{}},
        {QStringLiteral("subtitle"), subtitle ? subtitle->text() : QString{}},
        {QStringLiteral("tabs"), nextTabLabels(tabs)}
    };
}

struct LowerUpcomingEventEvidence final
{
    QRect bounds;
    QString text;
};

QJsonObject scrollAreaJson(const QScrollArea* scrollArea, const QWidget* origin)
{
    if (!scrollArea || !scrollArea->verticalScrollBar())
    {
        return {};
    }
    const QScrollBar* bar = scrollArea->verticalScrollBar();
    return {
        {QStringLiteral("minimum"), bar->minimum()},
        {QStringLiteral("maximum"), bar->maximum()},
        {QStringLiteral("value"), bar->value()},
        {QStringLiteral("pageStep"), bar->pageStep()},
        {QStringLiteral("viewportBounds"), geometryJson(scrollArea->viewport(), origin)},
        {QStringLiteral("contentSize"), QJsonArray{
             scrollArea->widget() ? scrollArea->widget()->width() : 0,
             scrollArea->widget() ? scrollArea->widget()->height() : 0}}
    };
}

bool saveCapture(QWidget& page, const QString& path, QJsonObject* state, QString* error);
bool saveCurrentCapture(QWidget& page, const QString& path, QJsonObject* state, QString* error);

bool captureLowerUpcoming(
    QWidget& captureRoot,
    QScrollArea* scrollArea,
    QWidget* upcomingHeading,
    const std::function<LowerUpcomingEventEvidence()>& readRepresentativeEvent,
    const QString& outputPath,
    QJsonObject* state,
    QString* error
    )
{
    if (!scrollArea || !scrollArea->verticalScrollBar() || !upcomingHeading)
    {
        *error = QStringLiteral("The embedded Calendar Upcoming scroll surface is unavailable.");
        writeCaptureFailureJson(outputPath,
            QStringLiteral("lower-upcoming-failure.json"), QJsonObject{
                {QStringLiteral("error"), *error},
                {QStringLiteral("scrollAreaFound"), scrollArea != nullptr},
                {QStringLiteral("headingFound"), upcomingHeading != nullptr}
            });
        return false;
    }

    QScrollBar* const bar = scrollArea->verticalScrollBar();
    for (int attempt = 0; attempt < 6; ++attempt)
    {
        const int requestedMaximum = bar->maximum();
        bar->setValue(requestedMaximum);
        QApplication::processEvents();
        QApplication::processEvents();
        if (bar->maximum() == requestedMaximum
            && bar->value() == bar->maximum())
        {
            break;
        }
    }

    // The lower section heading can be partly clipped at the exact bottom of
    // the content. Align it back into the viewport by only the amount it is
    // clipped, keeping this a near-bottom capture while making the full heading
    // readable.
    int headingScrollAdjustment = 0;
    for (int attempt = 0; attempt < 3; ++attempt)
    {
        const QRect viewportBounds = widgetRectIn(scrollArea->viewport(), &captureRoot);
        const QRect headingBounds = widgetRectIn(upcomingHeading, &captureRoot);
        if (viewportBounds.contains(headingBounds))
        {
            break;
        }
        if (headingBounds.top() < viewportBounds.top())
        {
            const int adjustment = headingBounds.top() - viewportBounds.top();
            headingScrollAdjustment += adjustment;
            bar->setValue(bar->value() + adjustment);
        }
        else if (headingBounds.bottom() > viewportBounds.bottom())
        {
            const int adjustment = headingBounds.bottom() - viewportBounds.bottom();
            headingScrollAdjustment += adjustment;
            bar->setValue(bar->value() + adjustment);
        }
        QApplication::processEvents();
        QApplication::processEvents();
    }

    const QRect viewportBounds = widgetRectIn(scrollArea->viewport(), &captureRoot);
    const QRect headingBounds = widgetRectIn(upcomingHeading, &captureRoot);
    const LowerUpcomingEventEvidence event = readRepresentativeEvent
        ? readRepresentativeEvent()
        : LowerUpcomingEventEvidence{};
    const QRect currentEventBounds = event.bounds;
    const bool headingInFrame = viewportBounds.contains(headingBounds)
        && upcomingHeading->isVisibleTo(&captureRoot);
    const bool eventInFrame = !currentEventBounds.isEmpty()
        && !event.text.trimmed().isEmpty()
        && viewportBounds.intersects(currentEventBounds);
    if (bar->maximum() <= bar->minimum()
        || !headingInFrame || !eventInFrame)
    {
        *error = QStringLiteral(
            "The lower Upcoming capture is not in the embedded Calendar viewport. "
            "scroll=%1 heading=%2 event=%3")
            .arg(QString::fromUtf8(QJsonDocument(scrollAreaJson(scrollArea, &captureRoot))
                .toJson(QJsonDocument::Compact)))
            .arg(QString::fromUtf8(QJsonDocument(geometryJson(upcomingHeading, &captureRoot))
                .toJson(QJsonDocument::Compact)))
            .arg(QString::fromUtf8(QJsonDocument(rectJson(currentEventBounds))
                .toJson(QJsonDocument::Compact)));
        writeCaptureFailureJson(outputPath,
            QStringLiteral("lower-upcoming-failure.json"), QJsonObject{
                {QStringLiteral("error"), *error},
                {QStringLiteral("scroll"), scrollAreaJson(scrollArea, &captureRoot)},
                {QStringLiteral("viewportBounds"), rectJson(viewportBounds)},
                {QStringLiteral("headingBounds"), rectJson(headingBounds)},
                {QStringLiteral("headingText"), qobject_cast<QLabel*>(upcomingHeading)
                    ? qobject_cast<QLabel*>(upcomingHeading)->text() : QString{}},
                {QStringLiteral("headingVisibleToRoot"),
                    upcomingHeading->isVisibleTo(&captureRoot)},
                {QStringLiteral("eventBounds"), rectJson(currentEventBounds)},
                {QStringLiteral("eventText"), event.text},
                {QStringLiteral("headingInFrame"), headingInFrame},
                  {QStringLiteral("headingScrollAdjustment"), headingScrollAdjustment},
                {QStringLiteral("eventInFrame"), eventInFrame}
            });
        return false;
    }

    QJsonObject lowerCapture;
    if (!saveCurrentCapture(captureRoot, outputPath, &lowerCapture, error))
    {
        return false;
    }
    state->insert(QStringLiteral("lowerUpcomingCapture"), lowerCapture);
    state->insert(QStringLiteral("lowerUpcomingScroll"),
        scrollAreaJson(scrollArea, &captureRoot));
    state->insert(QStringLiteral("lowerUpcomingHeadingText"),
        qobject_cast<QLabel*>(upcomingHeading)
            ? qobject_cast<QLabel*>(upcomingHeading)->text()
            : QString{});
    state->insert(QStringLiteral("lowerUpcomingHeadingBounds"),
        geometryJson(upcomingHeading, &captureRoot));
    state->insert(QStringLiteral("lowerUpcomingRepresentativeEventBounds"),
        rectJson(currentEventBounds));
    state->insert(QStringLiteral("lowerUpcomingRepresentativeEventText"),
        event.text);
    state->insert(QStringLiteral("lowerUpcomingHeadingInFrame"), headingInFrame);
    state->insert(QStringLiteral("lowerUpcomingRepresentativeEventInFrame"), eventInFrame);
    state->insert(QStringLiteral("lowerUpcomingHeadingScrollAdjustment"),
        headingScrollAdjustment);
    return true;
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
    return saveCurrentCapture(page, path, state, error);
}

bool saveCurrentCapture(QWidget& page, const QString& path, QJsonObject* state, QString* error)
{
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
    const QString& stylesheet,
    const QString& outputPath,
    const QString& lowerOutputPath,
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
    page.setStyleSheet(stylesheet);
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
    const QJsonObject shellLabels = legacyWorkspaceShellLabels(page);
    if (shellLabels.value(QStringLiteral("title")).toString().isEmpty()
        || shellLabels.value(QStringLiteral("subtitle")).toString().isEmpty()
        || shellLabels.value(QStringLiteral("tabs")).toArray().size() != 3)
    {
        *error = QStringLiteral("The Legacy My Workspace shell labels were not found.");
        return false;
    }
    state->insert(QStringLiteral("workspaceShellLabels"), shellLabels);
    auto* const calendarViewport = calendar->findChild<QScrollArea*>();
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
    const QJsonObject viewportRect = state->value(
        QStringLiteral("calendarViewportRect")).toObject();
    const QJsonObject gridRect = state->value(
        QStringLiteral("calendarGridRect")).toObject();
    const int viewportY = viewportRect.value(QStringLiteral("y")).toInt(-1);
    const int viewportHeight = viewportRect.value(
        QStringLiteral("height")).toInt(-1);
    const bool legacyGeometryIsStable =
        viewportRect.value(QStringLiteral("x")).toInt(-1) == 44
        && viewportRect.value(QStringLiteral("width")).toInt(-1) == 936
        && viewportY >= 0
        && viewportHeight > 0
        && viewportY + viewportHeight <= 768
        && gridRect.value(QStringLiteral("x")).toInt(-1) == 90
        && gridRect.value(QStringLiteral("width")).toInt(-1) == 834
        && gridRect.value(QStringLiteral("height")).toInt(-1) == 840
        && gridRect.value(QStringLiteral("y")).toInt(-1) == viewportY + 54;
    if (!legacyGeometryIsStable)
    {
        *error = QStringLiteral(
            "The Legacy embedded Calendar capture geometry changed: viewport=%1 grid=%2.")
            .arg(QString::fromUtf8(QJsonDocument(
                state->value(QStringLiteral("calendarViewportRect")).toObject())
                .toJson(QJsonDocument::Compact)))
            .arg(QString::fromUtf8(QJsonDocument(
                state->value(QStringLiteral("calendarGridRect")).toObject())
                .toJson(QJsonDocument::Compact)));
        writeCaptureFailureJson(outputPath,
            QStringLiteral("legacy-calendar-geometry-failure.json"), QJsonObject{
                {QStringLiteral("error"), *error},
                {QStringLiteral("viewportRect"), viewportRect},
                {QStringLiteral("gridRect"), gridRect},
                {QStringLiteral("expectedStableInsets"), QJsonObject{
                     {QStringLiteral("viewportX"), 44},
                     {QStringLiteral("viewportWidth"), 936},
                     {QStringLiteral("gridX"), 90},
                     {QStringLiteral("gridWidth"), 834},
                     {QStringLiteral("gridHeight"), 840}}}
            });
        return false;
    }

    auto* const upcomingHeading = calendar->findChild<QLabel*>(
        QStringLiteral("sectionTitle"));
    const auto eventRows = calendar->findChildren<QWidget*>(
        QStringLiteral("upcomingCalendarEventRow"));
    QWidget* const representativeEvent = eventRows.isEmpty()
        ? nullptr : eventRows.front();
    if (!captureLowerUpcoming(
            page,
            calendarViewport,
            upcomingHeading,
            [&page, representativeEvent]
            {
                QStringList labels;
                if (representativeEvent)
                {
                    for (const QLabel* label :
                         representativeEvent->findChildren<QLabel*>())
                    {
                        if (!label->text().trimmed().isEmpty())
                        {
                            labels.append(label->text());
                        }
                    }
                }
                return LowerUpcomingEventEvidence{
                    .bounds = widgetRectIn(representativeEvent, &page),
                    .text = labels.join(QStringLiteral(" | "))
                };
            },
            lowerOutputPath,
            state,
            error))
    {
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
    const QString& stylesheet,
    const int tabIndex,
    const QString& outputPath,
    const QString& lowerOutputPath,
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
    host.setStyleSheet(stylesheet);
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
    const QJsonObject shellLabels = nextWorkspaceShellLabels(*nextPage);
    if (shellLabels.value(QStringLiteral("title")).toString().isEmpty()
        || shellLabels.value(QStringLiteral("subtitle")).toString().isEmpty()
        || shellLabels.value(QStringLiteral("tabs")).toArray().size() != 3)
    {
        *error = QStringLiteral("The Next My Workspace shell labels were not found.");
        return false;
    }
    state->insert(QStringLiteral("workspaceShellLabels"), shellLabels);

    const ProcessMemorySnapshot active = memoryProvider.snapshot();
    const auto* const scheduleTable = nextPage->findChild<QTableView*>(
        QStringLiteral("nextMyWorkspaceScheduleTable"));
    const auto* const embeddedCalendar = nextPage->findChild<Ui::CalendarPage*>(
        QStringLiteral("nextMyWorkspaceEmbeddedCalendar"));
    Ui::CalendarPageRuntimeMetrics activeCalendarMetrics;
    const QWidget* const embeddedCalendarGrid = embeddedCalendar
        ? embeddedCalendar->calendarViewWidget()
        : nullptr;
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
            const auto* const calendarCard = embeddedCalendarGrid
                ? embeddedCalendarGrid->parentWidget()
                : nullptr;
            calendarViewportRect = geometryJson(calendarViewport, nextPage);
            calendarGridRect = geometryJson(embeddedCalendarGrid, nextPage);
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
        && (!embeddedCalendar || !embeddedCalendarGrid
            || activeCalendarMetrics.calendarRootObjectCount == 0))
    {
        *error = QStringLiteral(
            "The embedded Next My Workspace Calendar content did not activate.");
        return false;
    }
    if (tabIndex == 2)
    {
        auto* const calendarScrollArea = nextPage->findChild<QScrollArea*>(
            QStringLiteral("nextMyWorkspaceCalendarScrollArea"));
        auto* const upcomingHeading = embeddedCalendar
            ? embeddedCalendar->findChild<QLabel*>(
                QStringLiteral("calendarUpcomingTitle"))
            : nullptr;
        auto* const upcomingList = embeddedCalendar
            ? embeddedCalendar->findChild<QListWidget*>(
                QStringLiteral("calendarUpcomingEvents"))
            : nullptr;
        if (!captureLowerUpcoming(
                host,
                calendarScrollArea,
                upcomingHeading,
                [&host, upcomingList]
                {
                    if (!upcomingList || upcomingList->count() == 0)
                    {
                        return LowerUpcomingEventEvidence{};
                    }
                    const QRect itemBounds = upcomingList->visualItemRect(
                        upcomingList->item(0));
                    return LowerUpcomingEventEvidence{
                        .bounds = QRect(
                            upcomingList->viewport()->mapTo(
                                &host, itemBounds.topLeft()),
                            itemBounds.size()),
                        .text = upcomingList->item(0)->text()
                    };
                },
                lowerOutputPath,
                state,
                error))
        {
            return false;
        }
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
                "The Next embedded Calendar viewport or grid geometry changed: viewport=%1 grid=%2.")
                .arg(QString::fromUtf8(QJsonDocument(calendarViewportRect)
                    .toJson(QJsonDocument::Compact)))
                .arg(QString::fromUtf8(QJsonDocument(calendarGridRect)
                    .toJson(QJsonDocument::Compact)));
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
            const QString stylesheet = readText(
                QDir(styles).filePath(theme + QStringLiteral(".qss")));
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
                const QString lowerUpcomingName = tabIndex == 2
                    ? QStringLiteral("next_my_workspace_calendar_upcoming_")
                        + suffix + QStringLiteral(".png")
                    : QString{};
                QJsonObject nextState;
                if (!captureNextSurface(
                        sourceDatabase,
                        loader,
                        preferences.model,
                        locale,
                        stylesheet,
                        tabIndex,
                        QDir(outputDirectory).filePath(name),
                        lowerUpcomingName.isEmpty()
                            ? QString{}
                            : QDir(outputDirectory).filePath(lowerUpcomingName),
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
                    const QString legacyLowerUpcomingName =
                        QStringLiteral("legacy_my_workspace_calendar_upcoming_")
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
                            stylesheet,
                            QDir(outputDirectory).filePath(legacyName),
                            QDir(outputDirectory).filePath(legacyLowerUpcomingName),
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
    const QString lightStylesheet = readText(
        QDir(styles).filePath(QStringLiteral("light.qss")));
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
            lightStylesheet,
            1,
            QDir(outputDirectory).filePath(
                QStringLiteral("next_schedule_96_classes_en_US_light.png")),
            QString{},
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
            "My Workspace page at a 1024x768 embedded viewport. Each Calendar tab "
            "has an initial capture and a second capture after its scroll range settles "
            "at the bottom; a minimal scroll offset then brings the full Upcoming heading "
            "into view while keeping a representative event row visible. Each lower capture "
            "records the resulting scroll value and range, heading adjustment, and event "
            "bounds. "
            "Next uses fixed upper-view rectangles 44,185 936x495 and 90,239 834x840; "
            "Legacy is checked for the same stable insets while allowing native Qt "
            "height metrics.")},
        {QStringLiteral("shellLabelEvidence"), QStringLiteral(
            "Each route records the title, subtitle, and tab labels read from the live "
            "Legacy or Next workspace widgets; no fixed localization divergence is asserted.")},
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
