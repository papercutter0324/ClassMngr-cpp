#include "data/database/workspace_store.h"
#include "features/calendar/academic_calendar_schedule.h"
#include "next/application/calendar_event_repeat_occurrence_plan.h"
#include "next/platform/workspace_store_calendar_actions.h"

#include <QJsonDocument>
#include <QTemporaryDir>
#include <QtTest/QtTest>

using namespace ClassMngr::Next;

namespace
{

QString createWorkspace(QTemporaryDir& directory, const QString& name)
{
    const QString path = directory.filePath(name);
    WorkspaceStore store;
    if (!store.open(path))
    {
        return {};
    }
    store.close();
    return path;
}

Application::CalendarEventSaveInput timedEvent(
    const std::string& title,
    const std::string& date,
    const std::string& eventType)
{
    return {
        .title = title,
        .startDate = date,
        .endDate = date,
        .startTime = std::string("09:00"),
        .endTime = std::string("10:00"),
        .eventType = eventType
    };
}

} // namespace

class CalendarWorkspaceStoreActionsTests final : public QObject
{
    Q_OBJECT

private slots:
    void singleEventPersistsAndReadsBackAtPath();
    void repeatSeriesCanBeEditedAndDeletedFromDate();
    void preferencesPersistAtPath();
    void missingWorkspaceReturnsReadFailure();
};

void CalendarWorkspaceStoreActionsTests::singleEventPersistsAndReadsBackAtPath()
{
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const QString path = createWorkspace(
        temporary, QStringLiteral("calendar-events.tps"));
    QVERIFY(!path.isEmpty());

    const auto saveResult = Platform::saveCalendarEventAtPath(
        path,
        timedEvent("Persisted meeting", "2026-08-17", "Meeting"));
    QVERIFY(saveResult);

    const auto event = Platform::readCalendarEventAtPath(
        path, saveResult.value());
    QVERIFY(event);
    QCOMPARE(event.value().id, saveResult.value());
    QCOMPARE(event.value().title, std::string("Persisted meeting"));
    QCOMPARE(event.value().startDate, std::string("2026-08-17"));
    QCOMPARE(event.value().endDate, std::string("2026-08-17"));
    QCOMPARE(event.value().startTime, std::optional<std::string>("09:00"));
    QCOMPARE(event.value().endTime, std::optional<std::string>("10:00"));
    QCOMPARE(event.value().eventType, Application::CalendarEventTypeName("Meeting"));
    QVERIFY(!event.value().repeatSeriesId.has_value());
}

void CalendarWorkspaceStoreActionsTests::repeatSeriesCanBeEditedAndDeletedFromDate()
{
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const QString path = createWorkspace(
        temporary, QStringLiteral("calendar-repeat-series.tps"));
    QVERIFY(!path.isEmpty());

    const auto seriesId = Domain::CalendarEventSeriesId::fromString(
        "series-workshop-1");
    QVERIFY(seriesId.has_value());
    const Application::CalendarEventSeriesCreateInput createInput{
        .seed = timedEvent("Repeat workshop", "2026-08-18", "Workshop"),
        .repeatSeriesId = seriesId,
        .frequency = Application::CalendarEventRepeatFrequency::Daily,
        .untilDate = "2026-08-20"
    };
    const auto created = Platform::createCalendarEventSeriesAtPath(
        path, createInput);
    QVERIFY(created);
    QCOMPARE(created.value().size(), std::size_t(3));

    const Application::CalendarEventSeriesEditInput editInput{
        .repeatSeriesId = seriesId,
        .startDate = "2026-08-19",
        .editedStartDate = "2026-08-19",
        .editedEndDate = "2026-08-19",
        .title = "Updated workshop",
        .startTime = std::string("09:00"),
        .endTime = std::string("10:00"),
        .eventType = "Workshop"
    };
    QVERIFY(Platform::editCalendarEventSeriesAtPath(path, editInput));

    const auto first = Platform::readCalendarEventAtPath(
        path, created.value().at(0));
    const auto second = Platform::readCalendarEventAtPath(
        path, created.value().at(1));
    const auto third = Platform::readCalendarEventAtPath(
        path, created.value().at(2));
    QVERIFY(first);
    QVERIFY(second);
    QVERIFY(third);
    QCOMPARE(first.value().title, std::string("Repeat workshop"));
    QCOMPARE(second.value().title, std::string("Updated workshop"));
    QCOMPARE(third.value().title, std::string("Updated workshop"));
    QCOMPARE(second.value().startDate, std::string("2026-08-19"));
    QCOMPARE(third.value().startDate, std::string("2026-08-20"));

    const Application::CalendarEventSeriesDeleteInput deleteInput{
        .repeatSeriesId = seriesId,
        .startDate = "2026-08-20"
    };
    QVERIFY(Platform::deleteCalendarEventSeriesAtPath(path, deleteInput));
    QVERIFY(Platform::readCalendarEventAtPath(path, created.value().at(0)));
    QVERIFY(Platform::readCalendarEventAtPath(path, created.value().at(1)));
    QVERIFY(!Platform::readCalendarEventAtPath(path, created.value().at(2)));
}

void CalendarWorkspaceStoreActionsTests::preferencesPersistAtPath()
{
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const QString path = createWorkspace(
        temporary, QStringLiteral("calendar-preferences.tps"));
    QVERIFY(!path.isEmpty());

    const Application::CalendarEventDisplayPreferences preferences{
        .showEventsAtAllCampuses = true,
        .hideStartOfTermEvents = true
    };
    QVERIFY(Platform::saveCalendarEventDisplayPreferencesAtPath(
        path, preferences));
    const auto loaded = Platform::readCalendarEventDisplayPreferencesAtPath(path);
    QVERIFY(loaded);
    QCOMPARE(loaded.value(), preferences);

    QVERIFY(Platform::saveCalendarFirstDayOfWeekAtPath(
        path, Application::CalendarFirstDayOfWeek::Monday));
    const auto firstDay = Platform::readCalendarFirstDayOfWeekAtPath(path);
    QVERIFY(firstDay);
    QCOMPARE(firstDay.value(), Application::CalendarFirstDayOfWeek::Monday);

    AcademicCalendarSchedule schedule;
    AcademicYearSchedule elementary =
        schedule.defaultYearSchedule(SchoolLevel::Elementary, 2026);
    AcademicYearSchedule middle =
        schedule.defaultYearSchedule(SchoolLevel::Middle, 2026);
    elementary.winterStart = QDate(2025, 12, 29);
    middle.winterStart = QDate(2025, 12, 29);
    QVERIFY(elementary.isValid());
    QVERIFY(middle.isValid());
    schedule.setYearSchedules(2026, elementary, middle);

    const QByteArray scheduleJson =
        QJsonDocument(schedule.toJson()).toJson(QJsonDocument::Compact);
    const Application::AcademicCalendarScheduleJsonText schedulePayload(
        std::string(scheduleJson.constData(),
                    static_cast<std::size_t>(scheduleJson.size())));
    QVERIFY(Platform::writeAcademicCalendarScheduleAtPath(
        path, schedulePayload));
    const auto storedSchedule =
        Platform::readAcademicCalendarScheduleAtPath(path);
    QVERIFY(storedSchedule);
    QCOMPARE(storedSchedule.value(), schedulePayload);

    const std::string& storedText = storedSchedule.value().value();
    const QJsonDocument storedJson = QJsonDocument::fromJson(QByteArray(
        storedText.data(), static_cast<qsizetype>(storedText.size())));
    QVERIFY(storedJson.isObject());
    AcademicCalendarSchedule roundTrippedSchedule;
    QVERIFY(roundTrippedSchedule.fromJson(storedJson.object()));
    QCOMPARE(roundTrippedSchedule
                 .yearSchedule(SchoolLevel::Elementary, 2026)
                 .winterStart,
             QDate(2025, 12, 29));
    QCOMPARE(roundTrippedSchedule
                 .yearSchedule(SchoolLevel::Middle, 2026)
                 .winterStart,
             QDate(2025, 12, 29));
}

void CalendarWorkspaceStoreActionsTests::missingWorkspaceReturnsReadFailure()
{
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const QString missingPath = temporary.filePath(
        QStringLiteral("missing-calendar.tps"));
    const auto eventId = Domain::CalendarEventId::fromString("missing-event");
    QVERIFY(eventId.has_value());

    const auto missing = Platform::readCalendarEventAtPath(
        missingPath, *eventId);
    QVERIFY(!missing);
    QVERIFY(!missing.error().message.empty());
}

QTEST_GUILESS_MAIN(CalendarWorkspaceStoreActionsTests)

#include "calendar_workspace_store_actions_tests.moc"
