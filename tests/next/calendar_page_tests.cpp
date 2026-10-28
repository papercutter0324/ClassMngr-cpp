#include "core/process_memory_snapshot.h"
#include "data/database/workspace_store.h"
#include "features/calendar/academic_calendar_schedule.h"
#include "features/calendar/ui/calendar_event_model.h"
#include "next/resources/resource_catalog.h"
#include "next/resources/resource_loader.h"
#include "next/resources/resource_scope.h"
#include "next/platform/calendar_page_resource_campus_directory_query_adapter.h"
#include "next/platform/application_services_academic_calendar_schedule_preferences_port.h"
#include "next/platform/application_services_calendar_first_day_of_week_preferences_port.h"
#include "next/platform/application_services_calendar_event_save_port.h"
#include "next/application/calendar_event_save_use_case.h"
#include "next/ui/calendar_event_dialog.h"
#include "next/ui/calendar_page.h"
#include "next/ui/calendar_preferences_dialog.h"
#include "next/ui/page_host.h"
#include "preferences_test_ports.h"

#include <QApplication>
#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QDateEdit>
#include <QDateTime>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QJsonDocument>
#include <QJsonArray>
#include <QLabel>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPalette>
#include <QPushButton>
#include <QQuickItem>
#include <QQuickWidget>
#include <QSpinBox>
#include <QStyle>
#include <QTemporaryDir>
#include <QTimer>
#include <QTranslator>
#include <QtTest/QtTest>

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

using namespace ClassMngr::Next;

namespace
{

class OtherPage final : public Ui::Page
{
public:
    using Ui::Page::Page;
};

class CalendarPageFixture final
{
public:
    Test::MemoryPreferences preferences;
    std::unique_ptr<QTranslator> koreanTranslator;
    std::unique_ptr<Resources::ResourceCatalog> catalog;
    std::unique_ptr<Resources::ResourceLoader> loader;
    std::unique_ptr<Ui::PageHost> host;
    QTemporaryDir temporaryDirectory;
    QString initializationError;
    QString path;
    std::string holidayColorOverride;
    Ui::CalendarPage* page = nullptr;
    bool lazyBeforeActivation = false;
    bool failNextEventSave = false;
    bool failNextRepeatSeriesEdit = false;
    bool failNextRepeatSeriesDelete = false;
    bool readRepeatEvent = false;
    int saveEventCalls = 0;
    int deleteEventCalls = 0;
    int createSeriesCalls = 0;
    int editRepeatSeriesCalls = 0;
    int deleteRepeatSeriesCalls = 0;
    int saveDisplayPreferencesCalls = 0;
    int saveColorCalls = 0;
    int deleteAllEventsCalls = 0;
    std::optional<Application::CalendarEventSaveInput> savedEvent;
    std::optional<Domain::CalendarEventId> deletedEventId;
    std::optional<Application::CalendarEventSeriesCreateInput> createdSeries;
    std::optional<Application::CalendarEventSeriesEditInput> editedRepeatSeries;
    std::optional<Application::CalendarEventSeriesDeleteInput> deletedRepeatSeries;
    std::optional<Application::CalendarEventDisplayPreferencesSaveRequest>
        savedDisplayPreferences;

    bool initialize(
        const bool createWorkspace,
        const bool korean = false,
        const bool seedCalendar = false)
    {
        if (createWorkspace)
        {
            if (!temporaryDirectory.isValid())
            {
                initializationError = QStringLiteral("temporary directory unavailable");
                return false;
            }
            path = temporaryDirectory.filePath(QStringLiteral("calendar.db"));
            WorkspaceStore store;
            if (!store.open(path))
            {
                initializationError = QStringLiteral("workspace store open failed");
                return false;
            }
            if (seedCalendar)
            {
                Platform::ApplicationServicesCalendarFirstDayOfWeekPreferencesPort
                    firstDayOfWeek(store);
                if (!firstDayOfWeek.save(
                        Application::CalendarFirstDayOfWeek::Monday))
                {
                    initializationError = QStringLiteral(
                        "calendar capture first-day preference save failed");
                    return false;
                }
                Platform::ApplicationServicesCalendarEventSavePort savePort(store);
                const auto save = [&savePort](
                    const QString& title,
                    const QDate& date,
                    const char* type,
                    const bool allDay)
                {
                    Application::CalendarEventSaveInput input{
                        .title = title.toUtf8().toStdString(),
                        .startDate = date.toString(Qt::ISODate).toStdString(),
                        .endDate = date.toString(Qt::ISODate).toStdString(),
                        .startTime = allDay
                            ? std::nullopt
                            : std::optional<std::string>("09:30"),
                        .endTime = allDay
                            ? std::nullopt
                            : std::optional<std::string>("10:30"),
                        .allDay = allDay,
                        .eventType = type
                    };
                    return Application::CalendarEventSaveUseCase::execute(
                        savePort, input).hasValue();
                };
                if (!save(QStringLiteral("Campus meeting"), QDate(2026, 10, 6),
                          "Meeting", false)
                    || !save(QStringLiteral("Term planning"), QDate(2026, 10, 15),
                             "Workshop", false)
                    || !save(QStringLiteral("School holiday"), QDate(2026, 10, 26),
                             "Holiday", true))
                {
                    initializationError = QStringLiteral("calendar seed save failed");
                    return false;
                }
            }
        }

        auto loadedCatalog = Resources::ResourceCatalog::load(
            QStringLiteral(CLASSMNGR_NEXT_TEST_RESOURCE_CATALOG),
            QStringLiteral(CLASSMNGR_NEXT_TEST_RESOURCE_ROOT));
        if (!loadedCatalog)
        {
            initializationError = QStringLiteral("Next resource catalog load failed");
            return false;
        }
        catalog = std::make_unique<Resources::ResourceCatalog>(
            std::move(*loadedCatalog));
        loader = std::make_unique<Resources::ResourceLoader>(*catalog);
        if (korean)
        {
            if (!loadKoreanTranslation())
            {
                return false;
            }
        }
        host = std::make_unique<Ui::PageHost>(*loader, preferences.model);

        Ui::CalendarPageActions actions;
        actions.activeWorkspacePath = [this] { return path; };
        actions.readEvent = [this](
            const QString&,
            const Domain::CalendarEventId& id)
        {
            Application::CalendarEventSummary event{
                .id = id,
                .title = "Test event",
                .startDate = "2026-07-10",
                .endDate = "2026-07-10",
                .startTime = std::string("09:00"),
                .endTime = std::string("10:00"),
                .eventType = Application::CalendarEventTypeName("Meeting")
            };
            if (readRepeatEvent)
            {
                event.repeatSeriesId = Domain::CalendarEventSeriesId::fromString(
                    "series-42");
            }
            return Application::CalendarEventByIdQueryResult::success(event);
        };
        actions.saveEvent = [this](
            const QString&,
            const Application::CalendarEventSaveInput& input)
        {
            ++saveEventCalls;
            if (failNextEventSave)
            {
                failNextEventSave = false;
                return Application::CalendarEventSaveResult::failure({
                    .code = Domain::ErrorCode::Technical,
                    .message = "The test mutation failed.",
                    .recoverable = true
                });
            }
            savedEvent = input;
            return Application::CalendarEventSaveResult::success(
                *Domain::CalendarEventId::fromString("123"));
        };
        actions.deleteEvent = [this](
            const QString&,
            const Domain::CalendarEventId& id)
        {
            ++deleteEventCalls;
            deletedEventId = id;
            return Application::CalendarEventDeleteResult::success();
        };
        actions.createRepeatSeries = [this](
            const QString&,
            const Application::CalendarEventSeriesCreateInput& input)
        {
            ++createSeriesCalls;
            createdSeries = input;
            return Application::CalendarEventSeriesCreateResult::success({
                *Domain::CalendarEventId::fromString("124")
            });
        };
        actions.editRepeatSeries = [this](
            const QString&,
            const Application::CalendarEventSeriesEditInput& input)
        {
            ++editRepeatSeriesCalls;
            editedRepeatSeries = input;
            if (failNextRepeatSeriesEdit)
            {
                failNextRepeatSeriesEdit = false;
                return Application::CalendarEventSeriesEditResult::failure({
                    .code = Domain::ErrorCode::Technical,
                    .message = "The test repeat-series edit failed.",
                    .recoverable = true
                });
            }
            return Application::CalendarEventSeriesEditResult::success();
        };
        actions.deleteRepeatSeries = [this](
            const QString&,
            const Application::CalendarEventSeriesDeleteInput& input)
        {
            ++deleteRepeatSeriesCalls;
            deletedRepeatSeries = input;
            if (failNextRepeatSeriesDelete)
            {
                failNextRepeatSeriesDelete = false;
                return Application::CalendarEventSeriesDeleteResult::failure({
                    .code = Domain::ErrorCode::Technical,
                    .message = "The test repeat-series delete failed.",
                    .recoverable = true
                });
            }
            return Application::CalendarEventSeriesDeleteResult::success();
        };
        actions.deleteAllEvents = [this](const QString&)
        {
            ++deleteAllEventsCalls;
            return Application::CalendarEventDeleteAllResult::success();
        };
        actions.readDisplayPreferences = [](
            const QString&)
        {
            return Application::CalendarEventDisplayPreferencesResult::success({});
        };
        actions.saveDisplayPreferences = [this](
            const QString&,
            const Application::CalendarEventDisplayPreferencesSaveRequest& request)
        {
            ++saveDisplayPreferencesCalls;
            savedDisplayPreferences = request;
            return Application::CalendarEventDisplayPreferencesSaveResult::success();
        };
        actions.readCurrentCampus = [](const QString&)
        {
            return Domain::Result<Application::CurrentCampusPreferenceValue>::success({});
        };
        actions.readCampusDirectory = []
        {
            return std::vector<Application::CalendarPageCampusMetadata>{};
        };
        actions.readEventTypeColor = [this](
            const QString&,
            const Domain::CalendarEventType eventType)
        {
            if (eventType == Domain::CalendarEventType::Holiday
                && !holidayColorOverride.empty())
            {
                return Domain::Result<std::string>::success(
                    holidayColorOverride);
            }
            return Domain::Result<std::string>::success({});
        };
        actions.saveEventTypeColor = [this](
            const QString&,
            const Domain::CalendarEventType,
            const std::string&)
        {
            ++saveColorCalls;
            return Application::CalendarEventTypeColorPreferencesPort::SaveResult::success();
        };

        if (!host->registerPage({
                .id = *Ui::PageId::fromString(QStringLiteral("calendar")),
                .label = QStringLiteral("Calendar"),
                .factory = [this, actions = std::move(actions)](
                               const Ui::PageContext& context) mutable
                    -> Ui::PageOperationResult<std::unique_ptr<Ui::Page>>
                {
                    auto instance = std::make_unique<Ui::CalendarPage>(
                        context, std::move(actions));
                    page = instance.get();
                    lazyBeforeActivation = !instance->calendarView()
                        && !instance->eventCache()
                        && !instance->eventModel();
                    return std::unique_ptr<Ui::Page>(std::move(instance));
                }
            })
            || !host->registerPage({
                .id = *Ui::PageId::fromString(QStringLiteral("other")),
                .label = QStringLiteral("Other"),
                .factory = [](const Ui::PageContext& context)
                    -> Ui::PageOperationResult<std::unique_ptr<Ui::Page>>
                {
                    return std::unique_ptr<Ui::Page>(
                        std::make_unique<OtherPage>(context));
                }
            }))
        {
            initializationError = QStringLiteral("page registration failed");
            return false;
        }
        host->resize(1280, 920);
        host->show();
        if (!host->activatePage(QStringLiteral("calendar")) || !page)
        {
            initializationError = QStringLiteral("calendar page activation failed");
            return false;
        }
        QApplication::processEvents();
        return true;
    }

    [[nodiscard]] bool loadKoreanTranslation()
    {
        koreanTranslator = std::make_unique<QTranslator>();
        const QString translationPath = QFileInfo(
            QStringLiteral(CLASSMNGR_NEXT_TEST_RESOURCE_CATALOG))
            .dir()
            .filePath(QStringLiteral("assets/translations/ClassMngr_ko_KR.qm"));
        if (!koreanTranslator->load(translationPath))
        {
            initializationError = QStringLiteral("compiled Korean translation could not be loaded: %1")
                .arg(translationPath);
            return false;
        }
        if (!QCoreApplication::installTranslator(koreanTranslator.get()))
        {
            initializationError = QStringLiteral("compiled Korean translation could not be installed");
            return false;
        }
        return true;
    }
};

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

} // namespace

class CalendarPageTests final : public QObject
{
    Q_OBJECT

private slots:
    void calendarContentIsLazyBoundedAndReleased();
    void eventTypeFilterAffectsUpcomingButNotMonthGrid();
    void cancellingAnEventDialogWritesNothing();
    void savingEventPreservesCivilIsoDates();
    void failedEventMutationKeepsTheDraftRecoverable();
    void repeatCreationUsesTheTypedSeriesContract();
    void existingRepeatSeriesEditUsesSelectedScope_data();
    void existingRepeatSeriesEditUsesSelectedScope();
    void existingRepeatSeriesDeleteUsesSelectedScope_data();
    void existingRepeatSeriesDeleteUsesSelectedScope();
    void cancellingRepeatSeriesDeleteWritesNothing();
    void failedRepeatSeriesEditKeepsDraftAndScopeForRetry();
    void failedRepeatSeriesDeleteKeepsDraftAndScopeForRetry();
    void cancelledColorPickerAndPreferencesWriteNothing();
    void savingPreferencesUsesTypedActions();
    void calendarResetRequiresConfirmation();
    void campusDirectoryReadsTypedNextCatalogResources();
    void captureEnglishAndKoreanLightAndDark();
};

void CalendarPageTests::calendarContentIsLazyBoundedAndReleased()
{
    CalendarPageFixture fixture;
    QVERIFY(fixture.initialize(false));
    QVERIFY(fixture.lazyBeforeActivation);
    QVERIFY(fixture.page->calendarView());
    QCOMPARE(fixture.page->runtimeMetrics().calendarRootObjectCount, 1);
    const auto initial = fixture.page->runtimeMetrics();
    QVERIFY(initial.visibleGridDayCount >= 28);
    QVERIFY(initial.visibleGridDayCount <= 42);
    QVERIFY(initial.upcomingResultCount <= initial.upcomingResultLimit);

    auto* scope = fixture.page->findChild<QComboBox*>(
        QStringLiteral("calendarUpcomingScope"));
    QVERIFY(scope);
    scope->setCurrentIndex(2);
    QApplication::processEvents();
    QCOMPARE(fixture.page->runtimeMetrics().upcomingRangeDayCount, 366);
    QVERIFY(fixture.page->runtimeMetrics().upcomingResultCount <= 10);

    const int constructions =
        fixture.page->runtimeMetrics().calendarViewConstructionCount;
    QVERIFY(fixture.host->activatePage(QStringLiteral("other")));
    QApplication::processEvents();
    QCOMPARE(fixture.page->runtimeMetrics().calendarViewCount, 0);
    QCOMPARE(fixture.page->runtimeMetrics().calendarRootObjectCount, 0);
    QVERIFY(!fixture.page->eventModel());
    QVERIFY(!fixture.page->eventCache());

    QVERIFY(fixture.host->activatePage(QStringLiteral("calendar")));
    QApplication::processEvents();
    QCOMPARE(fixture.page->runtimeMetrics().calendarViewConstructionCount,
             constructions + 1);
    QCOMPARE(fixture.page->runtimeMetrics().calendarRootObjectCount, 1);
}

void CalendarPageTests::eventTypeFilterAffectsUpcomingButNotMonthGrid()
{
    CalendarPageFixture fixture;
    fixture.holidayColorOverride = "#1255aa";
    QVERIFY(fixture.initialize(true, false, true));

    QQuickWidget* const calendarView = fixture.page->calendarView();
    QVERIFY(calendarView);
    QVERIFY(calendarView->rootObject());
    QVERIFY(calendarView->rootObject()->setProperty(
        "shownDate", QDateTime(QDate(2026, 10, 1), QTime(0, 0))));
    QTRY_VERIFY_WITH_TIMEOUT(
        fixture.page->eventModel()->isMonthLoaded(2026, 10), 5000);
    QTRY_VERIFY_WITH_TIMEOUT(
        !fixture.page->runtimeMetrics().cacheLoading, 5000);

    auto* holidayFilter = fixture.page->findChild<QPushButton*>(
        QStringLiteral("calendarEventTypeFilter1"));
    QListWidget* const upcoming = fixture.page->upcomingList();
    QVERIFY(holidayFilter);
    QVERIFY(upcoming);
    QVERIFY(holidayFilter->geometry().top() > calendarView->geometry().bottom());
    QVERIFY(holidayFilter->geometry().bottom() < upcoming->geometry().top());
    QVERIFY(holidayFilter->styleSheet().contains(
        QStringLiteral("#1255aa"), Qt::CaseInsensitive));

    const auto gridContainsHoliday = [&fixture]
    {
        const QVariantList events = fixture.page->eventModel()->eventsForDate(
            2026, 10, 26);
        for (const QVariant& value : events)
        {
            const QVariantMap event = value.toMap();
            if (event.value(QStringLiteral("title")).toString()
                    == QStringLiteral("School holiday")
                && event.value(QStringLiteral("eventType")).toString()
                    == QStringLiteral("Holiday")
                && event.value(QStringLiteral("allDay")).toBool())
            {
                return true;
            }
        }
        return false;
    };
    const auto upcomingHoliday = [upcoming]
    {
        for (int row = 0; row < upcoming->count(); ++row)
        {
            QListWidgetItem* const item = upcoming->item(row);
            if (item->text().contains(QStringLiteral("School holiday")))
            {
                return item;
            }
        }
        return static_cast<QListWidgetItem*>(nullptr);
    };

    QVERIFY(gridContainsHoliday());
    QVERIFY(upcomingHoliday());
    QCOMPARE(upcomingHoliday()->icon().pixmap(QSize(12, 12))
                 .toImage().pixelColor(6, 6),
             QColor(QStringLiteral("#1255aa")));

    holidayFilter->click();
    QVERIFY(!holidayFilter->isChecked());
    QTRY_VERIFY_WITH_TIMEOUT(!upcomingHoliday(), 5000);
    QVERIFY(gridContainsHoliday());

    holidayFilter->click();
    QVERIFY(holidayFilter->isChecked());
    QTRY_VERIFY_WITH_TIMEOUT(upcomingHoliday(), 5000);
    QVERIFY(gridContainsHoliday());
}

void CalendarPageTests::cancellingAnEventDialogWritesNothing()
{
    CalendarPageFixture fixture;
    QVERIFY(fixture.initialize(true));
    QTimer::singleShot(0, []
    {
        auto* dialog = qobject_cast<Ui::CalendarEventDialog*>(
            QApplication::activeModalWidget());
        QVERIFY(dialog);
        auto* buttons = dialog->findChild<QDialogButtonBox*>(
            QStringLiteral("calendarEventButtons"));
        QVERIFY(buttons);
        buttons->button(QDialogButtonBox::Cancel)->click();
    });
    QVERIFY(QMetaObject::invokeMethod(
        fixture.page,
        "handleCalendarDayActivated",
        Qt::DirectConnection,
        Q_ARG(int, 2026),
        Q_ARG(int, 7),
        Q_ARG(int, 10)));
    QCOMPARE(fixture.saveEventCalls, 0);
    QCOMPARE(fixture.createSeriesCalls, 0);
}

void CalendarPageTests::savingEventPreservesCivilIsoDates()
{
    CalendarPageFixture fixture;
    QVERIFY(fixture.initialize(true));
    QTimer::singleShot(0, []
    {
        auto* dialog = qobject_cast<Ui::CalendarEventDialog*>(
            QApplication::activeModalWidget());
        QVERIFY(dialog);
        auto* title = dialog->findChild<QLineEdit*>(
            QStringLiteral("calendarEventTitle"));
        auto* start = dialog->findChild<QDateEdit*>(
            QStringLiteral("calendarEventStartDate"));
        auto* end = dialog->findChild<QDateEdit*>(
            QStringLiteral("calendarEventEndDate"));
        auto* buttons = dialog->findChild<QDialogButtonBox*>(
            QStringLiteral("calendarEventButtons"));
        QVERIFY(title);
        QVERIFY(start);
        QVERIFY(end);
        QVERIFY(buttons);
        title->setText(QStringLiteral("Civil date event"));
        start->setDate(QDate(2026, 10, 31));
        end->setDate(QDate(2026, 11, 1));
        buttons->button(QDialogButtonBox::Save)->click();
    });
    QVERIFY(QMetaObject::invokeMethod(
        fixture.page,
        "handleCalendarDayActivated",
        Qt::DirectConnection,
        Q_ARG(int, 2026),
        Q_ARG(int, 10),
        Q_ARG(int, 31)));

    QCOMPARE(fixture.saveEventCalls, 1);
    QVERIFY(fixture.savedEvent.has_value());
    QCOMPARE(fixture.savedEvent->startDate, std::string("2026-10-31"));
    QCOMPARE(fixture.savedEvent->endDate, std::string("2026-11-01"));
    QCOMPARE(fixture.savedEvent->title, std::string("Civil date event"));
}

void CalendarPageTests::failedEventMutationKeepsTheDraftRecoverable()
{
    CalendarPageFixture fixture;
    QVERIFY(fixture.initialize(true));
    fixture.failNextEventSave = true;

    int step = 0;
    auto* driver = new QTimer(fixture.page);
    connect(driver, &QTimer::timeout, fixture.page, [&]
    {
        QWidget* const modal = QApplication::activeModalWidget();
        if (step == 0)
        {
            auto* dialog = qobject_cast<Ui::CalendarEventDialog*>(modal);
            if (!dialog)
            {
                return;
            }
            auto* title = dialog->findChild<QLineEdit*>(
                QStringLiteral("calendarEventTitle"));
            auto* buttons = dialog->findChild<QDialogButtonBox*>(
                QStringLiteral("calendarEventButtons"));
            if (!title || !buttons)
            {
                return;
            }
            title->setText(QStringLiteral("Recoverable draft"));
            step = 1;
            buttons->button(QDialogButtonBox::Save)->click();
            return;
        }
        if (step == 1)
        {
            auto* message = qobject_cast<QMessageBox*>(modal);
            if (!message)
            {
                return;
            }
            step = 2;
            message->button(QMessageBox::Ok)->click();
            return;
        }
        auto* dialog = qobject_cast<Ui::CalendarEventDialog*>(modal);
        if (!dialog)
        {
            return;
        }
        auto* title = dialog->findChild<QLineEdit*>(
            QStringLiteral("calendarEventTitle"));
        auto* buttons = dialog->findChild<QDialogButtonBox*>(
            QStringLiteral("calendarEventButtons"));
        if (!title || !buttons)
        {
            return;
        }
        QCOMPARE(title->text(), QStringLiteral("Recoverable draft"));
        step = 3;
        driver->stop();
        buttons->button(QDialogButtonBox::Cancel)->click();
    });
    driver->start(0);

    QVERIFY(QMetaObject::invokeMethod(
        fixture.page,
        "handleCalendarDayActivated",
        Qt::DirectConnection,
        Q_ARG(int, 2026),
        Q_ARG(int, 7),
        Q_ARG(int, 10)));
    QCOMPARE(step, 3);
    QCOMPARE(fixture.saveEventCalls, 1);
}

void CalendarPageTests::repeatCreationUsesTheTypedSeriesContract()
{
    CalendarPageFixture fixture;
    QVERIFY(fixture.initialize(true));
    QTimer::singleShot(0, []
    {
        auto* dialog = qobject_cast<Ui::CalendarEventDialog*>(
            QApplication::activeModalWidget());
        QVERIFY(dialog);
        auto* title = dialog->findChild<QLineEdit*>(
            QStringLiteral("calendarEventTitle"));
        auto* repeat = dialog->findChild<QCheckBox*>(
            QStringLiteral("calendarEventRepeat"));
        auto* until = dialog->findChild<QDateEdit*>(
            QStringLiteral("calendarEventRepeatUntil"));
        auto* buttons = dialog->findChild<QDialogButtonBox*>(
            QStringLiteral("calendarEventButtons"));
        QVERIFY(title);
        QVERIFY(repeat);
        QVERIFY(until);
        QVERIFY(buttons);
        title->setText(QStringLiteral("Repeating meeting"));
        repeat->setChecked(true);
        until->setDate(QDate(2026, 7, 24));
        buttons->button(QDialogButtonBox::Save)->click();
    });
    QVERIFY(QMetaObject::invokeMethod(
        fixture.page,
        "handleCalendarDayActivated",
        Qt::DirectConnection,
        Q_ARG(int, 2026),
        Q_ARG(int, 7),
        Q_ARG(int, 10)));

    QCOMPARE(fixture.createSeriesCalls, 1);
    QVERIFY(fixture.createdSeries.has_value());
    QVERIFY(fixture.createdSeries->repeatSeriesId.has_value());
    QCOMPARE(fixture.createdSeries->frequency,
             Application::CalendarEventRepeatFrequency::Weekly);
    QCOMPARE(fixture.createdSeries->untilDate, std::string("2026-07-24"));
    QCOMPARE(fixture.createdSeries->seed.startDate, std::string("2026-07-10"));
    QCOMPARE(fixture.createdSeries->seed.title, std::string("Repeating meeting"));
    QCOMPARE(fixture.saveEventCalls, 0);
}

void CalendarPageTests::existingRepeatSeriesEditUsesSelectedScope_data()
{
    QTest::addColumn<int>("scope");
    QTest::addColumn<bool>("editsSeriesSuffix");
    QTest::newRow("single-occurrence")
        << static_cast<int>(Ui::CalendarEventSeriesEditScope::ThisEventOnly)
        << false;
    QTest::newRow("this-and-following")
        << static_cast<int>(Ui::CalendarEventSeriesEditScope::ThisAndFollowingEvents)
        << true;
}

void CalendarPageTests::existingRepeatSeriesEditUsesSelectedScope()
{
    QFETCH(int, scope);
    QFETCH(bool, editsSeriesSuffix);

    CalendarPageFixture fixture;
    fixture.readRepeatEvent = true;
    QVERIFY(fixture.initialize(true));
    QTimer::singleShot(0, [&fixture, scope]
    {
        auto* dialog = qobject_cast<Ui::CalendarEventDialog*>(
            QApplication::activeModalWidget());
        QVERIFY(dialog);
        auto* title = dialog->findChild<QLineEdit*>(
            QStringLiteral("calendarEventTitle"));
        auto* startDate = dialog->findChild<QDateEdit*>(
            QStringLiteral("calendarEventStartDate"));
        auto* seriesScope = dialog->findChild<QComboBox*>(
            QStringLiteral("calendarEventSeriesScope"));
        auto* buttons = dialog->findChild<QDialogButtonBox*>(
            QStringLiteral("calendarEventButtons"));
        QVERIFY(title);
        QVERIFY(startDate);
        QVERIFY(seriesScope);
        QVERIFY(buttons);
        QVERIFY(seriesScope->findData(scope) >= 0);
        title->setText(QStringLiteral("Edited repeat occurrence"));
        startDate->setDate(QDate(2026, 7, 17));
        seriesScope->setCurrentIndex(seriesScope->findData(scope));
        buttons->button(QDialogButtonBox::Save)->click();
    });

    QVERIFY(QMetaObject::invokeMethod(
        fixture.page,
        "handleCalendarEventActivated",
        Qt::DirectConnection,
        Q_ARG(QString, QStringLiteral("123"))));

    if (editsSeriesSuffix)
    {
        QCOMPARE(fixture.editRepeatSeriesCalls, 1);
        QVERIFY(fixture.editedRepeatSeries.has_value());
        QCOMPARE(fixture.editedRepeatSeries->repeatSeriesId->value(),
                 std::string("series-42"));
        QCOMPARE(fixture.editedRepeatSeries->startDate, std::string("2026-07-10"));
        QCOMPARE(fixture.editedRepeatSeries->editedStartDate,
                 std::string("2026-07-17"));
        QCOMPARE(fixture.editedRepeatSeries->title,
                 std::string("Edited repeat occurrence"));
        QCOMPARE(fixture.saveEventCalls, 0);
    }
    else
    {
        QCOMPARE(fixture.saveEventCalls, 1);
        QVERIFY(fixture.savedEvent.has_value());
        QVERIFY(fixture.savedEvent->id.has_value());
        QCOMPARE(fixture.savedEvent->id->value(), std::string("123"));
        QCOMPARE(fixture.savedEvent->startDate, std::string("2026-07-17"));
        QCOMPARE(fixture.savedEvent->title,
                 std::string("Edited repeat occurrence"));
        QCOMPARE(fixture.editRepeatSeriesCalls, 0);
    }
}

void CalendarPageTests::existingRepeatSeriesDeleteUsesSelectedScope_data()
{
    QTest::addColumn<int>("scope");
    QTest::addColumn<bool>("deletesSeriesSuffix");
    QTest::newRow("single-occurrence")
        << static_cast<int>(Ui::CalendarEventSeriesEditScope::ThisEventOnly)
        << false;
    QTest::newRow("this-and-following")
        << static_cast<int>(Ui::CalendarEventSeriesEditScope::ThisAndFollowingEvents)
        << true;
}

void CalendarPageTests::existingRepeatSeriesDeleteUsesSelectedScope()
{
    QFETCH(int, scope);
    QFETCH(bool, deletesSeriesSuffix);

    CalendarPageFixture fixture;
    fixture.readRepeatEvent = true;
    QVERIFY(fixture.initialize(true));
    QTimer::singleShot(0, [&fixture, scope]
    {
        auto* dialog = qobject_cast<Ui::CalendarEventDialog*>(
            QApplication::activeModalWidget());
        QVERIFY(dialog);
        auto* seriesScope = dialog->findChild<QComboBox*>(
            QStringLiteral("calendarEventSeriesScope"));
        auto* deleteButton = dialog->findChild<QPushButton*>(
            QStringLiteral("calendarEventDelete"));
        QVERIFY(seriesScope);
        QVERIFY(deleteButton);
        QVERIFY(seriesScope->findData(scope) >= 0);
        seriesScope->setCurrentIndex(seriesScope->findData(scope));
        QTimer::singleShot(0, []
        {
            auto* confirmation = qobject_cast<QMessageBox*>(
                QApplication::activeModalWidget());
            QVERIFY(confirmation);
            QVERIFY(confirmation->button(QMessageBox::Yes));
            confirmation->button(QMessageBox::Yes)->click();
        });
        deleteButton->click();
    });

    QVERIFY(QMetaObject::invokeMethod(
        fixture.page,
        "handleCalendarEventActivated",
        Qt::DirectConnection,
        Q_ARG(QString, QStringLiteral("123"))));

    if (deletesSeriesSuffix)
    {
        QCOMPARE(fixture.deleteRepeatSeriesCalls, 1);
        QVERIFY(fixture.deletedRepeatSeries.has_value());
        QCOMPARE(fixture.deletedRepeatSeries->repeatSeriesId->value(),
                 std::string("series-42"));
        QCOMPARE(fixture.deletedRepeatSeries->startDate,
                 std::string("2026-07-10"));
        QCOMPARE(fixture.deleteEventCalls, 0);
    }
    else
    {
        QCOMPARE(fixture.deleteEventCalls, 1);
        QVERIFY(fixture.deletedEventId.has_value());
        QCOMPARE(fixture.deletedEventId->value(), std::string("123"));
        QCOMPARE(fixture.deleteRepeatSeriesCalls, 0);
    }
}

void CalendarPageTests::cancellingRepeatSeriesDeleteWritesNothing()
{
    CalendarPageFixture fixture;
    fixture.readRepeatEvent = true;
    QVERIFY(fixture.initialize(true));

    int step = 0;
    bool draftPreserved = false;
    bool scopePreserved = false;
    auto* driver = new QTimer(fixture.page);
    connect(driver, &QTimer::timeout, fixture.page, [&]
    {
        QWidget* const modal = QApplication::activeModalWidget();
        if (step == 0)
        {
            auto* dialog = qobject_cast<Ui::CalendarEventDialog*>(modal);
            if (!dialog)
            {
                return;
            }
            auto* title = dialog->findChild<QLineEdit*>(
                QStringLiteral("calendarEventTitle"));
            auto* seriesScope = dialog->findChild<QComboBox*>(
                QStringLiteral("calendarEventSeriesScope"));
            auto* deleteButton = dialog->findChild<QPushButton*>(
                QStringLiteral("calendarEventDelete"));
            if (!title || !seriesScope || !deleteButton)
            {
                return;
            }
            title->setText(QStringLiteral("Keep this series"));
            seriesScope->setCurrentIndex(seriesScope->findData(
                static_cast<int>(Ui::CalendarEventSeriesEditScope::ThisAndFollowingEvents)));
            step = 1;
            deleteButton->click();
            return;
        }
        if (step == 1)
        {
            auto* confirmation = qobject_cast<QMessageBox*>(modal);
            if (!confirmation)
            {
                return;
            }
            step = 2;
            confirmation->button(QMessageBox::No)->click();
            return;
        }
        auto* dialog = qobject_cast<Ui::CalendarEventDialog*>(modal);
        if (!dialog)
        {
            return;
        }
        auto* title = dialog->findChild<QLineEdit*>(
            QStringLiteral("calendarEventTitle"));
        auto* seriesScope = dialog->findChild<QComboBox*>(
            QStringLiteral("calendarEventSeriesScope"));
        auto* buttons = dialog->findChild<QDialogButtonBox*>(
            QStringLiteral("calendarEventButtons"));
        if (!title || !seriesScope || !buttons)
        {
            return;
        }
        draftPreserved = title->text() == QStringLiteral("Keep this series");
        scopePreserved = seriesScope->currentData().toInt()
            == static_cast<int>(Ui::CalendarEventSeriesEditScope::ThisAndFollowingEvents);
        step = 3;
        driver->stop();
        buttons->button(QDialogButtonBox::Cancel)->click();
    });
    driver->start(0);

    QVERIFY(QMetaObject::invokeMethod(
        fixture.page,
        "handleCalendarEventActivated",
        Qt::DirectConnection,
        Q_ARG(QString, QStringLiteral("123"))));

    QCOMPARE(step, 3);
    QVERIFY(draftPreserved);
    QVERIFY(scopePreserved);
    QCOMPARE(fixture.saveEventCalls, 0);
    QCOMPARE(fixture.editRepeatSeriesCalls, 0);
    QCOMPARE(fixture.deleteEventCalls, 0);
    QCOMPARE(fixture.deleteRepeatSeriesCalls, 0);
}

void CalendarPageTests::failedRepeatSeriesEditKeepsDraftAndScopeForRetry()
{
    CalendarPageFixture fixture;
    fixture.readRepeatEvent = true;
    fixture.failNextRepeatSeriesEdit = true;
    QVERIFY(fixture.initialize(true));

    int step = 0;
    bool draftPreserved = false;
    bool scopePreserved = false;
    auto* driver = new QTimer(fixture.page);
    connect(driver, &QTimer::timeout, fixture.page, [&]
    {
        QWidget* const modal = QApplication::activeModalWidget();
        if (step == 0)
        {
            auto* dialog = qobject_cast<Ui::CalendarEventDialog*>(modal);
            if (!dialog)
            {
                return;
            }
            auto* title = dialog->findChild<QLineEdit*>(
                QStringLiteral("calendarEventTitle"));
            auto* seriesScope = dialog->findChild<QComboBox*>(
                QStringLiteral("calendarEventSeriesScope"));
            auto* buttons = dialog->findChild<QDialogButtonBox*>(
                QStringLiteral("calendarEventButtons"));
            if (!title || !seriesScope || !buttons)
            {
                return;
            }
            title->setText(QStringLiteral("Retry this series edit"));
            seriesScope->setCurrentIndex(seriesScope->findData(
                static_cast<int>(Ui::CalendarEventSeriesEditScope::ThisAndFollowingEvents)));
            step = 1;
            buttons->button(QDialogButtonBox::Save)->click();
            return;
        }
        if (step == 1)
        {
            auto* warning = qobject_cast<QMessageBox*>(modal);
            if (!warning)
            {
                return;
            }
            step = 2;
            warning->button(QMessageBox::Ok)->click();
            return;
        }
        auto* dialog = qobject_cast<Ui::CalendarEventDialog*>(modal);
        if (!dialog)
        {
            return;
        }
        auto* title = dialog->findChild<QLineEdit*>(
            QStringLiteral("calendarEventTitle"));
        auto* seriesScope = dialog->findChild<QComboBox*>(
            QStringLiteral("calendarEventSeriesScope"));
        auto* buttons = dialog->findChild<QDialogButtonBox*>(
            QStringLiteral("calendarEventButtons"));
        if (!title || !seriesScope || !buttons)
        {
            return;
        }
        draftPreserved = title->text() == QStringLiteral("Retry this series edit");
        scopePreserved = seriesScope->currentData().toInt()
            == static_cast<int>(Ui::CalendarEventSeriesEditScope::ThisAndFollowingEvents);
        step = 3;
        driver->stop();
        buttons->button(QDialogButtonBox::Save)->click();
    });
    driver->start(0);

    QVERIFY(QMetaObject::invokeMethod(
        fixture.page,
        "handleCalendarEventActivated",
        Qt::DirectConnection,
        Q_ARG(QString, QStringLiteral("123"))));

    QCOMPARE(step, 3);
    QVERIFY(draftPreserved);
    QVERIFY(scopePreserved);
    QCOMPARE(fixture.editRepeatSeriesCalls, 2);
    QVERIFY(fixture.editedRepeatSeries.has_value());
    QCOMPARE(fixture.editedRepeatSeries->title,
             std::string("Retry this series edit"));
    QCOMPARE(fixture.saveEventCalls, 0);
}

void CalendarPageTests::failedRepeatSeriesDeleteKeepsDraftAndScopeForRetry()
{
    CalendarPageFixture fixture;
    fixture.readRepeatEvent = true;
    fixture.failNextRepeatSeriesDelete = true;
    QVERIFY(fixture.initialize(true));

    int step = 0;
    bool draftPreserved = false;
    bool scopePreserved = false;
    auto* driver = new QTimer(fixture.page);
    connect(driver, &QTimer::timeout, fixture.page, [&]
    {
        QWidget* const modal = QApplication::activeModalWidget();
        if (step == 0)
        {
            auto* dialog = qobject_cast<Ui::CalendarEventDialog*>(modal);
            if (!dialog)
            {
                return;
            }
            auto* title = dialog->findChild<QLineEdit*>(
                QStringLiteral("calendarEventTitle"));
            auto* seriesScope = dialog->findChild<QComboBox*>(
                QStringLiteral("calendarEventSeriesScope"));
            auto* deleteButton = dialog->findChild<QPushButton*>(
                QStringLiteral("calendarEventDelete"));
            if (!title || !seriesScope || !deleteButton)
            {
                return;
            }
            title->setText(QStringLiteral("Retry this series delete"));
            seriesScope->setCurrentIndex(seriesScope->findData(
                static_cast<int>(Ui::CalendarEventSeriesEditScope::ThisAndFollowingEvents)));
            step = 1;
            deleteButton->click();
            return;
        }
        if (step == 1)
        {
            auto* confirmation = qobject_cast<QMessageBox*>(modal);
            if (!confirmation)
            {
                return;
            }
            step = 2;
            confirmation->button(QMessageBox::Yes)->click();
            return;
        }
        if (step == 2)
        {
            auto* warning = qobject_cast<QMessageBox*>(modal);
            if (!warning)
            {
                return;
            }
            step = 3;
            warning->button(QMessageBox::Ok)->click();
            return;
        }
        if (step == 3)
        {
            auto* dialog = qobject_cast<Ui::CalendarEventDialog*>(modal);
            if (!dialog)
            {
                return;
            }
            auto* title = dialog->findChild<QLineEdit*>(
                QStringLiteral("calendarEventTitle"));
            auto* seriesScope = dialog->findChild<QComboBox*>(
                QStringLiteral("calendarEventSeriesScope"));
            auto* deleteButton = dialog->findChild<QPushButton*>(
                QStringLiteral("calendarEventDelete"));
            if (!title || !seriesScope || !deleteButton)
            {
                return;
            }
            draftPreserved = title->text()
                == QStringLiteral("Retry this series delete");
            scopePreserved = seriesScope->currentData().toInt()
                == static_cast<int>(Ui::CalendarEventSeriesEditScope::ThisAndFollowingEvents);
            step = 4;
            deleteButton->click();
            return;
        }
        auto* confirmation = qobject_cast<QMessageBox*>(modal);
        if (!confirmation)
        {
            return;
        }
        step = 5;
        driver->stop();
        confirmation->button(QMessageBox::Yes)->click();
    });
    driver->start(0);

    QVERIFY(QMetaObject::invokeMethod(
        fixture.page,
        "handleCalendarEventActivated",
        Qt::DirectConnection,
        Q_ARG(QString, QStringLiteral("123"))));

    QCOMPARE(step, 5);
    QVERIFY(draftPreserved);
    QVERIFY(scopePreserved);
    QCOMPARE(fixture.deleteRepeatSeriesCalls, 2);
    QCOMPARE(fixture.deleteEventCalls, 0);
    QVERIFY(fixture.deletedRepeatSeries.has_value());
    QVERIFY(fixture.deletedRepeatSeries->repeatSeriesId.has_value());
    QCOMPARE(fixture.deletedRepeatSeries->repeatSeriesId->value(),
             std::string("series-42"));
    QCOMPARE(fixture.deletedRepeatSeries->startDate, std::string("2026-07-10"));
}

void CalendarPageTests::cancelledColorPickerAndPreferencesWriteNothing()
{
    CalendarPageFixture fixture;
    QVERIFY(fixture.initialize(true));
    QTimer::singleShot(0, [&fixture]
    {
        auto* preferences = qobject_cast<Ui::CalendarPreferencesDialog*>(
            QApplication::activeModalWidget());
        QVERIFY(preferences);
        QTimer::singleShot(0, []
        {
            auto* colorDialog = qobject_cast<QColorDialog*>(
                QApplication::activeModalWidget());
            QVERIFY(colorDialog);
            colorDialog->reject();
        });
        auto* color = preferences->findChild<QPushButton*>(
            QStringLiteral("calendarEventColor0"));
        QVERIFY(color);
        color->click();

        QTimer::singleShot(0, []
        {
            auto* preferences = qobject_cast<Ui::CalendarPreferencesDialog*>(
                QApplication::activeModalWidget());
            QVERIFY(preferences);
            auto* buttons = preferences->findChild<QDialogButtonBox*>(
                QStringLiteral("calendarPreferencesButtons"));
            QVERIFY(buttons);
            buttons->button(QDialogButtonBox::Cancel)->click();
        });
    });
    auto* preferencesButton = fixture.page->findChild<QPushButton*>(
        QStringLiteral("calendarPreferences"));
    QVERIFY(preferencesButton);
    preferencesButton->click();
    QCOMPARE(fixture.saveDisplayPreferencesCalls, 0);
    QCOMPARE(fixture.saveColorCalls, 0);
}

void CalendarPageTests::savingPreferencesUsesTypedActions()
{
    CalendarPageFixture fixture;
    QVERIFY(fixture.initialize(true));
    int savedFirstDay = -1;
    int savedWeekCount = 0;
    int savedTermYear = 0;
    QTimer::singleShot(0, [&fixture, &savedFirstDay, &savedWeekCount, &savedTermYear]
    {
        auto* dialog = qobject_cast<Ui::CalendarPreferencesDialog*>(
            QApplication::activeModalWidget());
        QVERIFY(dialog);
        auto* allCampuses = dialog->findChild<QCheckBox*>(
            QStringLiteral("calendarShowAllCampuses"));
        auto* hideStart = dialog->findChild<QCheckBox*>(
            QStringLiteral("calendarHideStartOfTerm"));
        auto* firstDay = dialog->findChild<QComboBox*>(
            QStringLiteral("calendarFirstDayOfWeek"));
        auto* termYear = dialog->findChild<QSpinBox*>(
            QStringLiteral("calendarTermYear"));
        auto* weeks = dialog->findChild<QSpinBox*>(
            QStringLiteral("calendarTermWeeks0_0"));
        auto* color = dialog->findChild<QPushButton*>(
            QStringLiteral("calendarEventColor0"));
        auto* buttons = dialog->findChild<QDialogButtonBox*>(
            QStringLiteral("calendarPreferencesButtons"));
        QVERIFY(allCampuses);
        QVERIFY(hideStart);
        QVERIFY(firstDay);
        QVERIFY(termYear);
        QVERIFY(weeks);
        QVERIFY(color);
        QVERIFY(buttons);
        allCampuses->setChecked(true);
        hideStart->setChecked(true);
        firstDay->setCurrentIndex(firstDay->findData(1));
        weeks->setValue(weeks->value() == 53 ? 52 : weeks->value() + 1);
        savedFirstDay = firstDay->currentData().toInt();
        savedWeekCount = weeks->value();
        savedTermYear = termYear->value();
        QTimer::singleShot(0, []
        {
            auto* colorDialog = qobject_cast<QColorDialog*>(
                QApplication::activeModalWidget());
            QVERIFY(colorDialog);
            colorDialog->setCurrentColor(QColor(QStringLiteral("#336699")));
            colorDialog->accept();
        });
        color->click();
        buttons->button(QDialogButtonBox::Save)->click();
    });
    auto* preferencesButton = fixture.page->findChild<QPushButton*>(
        QStringLiteral("calendarPreferences"));
    QVERIFY(preferencesButton);
    preferencesButton->click();

    QCOMPARE(fixture.saveDisplayPreferencesCalls, 1);
    QVERIFY(fixture.savedDisplayPreferences.has_value());
    QVERIFY(fixture.savedDisplayPreferences->showEventsAtAllCampuses);
    QVERIFY(fixture.savedDisplayPreferences->hideStartOfTermEvents);
    QCOMPARE(fixture.saveColorCalls, 1);

    QVERIFY(fixture.host->activatePage(QStringLiteral("other")));
    WorkspaceStore store;
    QVERIFY(store.openExisting(fixture.path));
    Platform::ApplicationServicesCalendarFirstDayOfWeekPreferencesPort firstDayPort(store);
    const auto storedFirstDay = firstDayPort.load();
    QVERIFY(storedFirstDay);
    QCOMPARE(static_cast<int>(storedFirstDay.value()), savedFirstDay);

    Platform::ApplicationServicesAcademicCalendarSchedulePreferencesPort schedulePort(store);
    const auto storedSchedule = schedulePort.read();
    QVERIFY(storedSchedule);
    const QByteArray scheduleBytes(
        storedSchedule.value().value().data(),
        static_cast<qsizetype>(storedSchedule.value().value().size()));
    const QJsonDocument scheduleDocument = QJsonDocument::fromJson(scheduleBytes);
    QVERIFY(scheduleDocument.isObject());
    AcademicCalendarSchedule schedule;
    QVERIFY(schedule.fromJson(scheduleDocument.object()));
    QCOMPARE(schedule.yearSchedule(SchoolLevel::Elementary, savedTermYear)
                 .weeks[0],
             savedWeekCount);
}

void CalendarPageTests::calendarResetRequiresConfirmation()
{
    CalendarPageFixture cancelledFixture;
    QVERIFY(cancelledFixture.initialize(true));
    QTimer::singleShot(0, [&cancelledFixture]
    {
        auto* dialog = qobject_cast<Ui::CalendarPreferencesDialog*>(
            QApplication::activeModalWidget());
        QVERIFY(dialog);
        auto* reset = dialog->findChild<QPushButton*>(
            QStringLiteral("calendarResetEvents"));
        auto* buttons = dialog->findChild<QDialogButtonBox*>(
            QStringLiteral("calendarPreferencesButtons"));
        QVERIFY(reset);
        QVERIFY(buttons);
        QTimer::singleShot(0, []
        {
            auto* confirmation = qobject_cast<QMessageBox*>(
                QApplication::activeModalWidget());
            QVERIFY(confirmation);
            confirmation->button(QMessageBox::Cancel)->click();
        });
        reset->click();
        buttons->button(QDialogButtonBox::Cancel)->click();
    });
    auto* cancelledPreferences = cancelledFixture.page->findChild<QPushButton*>(
        QStringLiteral("calendarPreferences"));
    QVERIFY(cancelledPreferences);
    cancelledPreferences->click();
    QCOMPARE(cancelledFixture.deleteAllEventsCalls, 0);

    CalendarPageFixture confirmedFixture;
    QVERIFY(confirmedFixture.initialize(true));
    QTimer::singleShot(0, [&confirmedFixture]
    {
        auto* dialog = qobject_cast<Ui::CalendarPreferencesDialog*>(
            QApplication::activeModalWidget());
        QVERIFY(dialog);
        auto* reset = dialog->findChild<QPushButton*>(
            QStringLiteral("calendarResetEvents"));
        auto* buttons = dialog->findChild<QDialogButtonBox*>(
            QStringLiteral("calendarPreferencesButtons"));
        QVERIFY(reset);
        QVERIFY(buttons);
        QTimer::singleShot(0, []
        {
            auto* confirmation = qobject_cast<QMessageBox*>(
                QApplication::activeModalWidget());
            QVERIFY(confirmation);
            confirmation->button(QMessageBox::Reset)->click();
        });
        reset->click();
        buttons->button(QDialogButtonBox::Cancel)->click();
    });
    auto* confirmedPreferences = confirmedFixture.page->findChild<QPushButton*>(
        QStringLiteral("calendarPreferences"));
    QVERIFY(confirmedPreferences);
    confirmedPreferences->click();
    QCOMPARE(confirmedFixture.deleteAllEventsCalls, 1);
}

void CalendarPageTests::campusDirectoryReadsTypedNextCatalogResources()
{
    CalendarPageFixture fixture;
    QVERIFY(fixture.initialize(false));
    Platform::CalendarPageResourceCampusDirectoryQueryAdapter query(
        fixture.page->resourceContext().resourceLoader(),
        fixture.page->resourceContext().pageScope());
    const auto campuses = query.loadCampuses();
    QVERIFY(campuses.size() >= 10);

    std::string previousName;
    bool hasCampusCode = false;
    for (const auto& campus : campuses)
    {
        QVERIFY(!campus.id.value().empty());
        QVERIFY(!campus.campusName.empty());
        QVERIFY(previousName.empty()
            || QString::compare(
                QString::fromUtf8(previousName.data(),
                    static_cast<qsizetype>(previousName.size())),
                QString::fromUtf8(campus.campusName.data(),
                    static_cast<qsizetype>(campus.campusName.size())),
                Qt::CaseInsensitive) <= 0);
        previousName = campus.campusName;
        hasCampusCode = hasCampusCode || campus.campusCode.has_value();
    }
    QVERIFY(hasCampusCode);
}

void CalendarPageTests::captureEnglishAndKoreanLightAndDark()
{
    const QString outputDirectory = QStringLiteral(CLASSMNGR_CALENDAR_CAPTURE_DIR);
    QVERIFY(QDir().mkpath(outputDirectory));
    QJsonObject captures;
    PlatformProcessMemorySnapshotProvider memoryProvider;
    const QDate visibleMonth(2026, 10, 1);

    for (const bool dark : {false, true})
    {
        for (const bool korean : {false, true})
        {
            QLocale::setDefault(QLocale(korean
                ? QStringLiteral("ko_KR")
                : QStringLiteral("en_US")));
            const ProcessMemorySnapshot before = memoryProvider.snapshot();
            QPalette palette = QApplication::style()->standardPalette();
            if (dark)
            {
                palette.setColor(QPalette::Window, QColor(QStringLiteral("#20242a")));
                palette.setColor(QPalette::WindowText, QColor(QStringLiteral("#f3f4f6")));
                palette.setColor(QPalette::Base, QColor(QStringLiteral("#272c33")));
                palette.setColor(QPalette::AlternateBase, QColor(QStringLiteral("#343b44")));
                palette.setColor(QPalette::Text, QColor(QStringLiteral("#f3f4f6")));
                palette.setColor(QPalette::Button, QColor(QStringLiteral("#343b44")));
                palette.setColor(QPalette::ButtonText, QColor(QStringLiteral("#f3f4f6")));
                palette.setColor(QPalette::Highlight, QColor(QStringLiteral("#6487a5")));
                palette.setColor(QPalette::HighlightedText, Qt::white);
            }
            QApplication::setPalette(palette);

            CalendarPageFixture fixture;
            QVERIFY2(fixture.initialize(true, korean, true),
                qPrintable(fixture.initializationError));
            QVERIFY(fixture.page->calendarView());
            QVERIFY(fixture.page->calendarView()->rootObject());
            QVERIFY(fixture.page->calendarView()->rootObject()->setProperty(
                "shownDate",
                QDateTime(visibleMonth, QTime(0, 0))));
            auto* upcomingScope = fixture.page->findChild<QComboBox*>(
                QStringLiteral("calendarUpcomingScope"));
            QVERIFY(upcomingScope);
            upcomingScope->setCurrentIndex(0);
            QApplication::processEvents();
            QTRY_VERIFY_WITH_TIMEOUT(
                fixture.page->runtimeMetrics().cacheLoadedRangeCount > 0,
                5000);
            QTRY_VERIFY_WITH_TIMEOUT(
                !fixture.page->runtimeMetrics().cacheLoading, 5000);
            auto* pageTitle = fixture.page->findChild<QLabel*>(
                QStringLiteral("calendarPageTitle"));
            QVERIFY(pageTitle);
            QCOMPARE(pageTitle->text(), korean
                ? QStringLiteral("캘린더")
                : QStringLiteral("Calendar"));
            QVERIFY(fixture.page->calendarView());
            const auto activeMetrics = fixture.page->runtimeMetrics();
            QCOMPARE(activeMetrics.calendarRootObjectCount, 1);
            QVERIFY2(activeMetrics.cacheEventCount >= 3,
                qPrintable(QStringLiteral(
                    "Loaded %1 ranges with %2 events; status: %3")
                    .arg(activeMetrics.cacheLoadedRangeCount)
                    .arg(activeMetrics.cacheEventCount)
                    .arg(fixture.page->findChild<QLabel*>(
                        QStringLiteral("calendarPageStatus"))->text())));
            QVERIFY(activeMetrics.upcomingResultCount <= 10);
            const ProcessMemorySnapshot active = memoryProvider.snapshot();
            const int activeWidgetCount = fixture.page->findChildren<QWidget*>().size();

            const QString mode = QStringLiteral("%1-%2")
                .arg(korean ? QStringLiteral("ko") : QStringLiteral("en"),
                     dark ? QStringLiteral("dark") : QStringLiteral("light"));
            const QString imagePath = outputDirectory
                + QLatin1Char('/') + QStringLiteral("calendar-") + mode
                + QStringLiteral(".png");
            const QImage image = fixture.page->grab().toImage();
            QVERIFY(!image.isNull());
            QVERIFY(image.save(imagePath));
            QVERIFY(fixture.host->activatePage(QStringLiteral("other")));
            QApplication::processEvents();
            const ProcessMemorySnapshot released = memoryProvider.snapshot();
            const auto releasedMetrics = fixture.page->runtimeMetrics();
            captures.insert(mode, QJsonObject{
                {QStringLiteral("image"), QFileInfo(imagePath).fileName()},
                {QStringLiteral("visibleMonth"), QStringLiteral("2026-10")},
                {QStringLiteral("firstWeekday"), QStringLiteral("Monday")},
                {QStringLiteral("upcomingScope"), QStringLiteral("This month")},
                {QStringLiteral("activeWidgetCount"), activeWidgetCount},
                {QStringLiteral("visibleGridDays"),
                 activeMetrics.visibleGridDayCount},
                {QStringLiteral("cacheEventCount"),
                 activeMetrics.cacheEventCount},
                {QStringLiteral("cacheDateBucketCount"),
                 activeMetrics.cacheDateBucketCount},
                {QStringLiteral("cacheRetainedRangeCount"),
                 activeMetrics.cacheRetainedRangeCount},
                {QStringLiteral("calendarViewConstructionCount"),
                 activeMetrics.calendarViewConstructionCount},
                {QStringLiteral("upcomingRangeDays"),
                 activeMetrics.upcomingRangeDayCount},
                {QStringLiteral("upcomingResultCount"),
                 activeMetrics.upcomingResultCount},
                {QStringLiteral("upcomingResultLimit"), 10},
                {QStringLiteral("releasedCalendarViews"),
                 releasedMetrics.calendarViewCount},
                {QStringLiteral("releasedCalendarRoots"),
                 releasedMetrics.calendarRootObjectCount},
                {QStringLiteral("memoryProcessWide"), QJsonObject{
                    {QStringLiteral("scope"), QStringLiteral(
                        "Whole-process observations, not page allocation or memory budgets")},
                    {QStringLiteral("before"), memoryJson(before)},
                    {QStringLiteral("active"), memoryJson(active)},
                    {QStringLiteral("released"), memoryJson(released)}
                }}
            });
        }
    }

    QFile manifest(outputDirectory + QStringLiteral("/calendar-captures.json"));
    QVERIFY(manifest.open(QIODevice::WriteOnly | QIODevice::Truncate));
    const QJsonObject metadata{
        {QStringLiteral("visibleMonth"), QStringLiteral("2026-10")},
        {QStringLiteral("firstWeekday"), QStringLiteral("Monday")},
        {QStringLiteral("upcomingScope"), QStringLiteral("This month")},
        {QStringLiteral("eventTypeFilters"), QJsonArray{
            QStringLiteral("Vacation"), QStringLiteral("Holiday"),
            QStringLiteral("Workshop"), QStringLiteral("CM"),
            QStringLiteral("Meeting"), QStringLiteral("Other")}},
        {QStringLiteral("campusFilter"), QStringLiteral(
            "No campus IDs are attached to the fixture events")},
        {QStringLiteral("events"), QJsonArray{
            QJsonObject{
                {QStringLiteral("title"), QStringLiteral("Campus meeting")},
                {QStringLiteral("type"), QStringLiteral("Meeting")},
                {QStringLiteral("date"), QStringLiteral("2026-10-06")},
                {QStringLiteral("startTime"), QStringLiteral("09:30")},
                {QStringLiteral("endTime"), QStringLiteral("10:30")}},
            QJsonObject{
                {QStringLiteral("title"), QStringLiteral("Term planning")},
                {QStringLiteral("type"), QStringLiteral("Workshop")},
                {QStringLiteral("date"), QStringLiteral("2026-10-15")},
                {QStringLiteral("startTime"), QStringLiteral("09:30")},
                {QStringLiteral("endTime"), QStringLiteral("10:30")}},
            QJsonObject{
                {QStringLiteral("title"), QStringLiteral("School holiday")},
                {QStringLiteral("type"), QStringLiteral("Holiday")},
                {QStringLiteral("date"), QStringLiteral("2026-10-26")},
                {QStringLiteral("allDay"), true}}}}
    };
    const QByteArray manifestBytes = QJsonDocument(QJsonObject{
        {QStringLiteral("fixture"), metadata},
        {QStringLiteral("captures"), captures}
    }).toJson(QJsonDocument::Indented);
    QCOMPARE(manifest.write(manifestBytes), qint64(manifestBytes.size()));
}

QTEST_MAIN(CalendarPageTests)

#include "calendar_page_tests.moc"
