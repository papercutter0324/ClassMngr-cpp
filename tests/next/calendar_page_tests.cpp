#include "core/process_memory_snapshot.h"
#include "core/zip_archive_read_adapter.h"
#include "core/archive_read_port.h"
#include "data/database/workspace_store.h"
#include "features/calendar/academic_calendar_schedule.h"
#include "features/calendar/ui/academic_calendar_provider.h"
#include "features/calendar/ui/calendar_event_model.h"
#include "next/resources/resource_catalog.h"
#include "next/resources/resource_loader.h"
#include "next/resources/resource_scope.h"
#include "next/platform/calendar_page_resource_campus_directory_query_adapter.h"
#include "next/platform/workspace_store_calendar_actions.h"
#include "next/platform/application_services_academic_calendar_schedule_preferences_port.h"
#include "next/platform/application_services_calendar_first_day_of_week_preferences_port.h"
#include "next/platform/application_services_calendar_event_save_port.h"
#include "next/application/calendar_event_save_use_case.h"
#include "next/bootstrap/qt_page_lifecycle_diagnostics.h"
#include "next/platform/qt_font_family_availability_adapter.h"
#include "next/ui/calendar_event_dialog.h"
#include "next/ui/calendar_event_import_service.h"
#include "next/ui/calendar_page.h"
#include "next/ui/calendar_preferences_dialog.h"
#include "next/ui/font_size_presenter.h"
#include "next/ui/page_host.h"
#include "preferences_test_ports.h"

#include <QApplication>
#include <QAccessible>
#include <QColor>
#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QDateEdit>
#include <QDateTime>
#include <QCryptographicHash>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFont>
#include <QFontDatabase>
#include <QFontMetricsF>
#include <QFontInfo>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QImage>
#include <QJsonDocument>
#include <QJsonArray>
#include <QLabel>
#include <QLocale>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLayout>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPalette>
#include <QPainter>
#include <QPointer>
#include <QSignalSpy>
#include <QPushButton>
#include <QQuickItem>
#include <QQuickWidget>
#include <QRawFont>
#include <QScrollArea>
#include <QScrollBar>
#include <QSizePolicy>
#include <QSysInfo>
#include <QSpinBox>
#include <QStyle>
#include <QTemporaryDir>
#include <QTimer>
#include <QToolButton>
#include <QTranslator>
#include <QtMath>
#include <QUuid>
#include <QSqlQuery>
#include <QVBoxLayout>
#include <QtTest/QtTest>

#include <cstring>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

using namespace ClassMngr::Next;

namespace
{

QStringList* capturedCalendarImportCheckpointMessages = nullptr;
QtMessageHandler previousCalendarImportMessageHandler = nullptr;

void captureCalendarImportCheckpointMessages(
    const QtMsgType type,
    const QMessageLogContext& context,
    const QString& message
    )
{
    if (capturedCalendarImportCheckpointMessages
        && message.startsWith(QStringLiteral(
            "ClassMngrNext calendar import checkpoint=")))
    {
        capturedCalendarImportCheckpointMessages->append(message);
    }
    if (previousCalendarImportMessageHandler)
    {
        previousCalendarImportMessageHandler(type, context, message);
    }
}

class CalendarImportCheckpointMessageCapture final
{
public:
    CalendarImportCheckpointMessageCapture()
    {
        capturedCalendarImportCheckpointMessages = &messages;
        previousCalendarImportMessageHandler = qInstallMessageHandler(
            captureCalendarImportCheckpointMessages);
    }

    ~CalendarImportCheckpointMessageCapture()
    {
        qInstallMessageHandler(previousCalendarImportMessageHandler);
        previousCalendarImportMessageHandler = nullptr;
        capturedCalendarImportCheckpointMessages = nullptr;
    }

    QStringList messages;
};

class RecordingArchiveReadPort final : public ArchiveReadPort
{
public:
    explicit RecordingArchiveReadPort(
        std::unique_ptr<ArchiveReadPort> delegate
        )
        : m_delegate(std::move(delegate))
    {
    }

    [[nodiscard]] std::unique_ptr<ArchiveReadSession> open(
        const QByteArray& data
        ) const override
    {
        ++byteOpenCount;
        return m_delegate->open(data);
    }

    [[nodiscard]] std::unique_ptr<ArchiveReadSession> open(
        QIODevice& device
        ) const override
    {
        ++deviceOpenCount;
        return m_delegate->open(device);
    }

    mutable int byteOpenCount = 0;
    mutable int deviceOpenCount = 0;

private:
    std::unique_ptr<ArchiveReadPort> m_delegate;
};

class OtherPage final : public Ui::Page
{
public:
    using Ui::Page::Page;
};

class CalendarImportTestReply final : public QNetworkReply
{
public:
    CalendarImportTestReply(
        const QNetworkAccessManager::Operation operation,
        const QNetworkRequest& request,
        QObject* parent
        )
        : QNetworkReply(parent)
    {
        setOperation(operation);
        setRequest(request);
        setUrl(request.url());
        setAttribute(QNetworkRequest::HttpStatusCodeAttribute, 200);
        setOpenMode(QIODevice::ReadOnly | QIODevice::Unbuffered);
    }

    void sendBody(const QByteArray& body)
    {
        constexpr qsizetype ChunkSize = 64 * 1024;
        qsizetype offset = 0;
        while (offset < body.size())
        {
            const qsizetype count = qMin(ChunkSize, body.size() - offset);
            m_readBuffer.append(body.constData() + offset, count);
            offset += count;
            emit readyRead();
        }
        complete();
    }

    void failRemote()
    {
        if (isFinished())
        {
            return;
        }
        setError(QNetworkReply::RemoteHostClosedError,
            QStringLiteral("Injected Calendar import transport failure"));
        complete();
    }

    void abort() override
    {
        if (isFinished())
        {
            return;
        }
        setError(QNetworkReply::OperationCanceledError,
            QStringLiteral("Operation canceled"));
        complete();
    }

    [[nodiscard]] qint64 bytesAvailable() const override
    {
        return m_readBuffer.size() + QNetworkReply::bytesAvailable();
    }

protected:
    [[nodiscard]] qint64 readData(char* data, const qint64 maxSize) override
    {
        const qint64 count = qMin(maxSize, static_cast<qint64>(m_readBuffer.size()));
        if (count <= 0)
        {
            return -1;
        }
        std::memcpy(data, m_readBuffer.constData(), static_cast<std::size_t>(count));
        m_readBuffer.remove(0, static_cast<qsizetype>(count));
        return count;
    }

private:
    void complete()
    {
        if (isFinished())
        {
            return;
        }
        setFinished(true);
        emit finished();
    }

    QByteArray m_readBuffer;
};

struct CalendarImportTestState final
{
    QByteArray body;
    QStringList requestedUrls;
    QList<QPointer<CalendarImportTestReply>> pendingReplies;
    bool holdResponses = false;
    bool closeWithoutResponse = false;
    int requestCount = 0;
};

class CalendarImportTestNetworkAccessManager final : public QNetworkAccessManager
{
public:
    CalendarImportTestNetworkAccessManager(
        std::shared_ptr<CalendarImportTestState> state,
        QObject* parent
        )
        : QNetworkAccessManager(parent),
          m_state(std::move(state))
    {
    }

protected:
    QNetworkReply* createRequest(
        const Operation operation,
        const QNetworkRequest& request,
        QIODevice* outgoingData = nullptr
        ) override
    {
        Q_UNUSED(outgoingData);
        auto* const reply = new CalendarImportTestReply(operation, request, this);
        ++m_state->requestCount;
        m_state->requestedUrls.append(request.url().toString());
        if (m_state->holdResponses)
        {
            m_state->pendingReplies.append(reply);
        }
        else
        {
            const auto state = m_state;
            QTimer::singleShot(0, reply, [state, reply]
            {
                respond(state, reply);
            });
        }
        return reply;
    }

public:
    static void respondForTest(
        const std::shared_ptr<CalendarImportTestState>& state,
        CalendarImportTestReply* reply
        )
    {
        respond(state, reply);
    }

private:
    static void respond(
        const std::shared_ptr<CalendarImportTestState>& state,
        CalendarImportTestReply* reply
        )
    {
        if (!reply || reply->isFinished())
        {
            return;
        }
        if (state->closeWithoutResponse)
        {
            reply->failRemote();
            return;
        }
        reply->sendBody(state->body);
    }

    std::shared_ptr<CalendarImportTestState> m_state;
};

class CalendarImportTestTransport final
{
public:
    explicit CalendarImportTestTransport(QByteArray body)
        : m_state(std::make_shared<CalendarImportTestState>())
    {
        m_state->body = std::move(body);
    }

    [[nodiscard]] QString url() const
    {
        return QStringLiteral("http://calendar-import.test/calendar.xlsx");
    }

    [[nodiscard]] std::function<QNetworkAccessManager*(QObject*)>
    networkAccessManagerFactory() const
    {
        const auto state = m_state;
        return [state](QObject* parent)
        {
            return new CalendarImportTestNetworkAccessManager(state, parent);
        };
    }

    void holdResponses(const bool hold) { m_state->holdResponses = hold; }
    void closeWithoutResponse(const bool close)
    {
        m_state->closeWithoutResponse = close;
    }
    void setBody(QByteArray body) { m_state->body = std::move(body); }
    [[nodiscard]] int requestCount() const { return m_state->requestCount; }
    [[nodiscard]] QString lastRequestedUrl() const
    {
        return m_state->requestedUrls.isEmpty()
            ? QString{}
            : m_state->requestedUrls.constLast();
    }

    void respondToHeldRequests()
    {
        const auto pending = std::move(m_state->pendingReplies);
        m_state->pendingReplies.clear();
        for (const QPointer<CalendarImportTestReply>& reply : pending)
        {
            CalendarImportTestNetworkAccessManager::respondForTest(
                m_state, reply.data());
        }
    }

private:
    std::shared_ptr<CalendarImportTestState> m_state;
};

struct CalendarViewTestState final
{
    int factoryCalls = 0;
    int destructionCount = 0;
    int paletteApplyCount = 0;
    int eventTypeColorApplyCount = 0;
    bool lastRequestHasBorrowedProviders = false;
    bool providersAliveWhenViewDestroyed = true;
    QDate lastRequestedMonth;
    std::function<void(int, int)> displayedMonthChanged;
};

class FakeCalendarView final : public Ui::CalendarView
{
public:
    FakeCalendarView(
        const Ui::CalendarViewRequest& request,
        CalendarViewTestState& state,
        const bool loaded
        )
        : m_widget(new QWidget(request.parent)),
          m_eventModel(request.eventModel),
          m_academicProvider(request.academicProvider),
          m_state(state),
          m_loaded(loaded)
    {
        m_widget->setObjectName(QStringLiteral("fakeCalendarViewWidget"));
    }

    ~FakeCalendarView() override
    {
        m_state.providersAliveWhenViewDestroyed =
            m_state.providersAliveWhenViewDestroyed
            && !m_eventModel.isNull() && !m_academicProvider.isNull();
        ++m_state.destructionCount;
        delete m_widget;
    }

    QWidget* widget() const noexcept override { return m_widget; }
    bool isLoaded() const noexcept override { return m_loaded; }

    void applyPalette(const QPalette&, bool) override
    {
        ++m_state.paletteApplyCount;
    }

    void setEventTypeColors(const QVariantMap&, const QVariantMap&) override
    {
        ++m_state.eventTypeColorApplyCount;
    }

private:
    QWidget* m_widget = nullptr;
    QPointer<CalendarEventModel> m_eventModel;
    QPointer<AcademicCalendarProvider> m_academicProvider;
    CalendarViewTestState& m_state;
    bool m_loaded = false;
};

class EmptyQuickWidgetCalendarView final : public Ui::CalendarView
{
public:
    explicit EmptyQuickWidgetCalendarView(const Ui::CalendarViewRequest& request)
        : m_widget(new QQuickWidget(request.parent))
    {
        m_widget->setObjectName(QStringLiteral("calendarMemoryProbeQuickWidget"));
        m_widget->setResizeMode(QQuickWidget::SizeRootObjectToView);
        m_widget->setMinimumHeight(500);
        m_widget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    }

    ~EmptyQuickWidgetCalendarView() override
    {
        delete m_widget;
    }

    QWidget* widget() const noexcept override { return m_widget; }
    // The diagnostic needs CalendarPage to retain its models and surrounding
    // shell while deliberately omitting the grid QML root.
    bool isLoaded() const noexcept override { return m_widget != nullptr; }
    void applyPalette(const QPalette&, bool) override {}
    void setEventTypeColors(const QVariantMap&, const QVariantMap&) override {}

private:
    QQuickWidget* m_widget = nullptr;
};

class CalendarPageFixture final
{
public:
    Test::MemoryPreferences preferences;
    CalendarViewTestState calendarViewState;
    std::unique_ptr<QTranslator> koreanTranslator;
    std::unique_ptr<Resources::ResourceCatalog> catalog;
    std::unique_ptr<Resources::ResourceLoader> loader;
    std::vector<Resources::ScopedResource<Resources::RegisteredFont>>
        captureInterFonts;
    std::optional<Resources::ScopedResource<Resources::RegisteredFont>>
        capturePretendardFont;
    Platform::QtFontFamilyAvailabilityAdapter captureFontAvailability;
    std::unique_ptr<Ui::FontSizePresenter> captureFontSizePresenter;
    QString captureInterFamily;
    QString capturePretendardFamily;
    QJsonArray captureFontResources;
    std::unique_ptr<Ui::PageHost> host;
    QTemporaryDir temporaryDirectory;
    QString initializationError;
    QString path;
    QString calendarImportSourceUrl;
    QString calendarImportTemporaryFileTemplate;
    std::function<QNetworkAccessManager*(QObject*)>
        calendarImportNetworkAccessManagerFactory;
    std::shared_ptr<ArchiveReadPort> calendarArchiveReadPort{
        makeZipArchiveReadPort()
        };
    Ui::CalendarViewFactory calendarViewFactory;
    std::unique_ptr<Bootstrap::QtPageLifecycleDiagnosticsRecorder>
        lifecycleDiagnosticsRecorder;
    QVector<QJsonObject> lifecycleRecords;
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
        const bool seedCalendar = false,
        const bool activateCalendar = true,
        const bool captureLifecycle = false,
        const bool loadProductionFonts = false)
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
        if (loadProductionFonts && !loadPackagedApplicationFonts(korean))
        {
            return false;
        }
        host = std::make_unique<Ui::PageHost>(*loader, preferences.model);

        Ui::CalendarPageActions actions;
        actions.activeWorkspacePath = [this] { return path; };
        actions.calendarImportSourceUrl = calendarImportSourceUrl;
        actions.calendarImportTemporaryFileTemplate =
            calendarImportTemporaryFileTemplate;
        actions.calendarImportNetworkAccessManagerFactory =
            calendarImportNetworkAccessManagerFactory;
        actions.calendarArchiveReadPort = calendarArchiveReadPort;
        actions.calendarViewFactory = calendarViewFactory;
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
                    lazyBeforeActivation = !instance->calendarViewWidget()
                        && !instance->eventCache()
                        && !instance->eventModel()
                        && calendarViewState.factoryCalls == 0;
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
        if (captureLifecycle)
        {
            lifecycleDiagnosticsRecorder = std::make_unique<
                Bootstrap::QtPageLifecycleDiagnosticsRecorder>(
                *host,
                std::unique_ptr<ProcessMemorySnapshotProvider>{},
                [this](const QByteArray& record) {
                    lifecycleRecords.push_back(
                        QJsonDocument::fromJson(record).object());
                },
                [](const QWidget* page)
                    -> std::optional<Ui::CalendarPageRuntimeMetrics> {
                    const auto* const calendarPage =
                        qobject_cast<const Ui::CalendarPage*>(page);
                    if (!calendarPage)
                    {
                        return std::nullopt;
                    }
                    return calendarPage->runtimeMetrics();
                });
        }
        host->resize(1280, 920);
        host->show();
        if (!activateCalendar)
        {
            return true;
        }
        if (!host->activatePage(QStringLiteral("calendar")) || !page)
        {
            initializationError = QStringLiteral("calendar page activation failed");
            return false;
        }
        QApplication::processEvents();
        return true;
    }

    void packagedFontIdentity(
        const QString& resourceId,
        const Resources::RegisteredFont& registration,
        const bool fileBacked)
    {
        const auto id = Resources::ResourceId::fromString(resourceId);
        if (!id)
        {
            initializationError = QStringLiteral(
                "Calendar capture font resource ID is invalid: %1")
                .arg(resourceId);
            return;
        }
        const Resources::ResourceDescriptor* const descriptor =
            catalog->find(*id);
        if (!descriptor)
        {
            initializationError = QStringLiteral(
                "Calendar capture font is missing from the package catalog: %1")
                .arg(resourceId);
            return;
        }

        const QString packagePath = QDir(catalog->resourceRoot()).filePath(
            descriptor->packageRelativePath());
        QFile packageFile(packagePath);
        if (!packageFile.open(QIODevice::ReadOnly))
        {
            initializationError = QStringLiteral(
                "Calendar capture font package file cannot be read: %1")
                .arg(packagePath);
            return;
        }
        const QByteArray packageBytes = packageFile.readAll();
        if (packageBytes.isEmpty())
        {
            initializationError = QStringLiteral(
                "Calendar capture font package file is empty: %1")
                .arg(packagePath);
            return;
        }
        const QByteArray actualSha256 = QCryptographicHash::hash(
            packageBytes, QCryptographicHash::Sha256).toHex();
        if (descriptor->expectedSha256()
            && actualSha256 != descriptor->expectedSha256()->toHex())
        {
            initializationError = QStringLiteral(
                "Calendar capture font package hash differs from its catalog: %1")
                .arg(resourceId);
            return;
        }

        QJsonArray familyNames;
        for (const QString& family : registration.familyNames())
        {
            familyNames.append(family);
        }
        captureFontResources.append(QJsonObject{
            {QStringLiteral("resourceId"), resourceId},
            {QStringLiteral("registrationPath"), fileBacked
                ? QStringLiteral("ResourceLoader::loadFileBackedFont")
                : QStringLiteral("ResourceLoader::loadFont")},
            {QStringLiteral("catalogLocation"),
             descriptor->location() == Resources::ResourceLocation::PackageFile
                ? QStringLiteral("packageFile") : QStringLiteral("qtResource")},
            {QStringLiteral("packageRelativePath"),
             descriptor->packageRelativePath()},
            {QStringLiteral("packageFile"), QFileInfo(packagePath).canonicalFilePath()},
            {QStringLiteral("packageFileBytes"), packageBytes.size()},
            {QStringLiteral("packageFileSha256"),
             QString::fromLatin1(actualSha256)},
            {QStringLiteral("catalogExpectedBytes"),
             descriptor->expectedBytes().has_value()
                ? QJsonValue(*descriptor->expectedBytes()) : QJsonValue{}},
            {QStringLiteral("catalogExpectedSha256"),
             descriptor->expectedSha256().has_value()
                ? QJsonValue(QString::fromLatin1(
                    descriptor->expectedSha256()->toHex())) : QJsonValue{}},
            {QStringLiteral("registeredFamilyNames"), familyNames},
            {QStringLiteral("registeredSourceFile"),
             registration.sourceFilePath()}
        });
    }

    [[nodiscard]] bool loadPackagedApplicationFonts(const bool korean)
    {
        const auto startupScope = Resources::ResourceScope::create(
            Resources::ResourceScopeKind::Startup,
            QStringLiteral("application-bootstrap"));
        if (!startupScope)
        {
            initializationError = QStringLiteral(
                "Calendar capture startup font scope could not be created");
            return false;
        }

        const std::array<QString, 6> interResources{
            QStringLiteral("resource:assets/fonts/Inter-Regular.ttf"),
            QStringLiteral("resource:assets/fonts/Inter-Medium.ttf"),
            QStringLiteral("resource:assets/fonts/Inter-SemiBold.ttf"),
            QStringLiteral("resource:assets/fonts/Inter-Bold.ttf"),
            QStringLiteral("resource:assets/fonts/Inter-BoldItalic.ttf"),
            QStringLiteral("resource:assets/fonts/Inter-Black.ttf")
        };
        for (const QString& resourceName : interResources)
        {
            const auto id = Resources::ResourceId::fromString(resourceName);
            if (!id)
            {
                initializationError = QStringLiteral(
                    "Calendar capture Inter font ID is invalid: %1")
                    .arg(resourceName);
                return false;
            }
            const auto request = Resources::ResourceRequest::create(
                *id, *startupScope);
            if (!request)
            {
                initializationError = QStringLiteral(
                    "Calendar capture Inter font request is invalid: %1")
                    .arg(resourceName);
                return false;
            }
            auto font = loader->loadFileBackedFont(*request);
            if (!font)
            {
                initializationError = QStringLiteral(
                    "Calendar capture Inter font registration failed for %1: %2")
                    .arg(resourceName, font.error().message);
                return false;
            }
            if (font->get().familyNames().isEmpty())
            {
                initializationError = QStringLiteral(
                    "Calendar capture Inter registration has no family: %1")
                    .arg(resourceName);
                return false;
            }
            packagedFontIdentity(
                resourceName, font->get(), true);
            if (!initializationError.isEmpty())
            {
                return false;
            }
            if (captureInterFamily.isEmpty())
            {
                captureInterFamily = font->get().familyNames().value(0);
            }
            captureInterFonts.emplace_back(std::move(*font));
        }

        const QString pretendardResource = QStringLiteral(
            "resource:assets/fonts/PretendardVariable.ttf");
        const auto pretendardId =
            Resources::ResourceId::fromString(pretendardResource);
        if (!pretendardId)
        {
            initializationError = QStringLiteral(
                "Calendar capture Pretendard font ID is invalid");
            return false;
        }
        const auto pretendardRequest = Resources::ResourceRequest::create(
            *pretendardId, *startupScope);
        if (!pretendardRequest)
        {
            initializationError = QStringLiteral(
                "Calendar capture Pretendard font request is invalid");
            return false;
        }
        auto pretendard = loader->loadFont(*pretendardRequest);
        if (!pretendard)
        {
            initializationError = QStringLiteral(
                "Calendar capture Pretendard font registration failed: %1")
                .arg(pretendard.error().message);
            return false;
        }
        if (pretendard->get().familyNames().isEmpty())
        {
            initializationError = QStringLiteral(
                "Calendar capture Pretendard registration has no family");
            return false;
        }
        packagedFontIdentity(pretendardResource, pretendard->get(), false);
        if (!initializationError.isEmpty())
        {
            return false;
        }
        capturePretendardFamily = pretendard->get().familyNames().value(0);
        capturePretendardFont.emplace(std::move(*pretendard));

        captureFontSizePresenter = std::make_unique<Ui::FontSizePresenter>(
            *qApp, captureFontAvailability);
        captureFontSizePresenter->applyStartupPreference(
            preferences.model.snapshot().fontSize(),
            korean ? QStringLiteral("ko_KR") : QStringLiteral("en_US"),
            captureInterFamily, capturePretendardFamily);
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
        {QStringLiteral("handleCount"),
         static_cast<double>(snapshot.handleCount)},
        {QStringLiteral("threadCount"),
         static_cast<double>(snapshot.threadCount)},
        {QStringLiteral("workingSetBytesAvailable"),
         snapshot.workingSetBytesAvailable},
        {QStringLiteral("privateUsageBytesAvailable"),
         snapshot.privateUsageBytesAvailable},
        {QStringLiteral("threadCountAvailable"),
         snapshot.threadCountAvailable},
        {QStringLiteral("capturedAt"),
         snapshot.capturedAt.toString(Qt::ISODateWithMs)}
    };
}

void appendCalendarQuickItems(
    QQuickItem* parent,
    QList<QQuickItem*>& items
    )
{
    if (!parent)
    {
        return;
    }
    for (QQuickItem* const child : parent->childItems())
    {
        items.push_back(child);
        appendCalendarQuickItems(child, items);
    }
}

bool setCalendarWidgetMonth(QWidget* widget, const QDate& requestedMonth)
{
    if (!widget || !requestedMonth.isValid())
    {
        return false;
    }
    const QDate target(
        requestedMonth.year(), requestedMonth.month(), 1);
    for (int attempt = 0; attempt < 24; ++attempt)
    {
        const QDate current = widget->property(
            "calendarShownMonth").toDate();
        if (current == target)
        {
            return true;
        }
        const QString buttonName = current < target
            ? QStringLiteral("calendarWidgetNextMonth")
            : QStringLiteral("calendarWidgetPreviousMonth");
        QToolButton* const button = widget->findChild<QToolButton*>(buttonName);
        if (!button || !button->isEnabled())
        {
            return false;
        }
        QTest::mouseClick(button, Qt::LeftButton);
        QApplication::processEvents();
    }
    return widget->property("calendarShownMonth").toDate() == target;
}

} // namespace

class CalendarPageTests final : public QObject
{
    Q_OBJECT

private slots:
    void calendarLifecycleDiagnosticsCaptureContentReadyAndSuspension();
    void calendarViewMemoryProbe();
    void calendarWidgetGridCandidatePreservesMonthAndActivation();
    void calendarWidgetGridSupportsEmptySparseAndBusyMonths();
    void calendarContentIsLazyBoundedAndReleased();
    void injectedCalendarViewFailureReleasesProvidersAndRetriesWithState();
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
    void calendarImportUsesStreamingTypedPathApplyAndRefreshesOnlyOnWrites();
    void calendarImportUsesEnvironmentSourceUrlOverride();
    void calendarImportMalformedAndTransportFailuresAreRecoverable();
    void calendarImportSuspensionAndPathChangeCancelBeforeApply();
    void calendarImportServiceDestructionCleansStagingWithoutWrites();
    void calendarImportQueryFailureIsVisibleAndWritesNothing();
    void calendarImportBatchSaveFailureRollsBackAllRows();
    void captureEnglishAndKoreanLightAndDark();
    void calendarWidgetViewMatchesQmlCapturesAcrossContexts();
};

void CalendarPageTests::calendarContentIsLazyBoundedAndReleased()
{
    CalendarPageFixture fixture;
    QVERIFY(fixture.initialize(false));
    QVERIFY(fixture.lazyBeforeActivation);
    QVERIFY(fixture.page->calendarViewWidget());
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

void CalendarPageTests::calendarLifecycleDiagnosticsCaptureContentReadyAndSuspension()
{
    CalendarPageFixture fixture;
    QVERIFY2(fixture.initialize(false, false, false, true, true),
             qPrintable(fixture.initializationError));

    const auto findCalendarRecord = [&fixture](const QString& state)
        -> QJsonObject
    {
        for (const QJsonObject& record : fixture.lifecycleRecords)
        {
            if (record.value(QStringLiteral("event")).toString()
                    == QStringLiteral("page-lifecycle")
                && record.value(QStringLiteral("page_key")).toString()
                    == QStringLiteral("calendar")
                && record.value(QStringLiteral("state")).toString() == state)
            {
                return record;
            }
        }
        return {};
    };

    const QJsonObject activated = findCalendarRecord(QStringLiteral("activated"));
    QVERIFY(!activated.isEmpty());
    const QJsonObject activatedMetrics = activated.value(QStringLiteral("page"))
        .toObject().value(QStringLiteral("calendar_runtime_metrics")).toObject();
    QCOMPARE(activatedMetrics.value(QStringLiteral("observation")).toString(),
             QStringLiteral("content-ready"));
    QCOMPARE(activatedMetrics.value(QStringLiteral("calendar_view_count")).toInt(), 1);
    QVERIFY(activatedMetrics.contains(QStringLiteral("cache_event_count")));
    QVERIFY(activatedMetrics.contains(QStringLiteral("cache_loading")));
    const QJsonObject activatedMemory = activated.value(QStringLiteral("memory"))
        .toObject();
    QVERIFY(activatedMemory.value(QStringLiteral("working_set_bytes")).isDouble());
    QVERIFY(activatedMemory.value(QStringLiteral("private_usage_bytes")).isDouble());
    QVERIFY(activatedMemory.value(QStringLiteral("thread_count")).isDouble());

    QVERIFY(fixture.host->activatePage(QStringLiteral("other")));
    QApplication::processEvents();

    const QJsonObject suspended = findCalendarRecord(QStringLiteral("suspended"));
    QVERIFY(!suspended.isEmpty());
    const QJsonObject suspendedMetrics = suspended.value(QStringLiteral("page"))
        .toObject().value(QStringLiteral("calendar_runtime_metrics")).toObject();
    QCOMPARE(suspendedMetrics.value(QStringLiteral("observation")).toString(),
             QStringLiteral("post-suspension"));
    QCOMPARE(suspendedMetrics.value(QStringLiteral("calendar_view_count")).toInt(), 0);
    QCOMPARE(suspendedMetrics.value(QStringLiteral("calendar_root_object_count")).toInt(), 0);
    QVERIFY(suspended.value(QStringLiteral("memory")).toObject()
        .value(QStringLiteral("working_set_bytes")).isDouble());
}

void CalendarPageTests::calendarViewMemoryProbe()
{
    const QString arm = qEnvironmentVariable(
        "CLASSMNGR_CALENDAR_VIEW_PROBE_ARM").trimmed().toLower();
    if (arm.isEmpty())
    {
        QSKIP("diagnostic-only Calendar view probe arm was not selected");
    }
    QVERIFY(arm == QStringLiteral("qml")
        || arm == QStringLiteral("minimal-qquick")
        || arm == QStringLiteral("minimal-widget")
        || arm == QStringLiteral("widget-grid"));

    CalendarPageFixture fixture;
    if (arm == QStringLiteral("minimal-widget"))
    {
        fixture.calendarViewFactory = [&fixture](
            const Ui::CalendarViewRequest& request)
        {
            return Ui::CalendarViewCreationResult{
                std::make_unique<FakeCalendarView>(
                    request, fixture.calendarViewState, true),
                QString{}
            };
        };
    }
    else if (arm == QStringLiteral("widget-grid"))
    {
        fixture.calendarViewFactory = [](
            const Ui::CalendarViewRequest& request)
        {
            return Ui::createWidgetCalendarView(request);
        };
    }
    else if (arm == QStringLiteral("minimal-qquick"))
    {
        fixture.calendarViewFactory = [](const Ui::CalendarViewRequest& request)
        {
            return Ui::CalendarViewCreationResult{
                std::make_unique<EmptyQuickWidgetCalendarView>(request),
                QString{}
            };
        };
    }
    QVERIFY2(fixture.initialize(true, false, true, false, true),
             qPrintable(fixture.initializationError));

    PlatformProcessMemorySnapshotProvider memoryProvider;
    const ProcessMemorySnapshot baseline = memoryProvider.snapshot();
    QVERIFY(baseline.isAvailable);
    QVERIFY(fixture.host->activatePage(QStringLiteral("calendar")));
    QApplication::processEvents();
    const QDate probeMonth(2026, 10, 1);
    if (arm == QStringLiteral("qml"))
    {
        auto* const quick = qobject_cast<QQuickWidget*>(
            fixture.page->calendarViewWidget());
        QVERIFY(quick);
        QVERIFY(quick->rootObject()->setProperty(
            "shownDate", QDateTime(probeMonth, QTime(0, 0))));
        QTRY_VERIFY_WITH_TIMEOUT(
            fixture.page->eventModel()->isMonthLoaded(2026, 10), 5000);
    }
    else if (arm == QStringLiteral("widget-grid"))
    {
        QWidget* const viewWidget = fixture.page->calendarViewWidget();
        QVERIFY(viewWidget);
        QVERIFY(setCalendarWidgetMonth(viewWidget, probeMonth));
        QTRY_VERIFY_WITH_TIMEOUT(
            fixture.page->eventModel()->isMonthLoaded(2026, 10), 5000);
    }
    QApplication::processEvents();
    QTest::qWait(150);
    QVERIFY(fixture.page);
    QVERIFY(fixture.page->calendarViewWidget());
    const ProcessMemorySnapshot contentReady = memoryProvider.snapshot();
    QVERIFY(contentReady.isAvailable);
    const Ui::CalendarPageRuntimeMetrics contentMetrics =
        fixture.page->runtimeMetrics();

    bool quickWidget = false;
    bool quickRootPresent = false;
    int gridCellCount = 0;
    int emptyGridDayCount = 0;
    int eventfulGridDayCount = 0;
    int eventListViewCount = 0;
    int widgetDayCellCount = 0;
    int widgetEventButtonCount = 0;
    if (auto* const quick = qobject_cast<QQuickWidget*>(
            fixture.page->calendarViewWidget()))
    {
        quickWidget = true;
        QQuickItem* const quickRoot = quick->rootObject();
        quickRootPresent = quickRoot != nullptr;
        QList<QQuickItem*> quickItems;
        appendCalendarQuickItems(quickRoot, quickItems);
        for (QQuickItem* const item : quickItems)
        {
            if (item->objectName()
                == QStringLiteral("calendarEventListView"))
            {
                ++eventListViewCount;
            }
            if (item->metaObject()->indexOfProperty("dayEvents") >= 0)
            {
                ++gridCellCount;
                if (item->property("dayEvents").toList().isEmpty())
                {
                    ++emptyGridDayCount;
                }
                else
                {
                    ++eventfulGridDayCount;
                }
            }
        }
    }
    if (arm == QStringLiteral("widget-grid"))
    {
        QWidget* const viewWidget = fixture.page->calendarViewWidget();
        QVERIFY(viewWidget);
        widgetDayCellCount = viewWidget->findChildren<QWidget*>(
            QStringLiteral("calendarWidgetDayCell")).size();
        widgetEventButtonCount = viewWidget->findChildren<QPushButton*>(
            QStringLiteral("calendarWidgetEventButton")).size();
        QCOMPARE(widgetDayCellCount,
            viewWidget->property("calendarDayCellCount").toInt());
        QCOMPARE(widgetEventButtonCount,
            viewWidget->property("calendarEventButtonCount").toInt());
        QVERIFY(widgetDayCellCount >= 28);
        QCOMPARE(widgetEventButtonCount, 3);
    }

    QVERIFY(fixture.host->activatePage(QStringLiteral("other")));
    QApplication::processEvents();
    QTest::qWait(100);
    const ProcessMemorySnapshot postSuspension = memoryProvider.snapshot();
    QVERIFY(postSuspension.isAvailable);

    QJsonObject activatedLifecycle;
    QJsonObject suspendedLifecycle;
    for (const QJsonObject& record : fixture.lifecycleRecords)
    {
        if (record.value(QStringLiteral("page_key")).toString()
            != QStringLiteral("calendar"))
        {
            continue;
        }
        const QString state = record.value(QStringLiteral("state")).toString();
        if (state == QStringLiteral("activated"))
        {
            activatedLifecycle = record;
        }
        else if (state == QStringLiteral("suspended"))
        {
            suspendedLifecycle = record;
        }
    }
    QVERIFY(!activatedLifecycle.isEmpty());
    QVERIFY(!suspendedLifecycle.isEmpty());

    const QJsonObject result{
        {QStringLiteral("schema"),
         QStringLiteral("classmngr-phase9-calendar-view-memory-probe-v1")},
        {QStringLiteral("arm"), arm},
        {QStringLiteral("process_id"), QCoreApplication::applicationPid()},
        {QStringLiteral("captured_at_utc"),
         QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)},
        {QStringLiteral("view_is_quick_widget"), quickWidget},
        {QStringLiteral("quick_root_object_present"), quickRootPresent},
        {QStringLiteral("grid_cell_count"), gridCellCount},
        {QStringLiteral("empty_grid_day_count"), emptyGridDayCount},
        {QStringLiteral("eventful_grid_day_count"), eventfulGridDayCount},
        {QStringLiteral("event_list_view_count"), eventListViewCount},
        {QStringLiteral("widget_day_cell_count"), widgetDayCellCount},
        {QStringLiteral("widget_event_button_count"), widgetEventButtonCount},
        {QStringLiteral("baseline"), memoryJson(baseline)},
        {QStringLiteral("content_ready"), memoryJson(contentReady)},
        {QStringLiteral("post_suspension"), memoryJson(postSuspension)},
        {QStringLiteral("content_ready_runtime_metrics"), QJsonObject{
            {QStringLiteral("cache_event_count"), contentMetrics.cacheEventCount},
            {QStringLiteral("cache_date_bucket_count"), contentMetrics.cacheDateBucketCount},
            {QStringLiteral("cache_loaded_range_count"), contentMetrics.cacheLoadedRangeCount},
            {QStringLiteral("cache_retained_range_count"), contentMetrics.cacheRetainedRangeCount},
            {QStringLiteral("visible_grid_day_count"), contentMetrics.visibleGridDayCount},
            {QStringLiteral("upcoming_range_day_count"), contentMetrics.upcomingRangeDayCount},
            {QStringLiteral("upcoming_result_count"), contentMetrics.upcomingResultCount},
            {QStringLiteral("upcoming_result_limit"), contentMetrics.upcomingResultLimit},
            {QStringLiteral("calendar_view_count"), contentMetrics.calendarViewCount},
            {QStringLiteral("calendar_root_object_count"), contentMetrics.calendarRootObjectCount},
            {QStringLiteral("calendar_view_construction_count"), contentMetrics.calendarViewConstructionCount},
            {QStringLiteral("live_heavy_session_count"), contentMetrics.liveHeavySessionCount},
            {QStringLiteral("peak_live_heavy_session_count"), contentMetrics.peakLiveHeavySessionCount},
            {QStringLiteral("cache_loading"), contentMetrics.cacheLoading}
        }},
        {QStringLiteral("lifecycle_records"), QJsonArray{
            activatedLifecycle, suspendedLifecycle}}
    };
    const QString resultPath = qEnvironmentVariable(
        "CLASSMNGR_CALENDAR_VIEW_PROBE_OUTPUT").trimmed();
    if (!resultPath.isEmpty())
    {
        const QByteArray resultBytes =
            QJsonDocument(result).toJson(QJsonDocument::Compact);
        QFile resultFile(resultPath);
        QVERIFY2(resultFile.open(QIODevice::WriteOnly),
                 qPrintable(QStringLiteral(
                     "could not create diagnostic result file: %1")
                     .arg(resultPath)));
        QCOMPARE(resultFile.write(resultBytes),
                 static_cast<qint64>(resultBytes.size()));
    }
    qInfo().noquote().nospace()
        << "CALENDAR_VIEW_MEMORY_PROBE="
        << QString::fromUtf8(QJsonDocument(result).toJson(QJsonDocument::Compact));
}

void CalendarPageTests::calendarWidgetGridCandidatePreservesMonthAndActivation()
{
    struct CallbackState final
    {
        int dayActivationCount = 0;
        QDate activatedDate;
        int eventActivationCount = 0;
        QString activatedEventId;
        QDate displayedMonth;
    } callbackState;

    CalendarPageFixture fixture;
    fixture.calendarViewFactory = [&callbackState](
        const Ui::CalendarViewRequest& request)
    {
        Ui::CalendarViewRequest instrumentedRequest = request;
        const auto displayedMonthChanged = request.callbacks.displayedMonthChanged;
        instrumentedRequest.callbacks.dayActivated =
            [&callbackState](const int year, const int month, const int day)
        {
            ++callbackState.dayActivationCount;
            callbackState.activatedDate = QDate(year, month, day);
        };
        instrumentedRequest.callbacks.eventActivated =
            [&callbackState](const QString& eventId)
        {
            ++callbackState.eventActivationCount;
            callbackState.activatedEventId = eventId;
        };
        instrumentedRequest.callbacks.displayedMonthChanged =
            [&callbackState, displayedMonthChanged](
                const int year, const int month)
        {
            callbackState.displayedMonth = QDate(year, month, 1);
            if (displayedMonthChanged)
            {
                displayedMonthChanged(year, month);
            }
        };
        return Ui::createWidgetCalendarView(instrumentedRequest);
    };
    QVERIFY2(fixture.initialize(true, false, true, false),
             qPrintable(fixture.initializationError));
    QVERIFY(fixture.host->activatePage(QStringLiteral("calendar")));
    QWidget* const viewWidget = fixture.page->calendarViewWidget();
    QVERIFY(viewWidget);
    QVERIFY(setCalendarWidgetMonth(viewWidget, QDate(2026, 10, 1)));
    QTRY_VERIFY_WITH_TIMEOUT(
        fixture.page->eventModel()->isMonthLoaded(2026, 10), 5000);
    QTRY_COMPARE(viewWidget->property(
        "calendarEventButtonCount").toInt(), 3);
    QCOMPARE(viewWidget->property(
        "calendarShownMonth").toDate(), QDate(2026, 10, 1));
    QCOMPARE(callbackState.displayedMonth, QDate(2026, 10, 1));
    QCOMPARE(viewWidget->property(
        "calendarDayCellCount").toInt(), 35);
    QCOMPARE(viewWidget->findChildren<QWidget*>(
        QStringLiteral("calendarWidgetDayCell")).size(), 35);

    QPushButton* dateButton = nullptr;
    for (QPushButton* const button : viewWidget->findChildren<QPushButton*>(
             QStringLiteral("calendarWidgetDateButton")))
    {
        if (button->property("calendarDate").toDate() == QDate(2026, 10, 5))
        {
            dateButton = button;
            break;
        }
    }
    QVERIFY(dateButton);
    QCOMPARE(dateButton->text(), QStringLiteral("5"));
    QCOMPARE(dateButton->accessibleName(),
             QLocale().toString(QDate(2026, 10, 5), QLocale::LongFormat));
    QVERIFY(dateButton->focusPolicy() != Qt::NoFocus);
    QAccessibleInterface* const dateAccessible =
        QAccessible::queryAccessibleInterface(dateButton);
    QVERIFY(dateAccessible);
    QCOMPARE(dateAccessible->role(), QAccessible::Button);
    QCOMPARE(dateAccessible->text(QAccessible::Name),
             dateButton->accessibleName());
    dateButton->setFocus();
    QTest::keyClick(dateButton, Qt::Key_Space);
    QCOMPARE(callbackState.dayActivationCount, 1);
    QCOMPARE(callbackState.activatedDate, QDate(2026, 10, 5));

    QWidget* emptyCell = nullptr;
    for (QWidget* const cell : viewWidget->findChildren<QWidget*>(
             QStringLiteral("calendarWidgetDayCell")))
    {
        if (cell->property("calendarDate").toDate() == QDate(2026, 10, 7))
        {
            emptyCell = cell;
            break;
        }
    }
    QVERIFY(emptyCell);
    QTest::mouseClick(emptyCell, Qt::LeftButton);
    QCOMPARE(callbackState.dayActivationCount, 2);
    QCOMPARE(callbackState.activatedDate, QDate(2026, 10, 7));

    const QVariantList events = fixture.page->eventModel()->eventsForDate(
        2026, 10, 6).toList();
    QCOMPARE(events.size(), 1);
    const QVariantMap event = events.first().toMap();
    const QString eventId = event.value(QStringLiteral("id")).toString();
    const QString eventTitle = event.value(QStringLiteral("title")).toString();
    QVERIFY(!eventId.isEmpty());
    QPushButton* eventButton = nullptr;
    for (QPushButton* const button : viewWidget->findChildren<QPushButton*>(
             QStringLiteral("calendarWidgetEventButton")))
    {
        if (button->property("calendarEventId").toString() == eventId)
        {
            eventButton = button;
            break;
        }
    }
    QVERIFY(eventButton);
    QCOMPARE(eventButton->text(), eventTitle);
    QVERIFY(eventButton->accessibleName().contains(eventTitle));
    QVERIFY(eventButton->focusPolicy() != Qt::NoFocus);
    QAccessibleInterface* const eventAccessible =
        QAccessible::queryAccessibleInterface(eventButton);
    QVERIFY(eventAccessible);
    QCOMPARE(eventAccessible->role(), QAccessible::Button);
    QCOMPARE(eventAccessible->text(QAccessible::Name),
             eventButton->accessibleName());
    eventButton->setFocus();
    QTest::keyClick(eventButton, Qt::Key_Space);
    QCOMPARE(callbackState.eventActivationCount, 1);
    QCOMPARE(callbackState.activatedEventId, eventId);

    QToolButton* const nextMonth = viewWidget->findChild<QToolButton*>(
        QStringLiteral("calendarWidgetNextMonth"));
    QVERIFY(nextMonth);
    QVERIFY(!nextMonth->accessibleName().isEmpty());
    nextMonth->setFocus();
    QTest::keyClick(nextMonth, Qt::Key_Space);
    QCOMPARE(viewWidget->property(
        "calendarShownMonth").toDate(), QDate(2026, 11, 1));
    QCOMPARE(callbackState.displayedMonth, QDate(2026, 11, 1));
    QTRY_VERIFY_WITH_TIMEOUT(
        fixture.page->eventModel()->isMonthLoaded(2026, 11), 5000);
    QToolButton* const previousMonth = viewWidget->findChild<QToolButton*>(
        QStringLiteral("calendarWidgetPreviousMonth"));
    QVERIFY(previousMonth);
    QTest::mouseClick(previousMonth, Qt::LeftButton);
    QCOMPARE(viewWidget->property(
        "calendarShownMonth").toDate(), QDate(2026, 10, 1));
    QTRY_VERIFY_WITH_TIMEOUT(
        fixture.page->eventModel()->isMonthLoaded(2026, 10), 5000);

    QVERIFY(fixture.host->activatePage(QStringLiteral("other")));
    QApplication::processEvents();
    QVERIFY(!fixture.page->calendarViewWidget());
    QCOMPARE(fixture.page->runtimeMetrics().calendarViewCount, 0);
}

void CalendarPageTests::calendarWidgetGridSupportsEmptySparseAndBusyMonths()
{
    CalendarPageFixture fixture;
    fixture.calendarViewFactory = [](const Ui::CalendarViewRequest& request)
    {
        return Ui::createWidgetCalendarView(request);
    };
    QVERIFY2(fixture.initialize(true, false, true, false),
             qPrintable(fixture.initializationError));

    {
        WorkspaceStore store;
        QVERIFY(store.openExisting(fixture.path));
        Platform::ApplicationServicesCalendarEventSavePort savePort(store);
        for (int index = 0; index < 12; ++index)
        {
            const int minute = index * 5;
            const QTime startTime(10 + minute / 60, minute % 60);
            const QString title = index == 0
                ? QStringLiteral(
                    "A deliberately long workshop event title for hover scrolling")
                : QStringLiteral("Busy day event %1").arg(index + 1);
            const auto saved = Application::CalendarEventSaveUseCase::execute(
                savePort,
                Application::CalendarEventSaveInput{
                    .title = title.toUtf8().toStdString(),
                    .startDate = "2026-10-06",
                    .endDate = "2026-10-06",
                    .startTime = startTime.toString(
                        QStringLiteral("HH:mm")).toStdString(),
                    .endTime = startTime.addSecs(30 * 60)
                        .toString(QStringLiteral("HH:mm")).toStdString(),
                    .allDay = false,
                    .eventType = "Workshop"
                });
            QVERIFY2(saved.hasValue(),
                     "could not seed busy-day calendar events");
        }
    }

    QVERIFY(fixture.host->activatePage(QStringLiteral("calendar")));
    QWidget* const viewWidget = fixture.page->calendarViewWidget();
    QVERIFY(viewWidget);

    QVERIFY(setCalendarWidgetMonth(viewWidget, QDate(2026, 8, 1)));
    QTRY_VERIFY_WITH_TIMEOUT(
        fixture.page->eventModel()->isMonthLoaded(2026, 8), 5000);
    QCOMPARE(viewWidget->property("calendarShownMonth").toDate(),
             QDate(2026, 8, 1));
    QCOMPARE(viewWidget->property("calendarDayCellCount").toInt(), 42);
    QCOMPARE(viewWidget->findChildren<QWidget*>(
        QStringLiteral("calendarWidgetDayCell")).size(), 42);
    QCOMPARE(viewWidget->findChildren<QPushButton*>(
        QStringLiteral("calendarWidgetDateButton")).size(), 42);
    QCOMPARE(viewWidget->property("calendarEventButtonCount").toInt(), 0);

    QVERIFY(setCalendarWidgetMonth(viewWidget, QDate(2026, 10, 1)));
    QTRY_VERIFY_WITH_TIMEOUT(
        fixture.page->eventModel()->isMonthLoaded(2026, 10), 5000);
    QTRY_COMPARE(viewWidget->property(
        "calendarEventButtonCount").toInt(), 15);
    QCOMPARE(viewWidget->property("calendarDayCellCount").toInt(), 35);
    QCOMPARE(viewWidget->findChildren<QWidget*>(
        QStringLiteral("calendarWidgetAcademicWeek")).size(), 10);

    int busyDayButtonCount = 0;
    QPushButton* longEventButton = nullptr;
    QPushButton* holidayButton = nullptr;
    for (QPushButton* const button : viewWidget->findChildren<QPushButton*>(
             QStringLiteral("calendarWidgetEventButton")))
    {
        const QDate date = button->property("calendarDate").toDate();
        if (date == QDate(2026, 10, 6))
        {
            ++busyDayButtonCount;
            if (button->text().startsWith(QStringLiteral("A deliberately long")))
            {
                longEventButton = button;
            }
        }
        if (button->property("calendarEventId").toString()
            == fixture.page->eventModel()->eventsForDate(2026, 10, 26)
                   .toList().first().toMap().value(QStringLiteral("id")).toString())
        {
            holidayButton = button;
        }
    }
    QCOMPARE(busyDayButtonCount, 13);
    QVERIFY(longEventButton);
    QVERIFY(holidayButton);
    QCOMPARE(holidayButton->property(
        "calendarEventBackground").value<QColor>(),
        QColor(QStringLiteral("#7a5f9e")));
    QCOMPARE(holidayButton->toolTip(), holidayButton->text());

    QScrollArea* busyDayScroll = nullptr;
    for (QScrollArea* const scroll : viewWidget->findChildren<QScrollArea*>(
             QStringLiteral("calendarWidgetEventScrollArea")))
    {
        if (scroll->property("calendarDate").toDate()
            == QDate(2026, 10, 6))
        {
            busyDayScroll = scroll;
            break;
        }
    }
    QVERIFY(busyDayScroll);
    QApplication::processEvents();
    QVERIFY(busyDayScroll->verticalScrollBar()->maximum() > 0);

    QWidget* holidayCell = nullptr;
    QWidget* weekendCell = nullptr;
    for (QWidget* const cell : viewWidget->findChildren<QWidget*>(
             QStringLiteral("calendarWidgetDayCell")))
    {
        const QDate date = cell->property("calendarDate").toDate();
        if (date == QDate(2026, 10, 26))
        {
            holidayCell = cell;
        }
        else if (date == QDate(2026, 10, 31))
        {
            weekendCell = cell;
        }
    }
    QVERIFY(holidayCell);
    QVERIFY(weekendCell);
    QVERIFY(holidayCell->property("calendarHoliday").toBool());
    QCOMPARE(holidayCell->property("calendarBackground").value<QColor>(),
             QColor(QStringLiteral("#f5d7d2")));
    QVERIFY(weekendCell->property("calendarWeekend").toBool());
    QCOMPARE(weekendCell->property("calendarBackground").value<QColor>(),
             QColor(QStringLiteral("#e8edf1")));

    const QImage beforeHover = longEventButton->grab().toImage();
    QVERIFY(!beforeHover.isNull());
    QTest::mouseMove(longEventButton,
        QPoint(longEventButton->width() / 2, longEventButton->height() / 2));
    QTest::qWait(450);
    const QImage afterHover = longEventButton->grab().toImage();
    QVERIFY(!afterHover.isNull());
    int changedPixels = 0;
    for (int y = 0; y < beforeHover.height(); ++y)
    {
        for (int x = 0; x < beforeHover.width(); ++x)
        {
            if (beforeHover.pixel(x, y) != afterHover.pixel(x, y))
            {
                ++changedPixels;
            }
        }
    }
    QVERIFY(changedPixels > 0);

    QVERIFY(fixture.host->activatePage(QStringLiteral("other")));
    QApplication::processEvents();
    QVERIFY(!fixture.page->calendarViewWidget());
    QCOMPARE(fixture.page->runtimeMetrics().calendarViewCount, 0);
}

void CalendarPageTests::
injectedCalendarViewFailureReleasesProvidersAndRetriesWithState()
{
    CalendarPageFixture fixture;
    fixture.calendarViewFactory = [&fixture](
        const Ui::CalendarViewRequest& request)
    {
        ++fixture.calendarViewState.factoryCalls;
        fixture.calendarViewState.lastRequestHasBorrowedProviders =
            request.parent && request.eventModel && request.academicProvider
            && request.callbacks.dayActivated
            && request.callbacks.eventActivated
            && request.callbacks.displayedMonthChanged;
        fixture.calendarViewState.lastRequestedMonth = request.shownMonth;

        if (fixture.calendarViewState.factoryCalls == 1)
        {
            // A partial view whose QML root failed to load must be discarded.
            return Ui::CalendarViewCreationResult{
                std::make_unique<FakeCalendarView>(
                    request, fixture.calendarViewState, false),
                QString{}
            };
        }

        fixture.calendarViewState.displayedMonthChanged =
            request.callbacks.displayedMonthChanged;
        return Ui::CalendarViewCreationResult{
            std::make_unique<FakeCalendarView>(
                request, fixture.calendarViewState, true),
            QString{}
        };
    };

    QVERIFY(fixture.initialize(false));
    QVERIFY(fixture.lazyBeforeActivation);
    QCOMPARE(fixture.calendarViewState.factoryCalls, 1);
    QCOMPARE(fixture.calendarViewState.destructionCount, 1);
    QVERIFY(fixture.calendarViewState.providersAliveWhenViewDestroyed);
    QVERIFY(fixture.calendarViewState.lastRequestHasBorrowedProviders);
    QVERIFY(!fixture.page->calendarViewWidget());
    QVERIFY(!fixture.page->eventModel());
    QVERIFY(!fixture.page->eventCache());
    QCOMPARE(fixture.page->runtimeMetrics().calendarViewCount, 0);
    QCOMPARE(fixture.page->runtimeMetrics().liveHeavySessionCount, 0);
    auto* const status = fixture.page->findChild<QLabel*>(
        QStringLiteral("calendarPageStatus"));
    QVERIFY(status && !status->isHidden());
    QVERIFY(status->text().contains(QStringLiteral("could not be loaded")));

    QVERIFY(fixture.host->activatePage(QStringLiteral("other")));
    QVERIFY(fixture.host->activatePage(QStringLiteral("calendar")));
    QCOMPARE(fixture.calendarViewState.factoryCalls, 2);
    QVERIFY(fixture.calendarViewState.lastRequestHasBorrowedProviders);
    QCOMPARE(fixture.calendarViewState.lastRequestedMonth,
             QDate::currentDate().year() < 2026
                 ? QDate(2026, 1, 1)
                 : QDate(QDate::currentDate().year(),
                         QDate::currentDate().month(), 1));
    QVERIFY(fixture.page->calendarViewWidget());
    QCOMPARE(fixture.page->runtimeMetrics().calendarRootObjectCount, 1);
    QVERIFY(fixture.page->eventModel());
    QVERIFY(fixture.page->eventCache());
    QCOMPARE(fixture.page->runtimeMetrics().liveHeavySessionCount, 1);
    QCOMPARE(fixture.calendarViewState.paletteApplyCount, 1);
    QVERIFY(fixture.calendarViewState.eventTypeColorApplyCount > 0);
    QVERIFY(fixture.calendarViewState.displayedMonthChanged);

    fixture.calendarViewState.displayedMonthChanged(2026, 10);
    auto* const scope = fixture.page->findChild<QComboBox*>(
        QStringLiteral("calendarUpcomingScope"));
    auto* const holidayFilter = fixture.page->findChild<QPushButton*>(
        QStringLiteral("calendarEventTypeFilter1"));
    QVERIFY(scope && holidayFilter);
    scope->setCurrentIndex(2);
    holidayFilter->click();
    QVERIFY(!holidayFilter->isChecked());

    QPointer<CalendarEventModel> model(fixture.page->eventModel());
    QPointer<AcademicCalendarProvider> provider(
        fixture.page->findChild<AcademicCalendarProvider*>());
    QVERIFY(model && provider);
    QVERIFY(fixture.host->activatePage(QStringLiteral("other")));
    QCOMPARE(fixture.calendarViewState.destructionCount, 2);
    QVERIFY(fixture.calendarViewState.providersAliveWhenViewDestroyed);
    QVERIFY(model.isNull());
    QVERIFY(provider.isNull());
    QVERIFY(!fixture.page->calendarViewWidget());
    QVERIFY(!fixture.page->eventModel());
    QVERIFY(!fixture.page->eventCache());
    QCOMPARE(fixture.page->runtimeMetrics().liveHeavySessionCount, 0);

    QVERIFY(fixture.host->activatePage(QStringLiteral("calendar")));
    QCOMPARE(fixture.calendarViewState.factoryCalls, 3);
    QVERIFY(fixture.calendarViewState.lastRequestHasBorrowedProviders);
    QCOMPARE(fixture.calendarViewState.lastRequestedMonth, QDate(2026, 10, 1));
    QVERIFY(fixture.page->calendarViewWidget());
    QCOMPARE(fixture.page->findChild<QComboBox*>(
                 QStringLiteral("calendarUpcomingScope"))->currentIndex(), 2);
    QVERIFY(!fixture.page->findChild<QPushButton*>(
        QStringLiteral("calendarEventTypeFilter1"))->isChecked());
    QCOMPARE(fixture.page->runtimeMetrics().liveHeavySessionCount, 1);
}

void CalendarPageTests::eventTypeFilterAffectsUpcomingButNotMonthGrid()
{
    CalendarPageFixture fixture;
    fixture.holidayColorOverride = "#1255aa";
    QVERIFY(fixture.initialize(true, false, true));

    QQuickWidget* const calendarView = fixture.page->findChild<QQuickWidget*>(
        QStringLiteral("nextCalendarMonthGrid"));
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

void CalendarPageTests::calendarImportUsesStreamingTypedPathApplyAndRefreshesOnlyOnWrites()
{
    QFile fixtureFile(QStringLiteral(CLASSMNGR_SOURCE_DIR)
        + QStringLiteral(
            "/tests/fixtures/imports/calendar_import_streaming_2026_large.xlsx"));
    QVERIFY(fixtureFile.open(QIODevice::ReadOnly));
    const QByteArray workbook = fixtureFile.readAll();
    QCOMPARE(workbook.size(), qsizetype(246017));
    QVERIFY(workbook.size() > 128 * 1024);

    CalendarImportTestTransport server(workbook);
    QTemporaryDir stagedFiles;
    QVERIFY(stagedFiles.isValid());

    CalendarPageFixture fixture;
    const auto archiveReader =
        std::make_shared<RecordingArchiveReadPort>(makeZipArchiveReadPort());
    fixture.calendarArchiveReadPort = archiveReader;
    fixture.calendarImportSourceUrl = server.url();
    fixture.calendarImportTemporaryFileTemplate = stagedFiles.filePath(
        QStringLiteral("calendar-import-XXXXXX.xlsx"));
    fixture.calendarImportNetworkAccessManagerFactory =
        server.networkAccessManagerFactory();
    QVERIFY2(fixture.initialize(true, false, false, true, true),
             qPrintable(fixture.initializationError));
    QTRY_VERIFY_WITH_TIMEOUT(
        !fixture.page->runtimeMetrics().cacheLoading, 5000);

    const auto existing = Platform::saveCalendarEventAtPath(
        fixture.path,
        Application::CalendarEventSaveInput{
            .title = "Red Day",
            .startDate = "2026-07-04",
            .endDate = "2026-07-04",
            .allDay = true,
            .eventType = "Holiday",
            .timeStatus = Domain::CalendarEventTimeStatus::Timed
        });
    QVERIFY(existing);

    auto* const service = fixture.page->findChild<Ui::NextCalendarEventImportService*>(
        QStringLiteral("nextCalendarEventImportService"));
    QVERIFY(service);
    Ui::CalendarPreferencesActions actions;
    actions.calendarImportService = service;
    Ui::CalendarPreferencesDialog preferences(
        fixture.path, nullptr, std::move(actions));
    preferences.show();
    auto* const importButton = preferences.findChild<QPushButton*>(
        QStringLiteral("calendarImportEvents"));
    auto* const importStatus = preferences.findChild<QLabel*>(
        QStringLiteral("calendarImportStatus"));
    QVERIFY(importButton);
    QVERIFY(importStatus);

    QSignalSpy importedSpy(service,
        &Ui::NextCalendarEventImportService::importFinished);
    QSignalSpy failedSpy(service,
        &Ui::NextCalendarEventImportService::importFailed);
    QSignalSpy cacheChangedSpy(fixture.page->eventCache(),
        &CalendarEventCache::cacheChanged);
    QVERIFY(importedSpy.isValid());
    QVERIFY(failedSpy.isValid());

    CalendarImportCheckpointMessageCapture checkpointCapture;
    QTest::mouseClick(importButton, Qt::LeftButton);
    QVERIFY(service->isImporting());
    QCOMPARE(importButton->isEnabled(), false);
    QCOMPARE(importStatus->text(), QStringLiteral("Importing events..."));
    QTRY_COMPARE_WITH_TIMEOUT(importedSpy.size(), 1, 5000);
    QCOMPARE(failedSpy.size(), 0);
    QCOMPARE(importedSpy.first().at(0).toInt(), 3);
    QCOMPARE(importedSpy.first().at(1).toInt(), 2);
    QCOMPARE(archiveReader->byteOpenCount, 0);
    QVERIFY(archiveReader->deviceOpenCount >= 1);
    QCOMPARE(importStatus->text(),
        QStringLiteral("Imported 3 event(s). Skipped 2 existing or ignored item(s)."));
    QCOMPARE(importButton->isEnabled(), true);
    QTRY_VERIFY_WITH_TIMEOUT(!fixture.page->runtimeMetrics().cacheLoading, 5000);
    QVERIFY(cacheChangedSpy.size() >= 1);

    QJsonObject terminalImportCheckpoint;
    QJsonObject settledImportCheckpoint;
    const QString checkpointPrefix = QStringLiteral(
        "ClassMngrNext calendar import checkpoint=");
    for (const QString& message : checkpointCapture.messages)
    {
        const QJsonDocument document = QJsonDocument::fromJson(
            message.mid(checkpointPrefix.size()).toUtf8());
        if (!document.isObject())
        {
            continue;
        }
        const QJsonObject record = document.object();
        const QString observation = record.value(
            QStringLiteral("observation")).toString();
        if (observation == QStringLiteral("terminal"))
        {
            terminalImportCheckpoint = record;
        }
        else if (observation == QStringLiteral("cache-settled"))
        {
            settledImportCheckpoint = record;
        }
    }
    QVERIFY(!terminalImportCheckpoint.isEmpty());
    QVERIFY(!settledImportCheckpoint.isEmpty());
    const QJsonObject terminalRuntimeMetrics = terminalImportCheckpoint
        .value(QStringLiteral("calendar_runtime_metrics")).toObject();
    const QJsonObject terminalImportMetrics = terminalRuntimeMetrics
        .value(QStringLiteral("calendar_import")).toObject();
    QCOMPARE(terminalImportMetrics.value(
        QStringLiteral("terminal_status")).toString(), QStringLiteral("completed"));
    QCOMPARE(terminalImportMetrics.value(
        QStringLiteral("imported_event_count")).toInt(), 3);
    QCOMPARE(terminalRuntimeMetrics.value(
        QStringLiteral("calendar_import_cache_refresh_count")).toInt(), 1);
    const QJsonObject settledRuntimeMetrics = settledImportCheckpoint
        .value(QStringLiteral("calendar_runtime_metrics")).toObject();
    QCOMPARE(settledRuntimeMetrics.value(
        QStringLiteral("cache_loading")).toBool(), false);
    QCOMPARE(settledRuntimeMetrics.value(
        QStringLiteral("upcoming_range_loaded")).toBool(), true);
    QCOMPARE(settledRuntimeMetrics.value(
        QStringLiteral("calendar_import_cache_refresh_count")).toInt(), 1);

    const auto firstMetrics = service->runtimeMetrics();
    QVERIFY(firstMetrics.usedDeviceParser);
    QVERIFY(firstMetrics.streamedWorksheetCellCount > 0);
    QCOMPARE(firstMetrics.materializedWorksheetCellCount, 0);
    QCOMPARE(firstMetrics.parsedEventCount, 4);
    QCOMPARE(firstMetrics.typedCandidateCountAtApply, 4);
    QCOMPARE(firstMetrics.legacyEventCountAtApply, 0);
    QVERIFY(firstMetrics.stagedWorkbookReleasedBeforeApply);
    QVERIFY(firstMetrics.stagedWorkbookRemovedAtTerminal);
    QCOMPARE(firstMetrics.downloadedBytes, workbook.size());
    QCOMPARE(firstMetrics.transferChunkCount,
        static_cast<int>((workbook.size() + (64 * 1024 - 1)) / (64 * 1024)));
    QCOMPARE(firstMetrics.transferChunkCount, 4);
    QVERIFY(firstMetrics.transferChunkCount >= 3);
    QCOMPARE(firstMetrics.largestTransferChunkBytes, qint64(64 * 1024));
    QVERIFY(QDir(stagedFiles.path()).entryList(
        QDir::Files | QDir::NoDotAndDotDot).isEmpty());

    const Ui::CalendarPageRuntimeMetrics pageMetrics =
        fixture.page->runtimeMetrics();
    QVERIFY(pageMetrics.calendarImportMetricsAvailable);
    QCOMPARE(pageMetrics.calendarImportMetrics.terminalStatus,
             QStringLiteral("completed"));
    QCOMPARE(pageMetrics.calendarImportMetrics.importedEventCount, 3);
    QCOMPARE(pageMetrics.calendarImportMetrics.skippedEventCount, 2);
    QCOMPARE(pageMetrics.calendarImportCacheRefreshCount, 1);
    QCOMPARE(pageMetrics.calendarImportMetrics.candidateRowsSha256.size(), 64);

    preferences.close();
    QVERIFY(fixture.host->activatePage(QStringLiteral("other")));
    QApplication::processEvents();
    QJsonObject suspendedCalendarMetrics;
    for (const QJsonObject& record : fixture.lifecycleRecords)
    {
        if (record.value(QStringLiteral("event")).toString()
                == QStringLiteral("page-lifecycle")
            && record.value(QStringLiteral("page_key")).toString()
                == QStringLiteral("calendar")
            && record.value(QStringLiteral("state")).toString()
                == QStringLiteral("suspended"))
        {
            suspendedCalendarMetrics = record.value(QStringLiteral("page"))
                .toObject().value(QStringLiteral("calendar_runtime_metrics"))
                .toObject();
        }
    }
    QVERIFY(!suspendedCalendarMetrics.isEmpty());
    const QJsonObject suspendedImport = suspendedCalendarMetrics
        .value(QStringLiteral("calendar_import")).toObject();
    QCOMPARE(suspendedImport.value(QStringLiteral("downloaded_bytes")).toInt(),
             static_cast<int>(workbook.size()));
    QCOMPARE(suspendedImport.value(QStringLiteral("parsed_event_count")).toInt(), 4);
    QCOMPARE(suspendedImport.value(QStringLiteral("typed_candidate_count_at_apply")).toInt(), 4);
    QCOMPARE(suspendedImport.value(QStringLiteral("imported_event_count")).toInt(), 3);
    QCOMPARE(suspendedImport.value(QStringLiteral("skipped_event_count")).toInt(), 2);
    QVERIFY(suspendedImport.value(
        QStringLiteral("staged_workbook_released_before_apply")).toBool());
    QVERIFY(suspendedImport.value(
        QStringLiteral("staged_workbook_removed_at_terminal")).toBool());
    QVERIFY(!suspendedImport.value(
        QStringLiteral("staged_workbook_removed_on_cancel")).toBool());
    QCOMPARE(suspendedCalendarMetrics.value(
        QStringLiteral("calendar_import_cache_refresh_count")).toInt(), 1);
    QVERIFY(fixture.host->activatePage(QStringLiteral("calendar")));
    QTRY_VERIFY_WITH_TIMEOUT(!fixture.page->runtimeMetrics().cacheLoading, 5000);
    int settledCheckpointCount = 0;
    for (const QString& message : checkpointCapture.messages)
    {
        const QJsonDocument document = QJsonDocument::fromJson(
            message.mid(checkpointPrefix.size()).toUtf8());
        if (document.isObject()
            && document.object().value(QStringLiteral("observation")).toString()
                == QStringLiteral("cache-settled"))
        {
            ++settledCheckpointCount;
        }
    }
    QCOMPARE(settledCheckpointCount, 2);

    cacheChangedSpy.clear();
    QTest::mouseClick(importButton, Qt::LeftButton);
    QTRY_COMPARE_WITH_TIMEOUT(importedSpy.size(), 2, 5000);
    QCOMPARE(failedSpy.size(), 0);
    QCOMPARE(importedSpy.last().at(0).toInt(), 0);
    QCOMPARE(importedSpy.last().at(1).toInt(), 5);
    QCOMPARE(importStatus->text(),
        QStringLiteral("Imported 0 event(s). Skipped 5 existing or ignored item(s)."));
    QCOMPARE(cacheChangedSpy.size(), 0);
    QCOMPARE(server.requestCount(), 2);

    WorkspaceStore verifyStore;
    QVERIFY(verifyStore.openExisting(fixture.path));
    const auto persisted = verifyStore.calendarEventRepository()
        ->loadCalendarEventsInRange(QDate(2026, 7, 1), QDate(2026, 7, 31));
    QVERIFY(persisted);
    QCOMPARE(persisted->size(), 4);
    QCOMPARE(persisted->at(0).title, QStringLiteral("New Semester"));
    QCOMPARE(persisted->at(1).title, QStringLiteral("Board Meeting"));
    QCOMPARE(persisted->at(2).title, QStringLiteral("Red Day"));
    QCOMPARE(persisted->at(3).title, QStringLiteral("DYB Workshop"));
}

void CalendarPageTests::calendarImportUsesEnvironmentSourceUrlOverride()
{
    struct EnvironmentRestore final
    {
        bool wasSet = false;
        QByteArray previousValue;

        ~EnvironmentRestore()
        {
            if (wasSet)
            {
                qputenv("CLASSMNGR_STARTUP_CALENDAR_IMPORT_URL", previousValue);
            }
            else
            {
                qunsetenv("CLASSMNGR_STARTUP_CALENDAR_IMPORT_URL");
            }
        }
    };
    const EnvironmentRestore restore{
        qEnvironmentVariableIsSet("CLASSMNGR_STARTUP_CALENDAR_IMPORT_URL"),
        qgetenv("CLASSMNGR_STARTUP_CALENDAR_IMPORT_URL")
    };
    const QByteArray overrideUrl = QByteArrayLiteral(
        "https://configured.example/calendar/import.xlsx?batch=phase7-c34");
    QVERIFY(qputenv("CLASSMNGR_STARTUP_CALENDAR_IMPORT_URL", overrideUrl));

    QFile fixtureFile(QStringLiteral(CLASSMNGR_SOURCE_DIR)
        + QStringLiteral("/tests/fixtures/imports/calendar_import_parity_2026.xlsx"));
    QVERIFY(fixtureFile.open(QIODevice::ReadOnly));
    const QByteArray workbook = fixtureFile.readAll();
    QVERIFY(!workbook.isEmpty());
    CalendarImportTestTransport transport(workbook);
    QTemporaryDir stagedFiles;
    QVERIFY(stagedFiles.isValid());

    CalendarPageFixture fixture;
    fixture.calendarImportTemporaryFileTemplate = stagedFiles.filePath(
        QStringLiteral("calendar-import-XXXXXX.xlsx"));
    fixture.calendarImportNetworkAccessManagerFactory =
        transport.networkAccessManagerFactory();
    QVERIFY2(fixture.initialize(true), qPrintable(fixture.initializationError));
    auto* const service = fixture.page->findChild<
        Ui::NextCalendarEventImportService*>(
            QStringLiteral("nextCalendarEventImportService"));
    QVERIFY(service);
    Ui::CalendarPreferencesActions actions;
    actions.calendarImportService = service;
    Ui::CalendarPreferencesDialog preferences(
        fixture.path, nullptr, std::move(actions));
    preferences.show();
    auto* const importButton = preferences.findChild<QPushButton*>(
        QStringLiteral("calendarImportEvents"));
    auto* const importStatus = preferences.findChild<QLabel*>(
        QStringLiteral("calendarImportStatus"));
    QVERIFY(importButton);
    QVERIFY(importStatus);
    QSignalSpy importedSpy(service,
        &Ui::NextCalendarEventImportService::importFinished);
    QVERIFY(importedSpy.isValid());

    QTest::mouseClick(importButton, Qt::LeftButton);
    QTRY_COMPARE_WITH_TIMEOUT(importedSpy.size(), 1, 5000);
    QCOMPARE(transport.requestCount(), 1);
    QCOMPARE(transport.lastRequestedUrl(), QString::fromUtf8(overrideUrl));
    QCOMPARE(importedSpy.first().at(0).toInt(), 4);
    QCOMPARE(importedSpy.first().at(1).toInt(), 1);
    QVERIFY(importStatus->text().startsWith(QStringLiteral("Imported 4")));
    QVERIFY(service->runtimeMetrics().stagedWorkbookReleasedBeforeApply);
    QVERIFY(QDir(stagedFiles.path()).entryList(
        QDir::Files | QDir::NoDotAndDotDot).isEmpty());
}

void CalendarPageTests::calendarImportMalformedAndTransportFailuresAreRecoverable()
{
    for (const int failureKind : {0, 1, 2})
    {
        QByteArray responseBody = QByteArrayLiteral("not an XLSX workbook");
        CalendarImportTestTransport server(responseBody);
        if (failureKind == 1)
        {
            server.closeWithoutResponse(true);
        }
        QTemporaryDir stagedFiles;
        QVERIFY(stagedFiles.isValid());

        CalendarPageFixture fixture;
        fixture.calendarImportSourceUrl = server.url();
        fixture.calendarImportTemporaryFileTemplate = failureKind == 2
            ? stagedFiles.filePath(QStringLiteral(
                "missing/calendar-import-XXXXXX.xlsx"))
            : stagedFiles.filePath(QStringLiteral("calendar-import-XXXXXX.xlsx"));
        fixture.calendarImportNetworkAccessManagerFactory =
            server.networkAccessManagerFactory();
        QVERIFY2(fixture.initialize(true), qPrintable(fixture.initializationError));
        auto* const service = fixture.page->findChild<Ui::NextCalendarEventImportService*>(
            QStringLiteral("nextCalendarEventImportService"));
        QVERIFY(service);
        Ui::CalendarPreferencesActions actions;
        actions.calendarImportService = service;
        Ui::CalendarPreferencesDialog preferences(
            fixture.path, nullptr, std::move(actions));
        preferences.show();
        auto* const importButton = preferences.findChild<QPushButton*>(
            QStringLiteral("calendarImportEvents"));
        auto* const importStatus = preferences.findChild<QLabel*>(
            QStringLiteral("calendarImportStatus"));
        QVERIFY(importButton);
        QVERIFY(importStatus);
        QSignalSpy failedSpy(service,
            &Ui::NextCalendarEventImportService::importFailed);
        QVERIFY(failedSpy.isValid());

        QTest::mouseClick(importButton, Qt::LeftButton);
        QTRY_COMPARE_WITH_TIMEOUT(failedSpy.size(), 1, 5000);
        QVERIFY(importStatus->text().startsWith(
            QStringLiteral("Calendar import failed: ")));
        QVERIFY(!service->isImporting());
        QVERIFY(service->runtimeMetrics().stagedWorkbookRemovedAtTerminal);
        if (failureKind == 2)
        {
            QCOMPARE(server.requestCount(), 0);
        }
        QVERIFY(QDir(stagedFiles.path()).entryList(
            QDir::Files | QDir::NoDotAndDotDot).isEmpty());

        WorkspaceStore verifyStore;
        QVERIFY(verifyStore.openExisting(fixture.path));
        const auto persisted = verifyStore.calendarEventRepository()
            ->loadCalendarEventsInRange(QDate(2026, 7, 1), QDate(2026, 7, 31));
        QVERIFY(persisted);
        QVERIFY(persisted->isEmpty());
    }
}

void CalendarPageTests::calendarImportSuspensionAndPathChangeCancelBeforeApply()
{
    QFile fixtureFile(QStringLiteral(CLASSMNGR_SOURCE_DIR)
        + QStringLiteral("/tests/fixtures/imports/calendar_import_parity_2026.xlsx"));
    QVERIFY(fixtureFile.open(QIODevice::ReadOnly));
    const QByteArray workbook = fixtureFile.readAll();
    QVERIFY(!workbook.isEmpty());

    {
        CalendarImportTestTransport server(workbook);
        server.holdResponses(true);
        QTemporaryDir stagedFiles;
        QVERIFY(stagedFiles.isValid());
        CalendarPageFixture fixture;
        fixture.calendarImportSourceUrl = server.url();
        fixture.calendarImportTemporaryFileTemplate = stagedFiles.filePath(
            QStringLiteral("calendar-import-XXXXXX.xlsx"));
        fixture.calendarImportNetworkAccessManagerFactory =
            server.networkAccessManagerFactory();
        QVERIFY2(fixture.initialize(true), qPrintable(fixture.initializationError));
        auto* const service = fixture.page->findChild<Ui::NextCalendarEventImportService*>(
            QStringLiteral("nextCalendarEventImportService"));
        QVERIFY(service);
        Ui::CalendarPreferencesActions actions;
        actions.calendarImportService = service;
        Ui::CalendarPreferencesDialog preferences(
            fixture.path, nullptr, std::move(actions));
        preferences.show();
        auto* const importButton = preferences.findChild<QPushButton*>(
            QStringLiteral("calendarImportEvents"));
        QVERIFY(importButton);
        QSignalSpy canceledSpy(service,
            &Ui::NextCalendarEventImportService::importCanceled);
        QTest::mouseClick(importButton, Qt::LeftButton);
        QTRY_COMPARE_WITH_TIMEOUT(server.requestCount(), 1, 5000);
        QCOMPARE(QDir(stagedFiles.path()).entryList(
            QDir::Files | QDir::NoDotAndDotDot).size(), 1);
        QVERIFY(fixture.host->activatePage(QStringLiteral("other")));
        QTRY_COMPARE_WITH_TIMEOUT(canceledSpy.size(), 1, 5000);
        QVERIFY(service->runtimeMetrics().stagedWorkbookRemovedOnCancel);
        QVERIFY(QDir(stagedFiles.path()).entryList(
            QDir::Files | QDir::NoDotAndDotDot).isEmpty());
        WorkspaceStore verifyStore;
        QVERIFY(verifyStore.openExisting(fixture.path));
        const auto persisted = verifyStore.calendarEventRepository()
            ->loadCalendarEventsInRange(QDate(2026, 7, 1), QDate(2026, 7, 31));
        QVERIFY(persisted);
        QVERIFY(persisted->isEmpty());
    }

    {
        CalendarImportTestTransport server(workbook);
        server.holdResponses(true);
        QTemporaryDir stagedFiles;
        QTemporaryDir otherWorkspaceDirectory;
        QVERIFY(stagedFiles.isValid());
        QVERIFY(otherWorkspaceDirectory.isValid());
        CalendarPageFixture fixture;
        fixture.calendarImportSourceUrl = server.url();
        fixture.calendarImportTemporaryFileTemplate = stagedFiles.filePath(
            QStringLiteral("calendar-import-XXXXXX.xlsx"));
        fixture.calendarImportNetworkAccessManagerFactory =
            server.networkAccessManagerFactory();
        QVERIFY2(fixture.initialize(true), qPrintable(fixture.initializationError));
        const QString otherPath = otherWorkspaceDirectory.filePath(
            QStringLiteral("other.db"));
        {
            WorkspaceStore otherStore;
            QVERIFY(otherStore.open(otherPath));
        }
        auto* const service = fixture.page->findChild<Ui::NextCalendarEventImportService*>(
            QStringLiteral("nextCalendarEventImportService"));
        QVERIFY(service);
        Ui::CalendarPreferencesActions actions;
        actions.calendarImportService = service;
        Ui::CalendarPreferencesDialog preferences(
            fixture.path, nullptr, std::move(actions));
        preferences.show();
        auto* const importButton = preferences.findChild<QPushButton*>(
            QStringLiteral("calendarImportEvents"));
        QVERIFY(importButton);
        QSignalSpy canceledSpy(service,
            &Ui::NextCalendarEventImportService::importCanceled);
        QTest::mouseClick(importButton, Qt::LeftButton);
        QTRY_COMPARE_WITH_TIMEOUT(server.requestCount(), 1, 5000);
        fixture.path = otherPath;
        server.respondToHeldRequests();
        QTRY_COMPARE_WITH_TIMEOUT(canceledSpy.size(), 1, 5000);
        QVERIFY(service->runtimeMetrics().stagedWorkbookRemovedAtTerminal);
        WorkspaceStore originalStore;
        QVERIFY(originalStore.openExisting(
            fixture.temporaryDirectory.filePath(QStringLiteral("calendar.db"))));
        const auto originalEvents = originalStore.calendarEventRepository()
            ->loadCalendarEventsInRange(QDate(2026, 7, 1), QDate(2026, 7, 31));
        QVERIFY(originalEvents);
        QVERIFY(originalEvents->isEmpty());
        WorkspaceStore otherStore;
        QVERIFY(otherStore.openExisting(otherPath));
        const auto otherEvents = otherStore.calendarEventRepository()
            ->loadCalendarEventsInRange(QDate(2026, 7, 1), QDate(2026, 7, 31));
        QVERIFY(otherEvents);
        QVERIFY(otherEvents->isEmpty());
        QVERIFY(QDir(stagedFiles.path()).entryList(
            QDir::Files | QDir::NoDotAndDotDot).isEmpty());
    }
}

void CalendarPageTests::calendarImportServiceDestructionCleansStagingWithoutWrites()
{
    CalendarImportTestTransport server({});
    server.holdResponses(true);
    QTemporaryDir stagedFiles;
    QTemporaryDir workspaceDirectory;
    QVERIFY(stagedFiles.isValid());
    QVERIFY(workspaceDirectory.isValid());
    const QString databasePath = workspaceDirectory.filePath(
        QStringLiteral("calendar.db"));
    {
        WorkspaceStore store;
        QVERIFY(store.open(databasePath));
    }

    auto* const service = new Ui::NextCalendarEventImportService(
        [databasePath] { return databasePath; },
        [] { return QStringList{}; },
        std::shared_ptr<ArchiveReadPort>(makeZipArchiveReadPort()),
        server.url(),
        stagedFiles.filePath(QStringLiteral("calendar-import-XXXXXX.xlsx")),
        server.networkAccessManagerFactory());
    service->start(databasePath);
    QTRY_COMPARE_WITH_TIMEOUT(server.requestCount(), 1, 5000);
    QCOMPARE(QDir(stagedFiles.path()).entryList(
        QDir::Files | QDir::NoDotAndDotDot).size(), 1);
    delete service;
    QVERIFY(QDir(stagedFiles.path()).entryList(
        QDir::Files | QDir::NoDotAndDotDot).isEmpty());

    WorkspaceStore verifyStore;
    QVERIFY(verifyStore.openExisting(databasePath));
    const auto persisted = verifyStore.calendarEventRepository()
        ->loadCalendarEventsInRange(QDate(2026, 7, 1), QDate(2026, 7, 31));
    QVERIFY(persisted);
    QVERIFY(persisted->isEmpty());
}

void CalendarPageTests::calendarImportQueryFailureIsVisibleAndWritesNothing()
{
    QFile fixtureFile(QStringLiteral(CLASSMNGR_SOURCE_DIR)
        + QStringLiteral("/tests/fixtures/imports/calendar_import_parity_2026.xlsx"));
    QVERIFY(fixtureFile.open(QIODevice::ReadOnly));
    const QByteArray workbook = fixtureFile.readAll();
    QVERIFY(!workbook.isEmpty());
    CalendarImportTestTransport server(workbook);
    QTemporaryDir stagedFiles;
    QVERIFY(stagedFiles.isValid());

    CalendarPageFixture fixture;
    fixture.calendarImportSourceUrl = server.url();
    fixture.calendarImportTemporaryFileTemplate = stagedFiles.filePath(
        QStringLiteral("calendar-import-XXXXXX.xlsx"));
    fixture.calendarImportNetworkAccessManagerFactory =
        server.networkAccessManagerFactory();
    QVERIFY2(fixture.initialize(true), qPrintable(fixture.initializationError));
    QTRY_VERIFY_WITH_TIMEOUT(!fixture.page->runtimeMetrics().cacheLoading, 5000);

    const auto seeded = Platform::saveCalendarEventAtPath(
        fixture.path,
        Application::CalendarEventSaveInput{
            .title = "Retained event",
            .startDate = "2026-07-01",
            .endDate = "2026-07-01",
            .allDay = true,
            .eventType = "Meeting",
            .timeStatus = Domain::CalendarEventTimeStatus::Timed
        });
    QVERIFY(seeded);

    {
        WorkspaceStore schemaStore;
        QVERIFY(schemaStore.openExisting(fixture.path));
        QSqlQuery schema(schemaStore.database());
        QVERIFY(schema.exec(QStringLiteral(
            "ALTER TABLE calendar_events RENAME TO calendar_events_before_import")));
        QVERIFY(schema.exec(QStringLiteral(
            "CREATE TABLE calendar_events (id INTEGER PRIMARY KEY)")));
    }

    auto* const service = fixture.page->findChild<
        Ui::NextCalendarEventImportService*>(
            QStringLiteral("nextCalendarEventImportService"));
    QVERIFY(service);
    Ui::CalendarPreferencesActions actions;
    actions.calendarImportService = service;
    Ui::CalendarPreferencesDialog preferences(
        fixture.path, nullptr, std::move(actions));
    preferences.show();
    auto* const importButton = preferences.findChild<QPushButton*>(
        QStringLiteral("calendarImportEvents"));
    auto* const importStatus = preferences.findChild<QLabel*>(
        QStringLiteral("calendarImportStatus"));
    QVERIFY(importButton);
    QVERIFY(importStatus);
    QSignalSpy failedSpy(service,
        &Ui::NextCalendarEventImportService::importFailed);
    QVERIFY(failedSpy.isValid());

    QTest::mouseClick(importButton, Qt::LeftButton);
    QTRY_COMPARE_WITH_TIMEOUT(failedSpy.size(), 1, 5000);
    QVERIFY(importStatus->text().contains(
        QStringLiteral("Calendar import failed:")));
    QVERIFY2(importStatus->text().contains(
            QStringLiteral("Loading calendar events in range")),
        qPrintable(importStatus->text()));
    QVERIFY(service->runtimeMetrics().stagedWorkbookReleasedBeforeApply);
    QVERIFY(service->runtimeMetrics().stagedWorkbookRemovedAtTerminal);

    WorkspaceStore verifyStore;
    QVERIFY(verifyStore.openExisting(fixture.path));
    QSqlQuery retained(verifyStore.database());
    QVERIFY(retained.exec(QStringLiteral(
        "SELECT COUNT(*) FROM calendar_events_before_import")));
    QVERIFY(retained.next());
    QCOMPARE(retained.value(0).toInt(), 1);
    QSqlQuery current(verifyStore.database());
    QVERIFY(current.exec(QStringLiteral("SELECT COUNT(*) FROM calendar_events")));
    QVERIFY(current.next());
    QCOMPARE(current.value(0).toInt(), 0);
    QVERIFY(QDir(stagedFiles.path()).entryList(
        QDir::Files | QDir::NoDotAndDotDot).isEmpty());
}

void CalendarPageTests::calendarImportBatchSaveFailureRollsBackAllRows()
{
    QFile fixtureFile(QStringLiteral(CLASSMNGR_SOURCE_DIR)
        + QStringLiteral("/tests/fixtures/imports/calendar_import_parity_2026.xlsx"));
    QVERIFY(fixtureFile.open(QIODevice::ReadOnly));
    const QByteArray workbook = fixtureFile.readAll();
    QVERIFY(!workbook.isEmpty());
    CalendarImportTestTransport server(workbook);
    QTemporaryDir stagedFiles;
    QVERIFY(stagedFiles.isValid());

    CalendarPageFixture fixture;
    fixture.calendarImportSourceUrl = server.url();
    fixture.calendarImportTemporaryFileTemplate = stagedFiles.filePath(
        QStringLiteral("calendar-import-XXXXXX.xlsx"));
    fixture.calendarImportNetworkAccessManagerFactory =
        server.networkAccessManagerFactory();
    QVERIFY2(fixture.initialize(true), qPrintable(fixture.initializationError));
    {
        WorkspaceStore triggerStore;
        QVERIFY(triggerStore.openExisting(fixture.path));
        QSqlQuery trigger(triggerStore.database());
        QVERIFY(trigger.exec(QStringLiteral(
            "CREATE TRIGGER reject_calendar_import BEFORE INSERT ON calendar_events "
            "WHEN NEW.title = 'Board Meeting' BEGIN "
            "SELECT RAISE(ABORT, 'test batch failure'); END")));
    }

    auto* const service = fixture.page->findChild<Ui::NextCalendarEventImportService*>(
        QStringLiteral("nextCalendarEventImportService"));
    QVERIFY(service);
    Ui::CalendarPreferencesActions actions;
    actions.calendarImportService = service;
    Ui::CalendarPreferencesDialog preferences(
        fixture.path, nullptr, std::move(actions));
    preferences.show();
    auto* const importButton = preferences.findChild<QPushButton*>(
        QStringLiteral("calendarImportEvents"));
    auto* const importStatus = preferences.findChild<QLabel*>(
        QStringLiteral("calendarImportStatus"));
    QVERIFY(importButton);
    QVERIFY(importStatus);
    QSignalSpy failedSpy(service,
        &Ui::NextCalendarEventImportService::importFailed);
    QVERIFY(failedSpy.isValid());

    QTest::mouseClick(importButton, Qt::LeftButton);
    QTRY_COMPARE_WITH_TIMEOUT(failedSpy.size(), 1, 5000);
    QVERIFY(importStatus->text().contains(QStringLiteral("Calendar import failed:")));
    QVERIFY(service->runtimeMetrics().stagedWorkbookReleasedBeforeApply);
    QVERIFY(service->runtimeMetrics().stagedWorkbookRemovedAtTerminal);
    WorkspaceStore verifyStore;
    QVERIFY(verifyStore.openExisting(fixture.path));
    const auto persisted = verifyStore.calendarEventRepository()
        ->loadCalendarEventsInRange(QDate(2026, 7, 1), QDate(2026, 7, 31));
    QVERIFY(persisted);
    QVERIFY(persisted->isEmpty());
    QVERIFY(QDir(stagedFiles.path()).entryList(
        QDir::Files | QDir::NoDotAndDotDot).isEmpty());
}

void CalendarPageTests::captureEnglishAndKoreanLightAndDark()
{
    QFile importFixture(QStringLiteral(CLASSMNGR_SOURCE_DIR)
        + QStringLiteral("/tests/fixtures/imports/calendar_import_parity_2026.xlsx"));
    QVERIFY(importFixture.open(QIODevice::ReadOnly));
    const QByteArray importWorkbook = importFixture.readAll();
    QVERIFY(!importWorkbook.isEmpty());

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

            CalendarImportTestTransport importServer(importWorkbook);
            importServer.holdResponses(true);
            QTemporaryDir stagedFiles;
            QVERIFY(stagedFiles.isValid());
            CalendarPageFixture fixture;
            fixture.calendarImportSourceUrl = importServer.url();
            fixture.calendarImportTemporaryFileTemplate = stagedFiles.filePath(
                QStringLiteral("calendar-import-XXXXXX.xlsx"));
            fixture.calendarImportNetworkAccessManagerFactory =
                importServer.networkAccessManagerFactory();
            QVERIFY2(fixture.initialize(true, korean, true, true, false, true),
                qPrintable(fixture.initializationError));
            QQuickWidget* const calendarView = fixture.page->findChild<QQuickWidget*>(
                QStringLiteral("nextCalendarMonthGrid"));
            QVERIFY(calendarView);
            QVERIFY(calendarView->rootObject());
            QVERIFY(calendarView->rootObject()->setProperty(
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
            QVERIFY(fixture.page->calendarViewWidget());
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

            auto* const service = fixture.page->findChild<
                Ui::NextCalendarEventImportService*>(
                    QStringLiteral("nextCalendarEventImportService"));
            QVERIFY(service);
            auto* const academicProvider = fixture.page->findChild<
                AcademicCalendarProvider*>();
            QVERIFY(academicProvider);
            Ui::CalendarPreferencesActions preferenceActions;
            preferenceActions.readDisplayPreferences = [](
                const QString& path)
            {
                return Platform::readCalendarEventDisplayPreferencesAtPath(path);
            };
            preferenceActions.saveDisplayPreferences = [](
                const QString& path,
                const Application::CalendarEventDisplayPreferencesSaveRequest& request)
            {
                return Platform::saveCalendarEventDisplayPreferencesAtPath(
                    path, request);
            };
            preferenceActions.deleteAllEvents = [](const QString& path)
            {
                return Platform::deleteAllCalendarEventsAtPath(path);
            };
            preferenceActions.readEventTypeColor = [](
                const QString& path,
                const Domain::CalendarEventType eventType)
            {
                return Platform::readCalendarEventTypeColorAtPath(path, eventType);
            };
            preferenceActions.saveEventTypeColor = [](
                const QString& path,
                const Domain::CalendarEventType eventType,
                const std::string& color)
            {
                return Platform::writeCalendarEventTypeColorAtPath(
                    path, eventType, color);
            };
            preferenceActions.calendarImportService = service;
            Ui::CalendarPreferencesDialog preferences(
                fixture.path, academicProvider, std::move(preferenceActions),
                fixture.page);
            preferences.show();
            QApplication::processEvents();
            auto* const importButton = preferences.findChild<QPushButton*>(
                QStringLiteral("calendarImportEvents"));
            auto* const importStatus = preferences.findChild<QLabel*>(
                QStringLiteral("calendarImportStatus"));
            QVERIFY(importButton);
            QVERIFY(importStatus);

            const QString preferencesImagePath = outputDirectory
                + QLatin1Char('/') + QStringLiteral("calendar-preferences-")
                + mode + QStringLiteral(".png");
            const QImage preferencesImage = preferences.grab().toImage();
            QVERIFY(!preferencesImage.isNull());
            QVERIFY(preferencesImage.save(preferencesImagePath));

            QTest::mouseClick(importButton, Qt::LeftButton);
            QTRY_COMPARE_WITH_TIMEOUT(importServer.requestCount(), 1, 5000);
            QCOMPARE(importButton->isEnabled(), false);
            const QString loadingImagePath = outputDirectory
                + QLatin1Char('/') + QStringLiteral("calendar-import-loading-")
                + mode + QStringLiteral(".png");
            const QImage loadingImage = preferences.grab().toImage();
            QVERIFY(!loadingImage.isNull());
            QVERIFY(loadingImage.save(loadingImagePath));

            importServer.holdResponses(false);
            importServer.respondToHeldRequests();
            QTRY_VERIFY_WITH_TIMEOUT(
                importStatus->text().contains(
                    korean ? QStringLiteral("가져왔습니다")
                           : QStringLiteral("Imported")),
                5000);
            QCOMPARE(importButton->isEnabled(), true);
            const QString successImagePath = outputDirectory
                + QLatin1Char('/') + QStringLiteral("calendar-import-success-")
                + mode + QStringLiteral(".png");
            const QImage successImage = preferences.grab().toImage();
            QVERIFY(!successImage.isNull());
            QVERIFY(successImage.save(successImagePath));

            importServer.setBody(QByteArrayLiteral("malformed calendar workbook"));
            QTest::mouseClick(importButton, Qt::LeftButton);
            QTRY_VERIFY_WITH_TIMEOUT(
                importStatus->text().contains(
                    korean ? QStringLiteral("실패했습니다")
                           : QStringLiteral("Calendar import failed:")),
                5000);
            const QString errorImagePath = outputDirectory
                + QLatin1Char('/') + QStringLiteral("calendar-import-error-")
                + mode + QStringLiteral(".png");
            const QImage errorImage = preferences.grab().toImage();
            QVERIFY(!errorImage.isNull());
            QVERIFY(errorImage.save(errorImagePath));

            importServer.holdResponses(true);
            QTest::mouseClick(importButton, Qt::LeftButton);
            QTRY_COMPARE_WITH_TIMEOUT(importServer.requestCount(), 3, 5000);
            service->cancel();
            QTRY_VERIFY_WITH_TIMEOUT(
                importStatus->text().contains(
                    korean ? QStringLiteral("취소되었습니다")
                           : QStringLiteral("Calendar import was canceled.")),
                5000);
            const QString canceledImagePath = outputDirectory
                + QLatin1Char('/') + QStringLiteral("calendar-import-canceled-")
                + mode + QStringLiteral(".png");
            const QImage canceledImage = preferences.grab().toImage();
            QVERIFY(!canceledImage.isNull());
            QVERIFY(canceledImage.save(canceledImagePath));
            preferences.close();

            QVERIFY(fixture.host->activatePage(QStringLiteral("other")));
            QApplication::processEvents();
            const ProcessMemorySnapshot released = memoryProvider.snapshot();
            const auto releasedMetrics = fixture.page->runtimeMetrics();
            captures.insert(mode, QJsonObject{
                {QStringLiteral("image"), QFileInfo(imagePath).fileName()},
                {QStringLiteral("preferencesImage"),
                 QFileInfo(preferencesImagePath).fileName()},
                {QStringLiteral("importStates"), QJsonObject{
                    {QStringLiteral("loading"),
                     QFileInfo(loadingImagePath).fileName()},
                    {QStringLiteral("success"),
                     QFileInfo(successImagePath).fileName()},
                    {QStringLiteral("error"),
                     QFileInfo(errorImagePath).fileName()},
                    {QStringLiteral("canceled"),
                     QFileInfo(canceledImagePath).fileName()}}},
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

void CalendarPageTests::calendarWidgetViewMatchesQmlCapturesAcrossContexts()
{
    struct EnvironmentRestore final
    {
        QLocale locale;
        QPalette palette;
        ~EnvironmentRestore()
        {
            QLocale::setDefault(locale);
            QApplication::setPalette(palette);
        }
    } restore{QLocale(), QApplication::palette()};

    const QDateTime runStartedAtUtc = QDateTime::currentDateTimeUtc();
    const QString runId = runStartedAtUtc.toString(
        QStringLiteral("yyyyMMdd'T'HHmmsszzz'Z'"))
        + QLatin1Char('-')
        + QUuid::createUuid().toString(QUuid::WithoutBraces);
    const QString outputDirectory = QDir::cleanPath(
        QStringLiteral(CLASSMNGR_CALENDAR_CAPTURE_DIR)
        + QStringLiteral("/calendar-view-parity/") + runId);
    QVERIFY(QDir().mkpath(outputDirectory));

    QJsonArray records;
    QJsonArray fontRegistrationRecords;
    const QDate visibleMonth(2026, 10, 1);
    const auto renderFullTitleReference = [](
        const QString& fullTitle,
        const QFont& titleFont,
        const QColor& toolbarBackground,
        const QColor& toolbarTextColor
        )
    {
        constexpr int referenceWidth = 4096;
        constexpr int referenceHeight = 48;
        QLabel fullTitleReference;
        fullTitleReference.setText(fullTitle);
        fullTitleReference.setFont(titleFont);
        fullTitleReference.setAlignment(Qt::AlignCenter);
        fullTitleReference.setWordWrap(false);
        fullTitleReference.setTextFormat(Qt::PlainText);
        QPalette referencePalette = fullTitleReference.palette();
        referencePalette.setColor(QPalette::WindowText, toolbarTextColor);
        referencePalette.setColor(QPalette::Text, toolbarTextColor);
        fullTitleReference.setPalette(referencePalette);
        fullTitleReference.resize(referenceWidth, referenceHeight);
        fullTitleReference.ensurePolished();

        QImage referenceImage(referenceWidth, referenceHeight,
            QImage::Format_ARGB32);
        referenceImage.fill(toolbarBackground);
        QPainter painter(&referenceImage);
        fullTitleReference.render(&painter, QPoint(0, 0), QRegion(),
            QWidget::DrawChildren);
        return referenceImage;
    };
    const auto renderedSuffixEvidence = [](
        const QImage& image,
        const QImage& referenceImage,
        const QString& fullTitle,
        const QString& suffix,
        const QStringList& suffixGroups,
        const QFont& titleFont,
        const QColor& toolbarBackground
        )
    {
        const QFontMetricsF metrics(titleFont);
        const qreal fullAdvance = metrics.horizontalAdvance(fullTitle);
        const qreal textStart = (image.width() - fullAdvance) / 2.0;
        const int prefixLength = fullTitle.size() - suffix.size();
        QJsonArray groups;
        bool allGroupsVisible = prefixLength >= 0 && image.width() > 0
            && referenceImage.width() >= image.width();
        const int referenceCropX =
            (referenceImage.width() - image.width()) / 2;
        const int toolbarHeight = qMin(48,
            qMin(image.height(), referenceImage.height()));
        qint64 totalInkPixelCount = 0;
        int searchOffset = 0;
        for (const QString& group : suffixGroups)
        {
            const int groupIndex = suffix.indexOf(group, searchOffset);
            if (groupIndex < 0)
            {
                allGroupsVisible = false;
                continue;
            }

            const QString textBeforeGroup = fullTitle.left(
                prefixLength + groupIndex);
            const QString textThroughGroup = textBeforeGroup + group;
            const qreal groupStart = textStart
                + metrics.horizontalAdvance(textBeforeGroup);
            const qreal groupEnd = textStart
                + metrics.horizontalAdvance(textThroughGroup);
            const int left = qMax(0, qFloor(groupStart) - 1);
            const int right = qMin(image.width(), qCeil(groupEnd) + 1);
            const int referenceLeft = referenceCropX + left;
            const int referenceRight = referenceCropX + right;
            qint64 groupInkPixelCount = 0;
            qint64 referenceInkPixelCount = 0;
            for (int y = 0; y < toolbarHeight; ++y)
            {
                for (int x = left; x < right; ++x)
                {
                    const QColor pixel = image.pixelColor(x, y);
                    const int colorDistance =
                        qAbs(pixel.red() - toolbarBackground.red())
                        + qAbs(pixel.green() - toolbarBackground.green())
                        + qAbs(pixel.blue() - toolbarBackground.blue());
                    if (pixel.alpha() > 200 && colorDistance >= 80)
                    {
                        ++groupInkPixelCount;
                    }

                    const QColor referencePixel = referenceImage.pixelColor(
                        referenceLeft + (x - left), y);
                    const int referenceColorDistance =
                        qAbs(referencePixel.red() - toolbarBackground.red())
                        + qAbs(referencePixel.green() - toolbarBackground.green())
                        + qAbs(referencePixel.blue() - toolbarBackground.blue());
                    if (referencePixel.alpha() > 200
                        && referenceColorDistance >= 80)
                    {
                        ++referenceInkPixelCount;
                    }
                }
            }
            const qint64 minimumRequiredInkPixelCount = qMax<qint64>(
                1, qCeil(referenceInkPixelCount * 0.65));
            totalInkPixelCount += groupInkPixelCount;
            const bool groupVisible = groupInkPixelCount
                >= minimumRequiredInkPixelCount;
            allGroupsVisible = allGroupsVisible && groupVisible;
            groups.append(QJsonObject{
                {QStringLiteral("text"), group},
                {QStringLiteral("startX"), groupStart},
                {QStringLiteral("endX"), groupEnd},
                {QStringLiteral("referenceInkPixelCount"),
                 referenceInkPixelCount},
                {QStringLiteral("renderedInkPixelCount"),
                 groupInkPixelCount},
                {QStringLiteral("minimumRequiredInkPixelCount"),
                 minimumRequiredInkPixelCount},
                {QStringLiteral("visible"), groupVisible}
            });
            searchOffset = groupIndex + group.size();
        }

        return QJsonObject{
            {QStringLiteral("expectedFullTitle"), fullTitle},
            {QStringLiteral("expectedSuffix"), suffix},
            {QStringLiteral("suffixGroups"),
             QJsonArray::fromStringList(suffixGroups)},
            {QStringLiteral("referenceWidth"), referenceImage.width()},
            {QStringLiteral("fontPixelSize"),
             QFontInfo(titleFont).pixelSize()},
            {QStringLiteral("imageWidth"), image.width()},
            {QStringLiteral("toolbarHeight"), qMin(48, image.height())},
            {QStringLiteral("expectedFullTitleAdvance"), fullAdvance},
            {QStringLiteral("toolbarBackground"), toolbarBackground.name()},
            {QStringLiteral("suffixGroupsEvidence"), groups},
            {QStringLiteral("suffixInkPixelCount"), totalInkPixelCount},
            {QStringLiteral("allSuffixGroupsVisible"), allGroupsVisible}
        };
    };
    const auto eraseExpectedSuffixGroup = [](
        const QImage& image,
        const QString& fullTitle,
        const QString& suffix,
        const QString& group,
        const QFont& titleFont,
        const QColor& toolbarBackground
        )
    {
        QImage result = image.copy();
        const int groupIndex = suffix.indexOf(group);
        const int prefixLength = fullTitle.size() - suffix.size();
        if (groupIndex < 0 || prefixLength < 0 || result.isNull())
        {
            return result;
        }

        const QFontMetricsF metrics(titleFont);
        const qreal textStart = (result.width()
            - metrics.horizontalAdvance(fullTitle)) / 2.0;
        const QString textBeforeGroup = fullTitle.left(
            prefixLength + groupIndex);
        const QString textThroughGroup = textBeforeGroup + group;
        const int left = qMax(0, qFloor(textStart
            + metrics.horizontalAdvance(textBeforeGroup)) - 2);
        const int right = qMin(result.width(), qCeil(textStart
            + metrics.horizontalAdvance(textThroughGroup)) + 2);
        QPainter painter(&result);
        painter.fillRect(QRect(left, 0, right - left,
            qMin(48, result.height())), toolbarBackground);
        return result;
    };
    for (const bool korean : {false, true})
    {
        QLocale::setDefault(QLocale(korean
            ? QStringLiteral("ko_KR") : QStringLiteral("en_US")));
        CalendarPageFixture fixture;
        QVERIFY2(fixture.initialize(true, korean, true, true, false, true),
                 qPrintable(fixture.initializationError));
        QCOMPARE(fixture.captureFontResources.size(), 7);
        const QString expectedPrimaryFamily = korean
            ? fixture.capturePretendardFamily : fixture.captureInterFamily;
        const QFont applicationFont = QApplication::font();
        QCOMPARE(applicationFont.families().value(0), expectedPrimaryFamily);
        const QFontInfo applicationFontInfo(applicationFont);
        QCOMPARE(applicationFontInfo.family(), expectedPrimaryFamily);

        QFont latinProbeFont(fixture.captureInterFamily);
        latinProbeFont.setPointSize(12);
        const QRawFont latinProbe = QRawFont::fromFont(latinProbeFont);
        QVERIFY(latinProbe.isValid());
        QVERIFY(latinProbe.supportsCharacter(static_cast<uint>('A')));
        QFont hangulProbeFont(fixture.capturePretendardFamily);
        hangulProbeFont.setPointSize(12);
        const QRawFont hangulProbe = QRawFont::fromFont(hangulProbeFont);
        QVERIFY(hangulProbe.isValid());
        QVERIFY(hangulProbe.supportsCharacter(0xAC00));
        const QFontMetricsF latinMetrics(latinProbeFont);
        const QFontMetricsF hangulMetrics(hangulProbeFont);
        QVERIFY(latinMetrics.horizontalAdvance(QStringLiteral("October 2026")) > 0);
        QVERIFY(hangulMetrics.horizontalAdvance(QStringLiteral("달력")) > 0);
        fontRegistrationRecords.append(QJsonObject{
            {QStringLiteral("locale"), korean
                ? QStringLiteral("ko") : QStringLiteral("en")},
            {QStringLiteral("registrationScope"), QStringLiteral(
                "Startup/application-bootstrap")},
            {QStringLiteral("registrationSequence"), QStringLiteral(
                "ApplicationBootstrap::loadRequiredFonts")},
            {QStringLiteral("applicationFontFamilies"), QJsonArray::fromStringList(
                applicationFont.families())},
            {QStringLiteral("resolvedApplicationFamily"),
             applicationFontInfo.family()},
            {QStringLiteral("applicationFontPointSize"),
             applicationFont.pointSize()},
            {QStringLiteral("applicationFontPixelSize"),
             applicationFontInfo.pixelSize()},
            {QStringLiteral("englishFamily"), fixture.captureInterFamily},
            {QStringLiteral("englishFamilyResolves"),
             QFontInfo(latinProbeFont).family()},
            {QStringLiteral("englishProbeText"), QStringLiteral("October 2026")},
            {QStringLiteral("englishProbeAdvance"),
             latinMetrics.horizontalAdvance(QStringLiteral("October 2026"))},
            {QStringLiteral("latinGlyphAvailable"),
             latinProbe.supportsCharacter(static_cast<uint>('A'))},
            {QStringLiteral("koreanFamily"), fixture.capturePretendardFamily},
            {QStringLiteral("koreanFamilyResolves"),
             QFontInfo(hangulProbeFont).family()},
            {QStringLiteral("koreanProbeText"), QStringLiteral("달력")},
            {QStringLiteral("koreanProbeAdvance"),
             hangulMetrics.horizontalAdvance(QStringLiteral("달력"))},
            {QStringLiteral("hangulGlyphAvailable"),
             hangulProbe.supportsCharacter(0xAC00)},
            {QStringLiteral("packagedFontResources"),
             fixture.captureFontResources}
        });
        auto* const pageQml = qobject_cast<QQuickWidget*>(
            fixture.page->calendarViewWidget());
        QVERIFY(pageQml);
        QVERIFY(pageQml->rootObject()->setProperty(
            "shownDate", QDateTime(visibleMonth, QTime(0, 0))));
        QTRY_VERIFY_WITH_TIMEOUT(
            fixture.page->eventModel()->isMonthLoaded(2026, 10), 5000);
        QTRY_VERIFY_WITH_TIMEOUT(
            !fixture.page->runtimeMetrics().cacheLoading, 5000);
        auto* const academicProvider = fixture.page->findChild<
            AcademicCalendarProvider*>();
        QVERIFY(academicProvider);
        const QString expectedTitle = korean
            ? QStringLiteral(
                "10\uC6D4 2026 \u2014 \uCD08\uB4F1: \uC5EC\uB984 "
                "11\uC8FC\uCC28 \u00b7 \uC911\uB4F1: \uAC00\uC744 "
                "7\uC8FC\uCC28")
            : QStringLiteral(
                "October 2026 \u2014 Elem: Summer Wk 11 "
                "\u00b7 MS: Fall Wk 7");
        const QString expectedTitleSuffix = korean
            ? QStringLiteral("\uAC00\uC744 7\uC8FC\uCC28")
            : QStringLiteral("Fall Wk 7");
        const QStringList expectedTitleSuffixGroups = korean
            ? QStringList{
                QStringLiteral("\uAC00\uC744"),
                QStringLiteral("7\uC8FC\uCC28")}
            : QStringList{
                QStringLiteral("Fall"),
                QStringLiteral("Wk"),
                QStringLiteral("7")};
        QCOMPARE(academicProvider->monthTitle(2026, 9), expectedTitle);
        QVERIFY(expectedTitle.endsWith(expectedTitleSuffix));

        QWidget narrowWidgetContainer;
        narrowWidgetContainer.resize(420, 120);
        Ui::CalendarViewRequest narrowWidgetRequest;
        narrowWidgetRequest.parent = &narrowWidgetContainer;
        narrowWidgetRequest.eventModel = fixture.page->eventModel();
        narrowWidgetRequest.academicProvider = academicProvider;
        narrowWidgetRequest.shownMonth = visibleMonth;
        const Ui::CalendarViewCreationResult narrowWidgetResult =
            Ui::createWidgetCalendarView(narrowWidgetRequest);
        QVERIFY2(narrowWidgetResult.view && narrowWidgetResult.error.isEmpty(),
                 qPrintable(narrowWidgetResult.error));
        narrowWidgetResult.view->widget()->setGeometry(
            narrowWidgetContainer.rect());
        narrowWidgetContainer.show();
        narrowWidgetResult.view->widget()->show();
        QApplication::processEvents();

        auto* const narrowTitle = narrowWidgetResult.view->widget()
            ->findChild<QLabel*>(QStringLiteral("calendarWidgetMonthTitle"));
        QVERIFY(narrowTitle);
        QCOMPARE(narrowTitle->text(), expectedTitle);
        QCOMPARE(narrowTitle->accessibleName(), expectedTitle);
        QCOMPARE(narrowTitle->toolTip(), expectedTitle);
        QVERIFY(narrowTitle->contentsRect().width() > 0);
        QVERIFY(narrowTitle->contentsRect().height() > 0);
        const QRectF narrowTitleBounds = QFontMetricsF(narrowTitle->font())
            .boundingRect(QRectF(narrowTitle->contentsRect()),
                Qt::AlignCenter | Qt::TextWordWrap, narrowTitle->text());
        QVERIFY(narrowTitleBounds.width()
            <= narrowTitle->contentsRect().width() + 0.5);
        QVERIFY(narrowTitleBounds.height()
            <= narrowTitle->contentsRect().height() + 0.5);

        const QVariantMap eventColors = pageQml->rootObject()->property(
            "eventTypeColors").toMap();
        const QVariantMap eventTextColors = pageQml->rootObject()->property(
            "eventTypeTextColors").toMap();

        for (const bool embedded : {false, true})
        {
            for (const bool dark : {false, true})
            {
                QPalette palette = QApplication::style()->standardPalette();
                if (dark)
                {
                    palette.setColor(QPalette::Window,
                        QColor(QStringLiteral("#20242a")));
                    palette.setColor(QPalette::WindowText,
                        QColor(QStringLiteral("#f3f4f6")));
                    palette.setColor(QPalette::Base,
                        QColor(QStringLiteral("#272c33")));
                    palette.setColor(QPalette::AlternateBase,
                        QColor(QStringLiteral("#343b44")));
                    palette.setColor(QPalette::Text,
                        QColor(QStringLiteral("#f3f4f6")));
                    palette.setColor(QPalette::Button,
                        QColor(QStringLiteral("#343b44")));
                    palette.setColor(QPalette::ButtonText,
                        QColor(QStringLiteral("#f3f4f6")));
                    palette.setColor(QPalette::Highlight,
                        QColor(QStringLiteral("#6487a5")));
                    palette.setColor(QPalette::HighlightedText, Qt::white);
                }

                const QString localeTag = korean
                    ? QStringLiteral("ko") : QStringLiteral("en");
                const QString paletteTag = dark
                    ? QStringLiteral("dark") : QStringLiteral("light");
                const QString contextTag = embedded
                    ? QStringLiteral("embedded") : QStringLiteral("standalone");
                QWidget qmlContainer;
                QWidget widgetContainer;
                qmlContainer.resize(1100, embedded ? 900 : 620);
                widgetContainer.resize(qmlContainer.size());
                qmlContainer.setPalette(palette);
                widgetContainer.setPalette(palette);

                Ui::CalendarViewRequest qmlRequest;
                qmlRequest.parent = &qmlContainer;
                qmlRequest.eventModel = fixture.page->eventModel();
                qmlRequest.academicProvider = academicProvider;
                qmlRequest.shownMonth = visibleMonth;
                qmlRequest.embedded = embedded;
                const Ui::CalendarViewCreationResult qmlResult =
                    Ui::createQmlCalendarView(qmlRequest);
                QVERIFY2(qmlResult.view && qmlResult.error.isEmpty(),
                         qPrintable(qmlResult.error));

                Ui::CalendarViewRequest widgetRequest = qmlRequest;
                widgetRequest.parent = &widgetContainer;
                const Ui::CalendarViewCreationResult widgetResult =
                    Ui::createWidgetCalendarView(widgetRequest);
                QVERIFY2(widgetResult.view && widgetResult.error.isEmpty(),
                         qPrintable(widgetResult.error));

                qmlResult.view->applyPalette(palette, embedded);
                widgetResult.view->applyPalette(palette, embedded);
                qmlResult.view->setEventTypeColors(eventColors, eventTextColors);
                widgetResult.view->setEventTypeColors(
                    eventColors, eventTextColors);
                qmlResult.view->widget()->setGeometry(qmlContainer.rect());
                widgetResult.view->widget()->setGeometry(widgetContainer.rect());
                qmlContainer.show();
                widgetContainer.show();
                qmlResult.view->widget()->show();
                widgetResult.view->widget()->show();
                QApplication::processEvents();
                QTest::qWait(120);

                auto* const qmlWidget = qobject_cast<QQuickWidget*>(
                    qmlResult.view->widget());
                QVERIFY(qmlWidget);
                QVERIFY(qmlWidget->rootObject());
                auto* const firstWidgetCell = widgetResult.view->widget()
                    ->findChild<QWidget*>(QStringLiteral(
                        "calendarWidgetDayCell"));
                QVERIFY(firstWidgetCell);
                const auto colorName = [](const QColor& color)
                {
                    return color.isValid() ? color.name() : QString{};
                };
                const QJsonObject appearanceColors{
                    {QStringLiteral("qmlGridLine"), colorName(
                        qmlWidget->rootObject()->property(
                            "gridLineColor").value<QColor>())},
                    {QStringLiteral("widgetGridLine"), colorName(
                        firstWidgetCell->property(
                            "calendarGridLineColor").value<QColor>())},
                    {QStringLiteral("paletteMid"),
                     palette.color(QPalette::Mid).name()},
                    {QStringLiteral("qmlCalendarBackground"), colorName(
                        qmlWidget->rootObject()->property(
                            "calendarBackground").value<QColor>())},
                    {QStringLiteral("widgetCalendarBackground"), colorName(
                        widgetResult.view->widget()->property(
                            "calendarCalendarBackground").value<QColor>())}
                };
                const QImage qmlImage = qmlResult.view->widget()
                    ->grab().toImage().convertToFormat(QImage::Format_ARGB32);
                const QImage widgetImage = widgetResult.view->widget()
                    ->grab().toImage().convertToFormat(QImage::Format_ARGB32);
                QVERIFY(!qmlImage.isNull());
                QVERIFY(!widgetImage.isNull());
                QCOMPARE(qmlImage.size(), widgetImage.size());

                auto* const widgetTitle = widgetResult.view->widget()
                    ->findChild<QLabel*>(QStringLiteral(
                        "calendarWidgetMonthTitle"));
                QVERIFY(widgetTitle);
                QCOMPARE(widgetTitle->text(), expectedTitle);
                QCOMPARE(widgetTitle->accessibleName(), expectedTitle);
                QCOMPARE(widgetTitle->toolTip(), expectedTitle);

                const QColor toolbarBackground = widgetTitle->parentWidget()
                    ->palette().color(QPalette::Window);
                QCOMPARE(toolbarBackground,
                    qmlWidget->rootObject()->property("toolbarColor")
                        .value<QColor>());
                QFont qmlTitleFont = qmlWidget->font();
                const qreal qmlBaseFontPixelSize = qmlWidget->rootObject()
                    ->property("baseFontPixelSize").toDouble();
                qmlTitleFont.setPixelSize(qMax(1,
                    qRound(qmlBaseFontPixelSize * 1.12)));
                const QColor toolbarTextColor = qmlWidget->rootObject()
                    ->property("toolbarTextColor").value<QColor>();
                QCOMPARE(toolbarTextColor,
                    widgetTitle->palette().color(QPalette::WindowText));
                const QImage qmlFullTitleReference =
                    renderFullTitleReference(expectedTitle, qmlTitleFont,
                        toolbarBackground, toolbarTextColor);
                const QImage widgetFullTitleReference =
                    renderFullTitleReference(expectedTitle,
                        widgetTitle->font(), toolbarBackground,
                        toolbarTextColor);
                const QJsonObject qmlRenderedSuffix =
                    renderedSuffixEvidence(qmlImage, qmlFullTitleReference,
                        expectedTitle, expectedTitleSuffix,
                        expectedTitleSuffixGroups, qmlTitleFont,
                        toolbarBackground);
                const QJsonObject widgetRenderedSuffix =
                    renderedSuffixEvidence(widgetImage,
                        widgetFullTitleReference, expectedTitle,
                        expectedTitleSuffix, expectedTitleSuffixGroups,
                        widgetTitle->font(), toolbarBackground);
                QJsonArray qmlMissingSuffixControls;
                for (const QString& group : expectedTitleSuffixGroups)
                {
                    const QImage imageWithMissingGroup =
                        eraseExpectedSuffixGroup(qmlImage, expectedTitle,
                            expectedTitleSuffix, group, qmlTitleFont,
                            toolbarBackground);
                    const QJsonObject missingGroupEvidence =
                        renderedSuffixEvidence(imageWithMissingGroup,
                            qmlFullTitleReference, expectedTitle,
                            expectedTitleSuffix, expectedTitleSuffixGroups,
                            qmlTitleFont, toolbarBackground);
                    bool erasedGroupDetected = false;
                    for (const QJsonValue value : missingGroupEvidence.value(
                        QStringLiteral("suffixGroupsEvidence")).toArray())
                    {
                        const QJsonObject groupEvidence = value.toObject();
                        if (groupEvidence.value(QStringLiteral("text"))
                            .toString() == group)
                        {
                            erasedGroupDetected = !groupEvidence.value(
                                QStringLiteral("visible")).toBool();
                            break;
                        }
                    }
                    QVERIFY2(erasedGroupDetected,
                        qPrintable(QStringLiteral(
                            "The QML pixel oracle did not reject missing "
                            "suffix group '%1': %2")
                            .arg(group, QString::fromUtf8(QJsonDocument(
                                missingGroupEvidence).toJson(
                                    QJsonDocument::Compact)))));
                    qmlMissingSuffixControls.append(QJsonObject{
                        {QStringLiteral("erasedSuffixGroup"), group},
                        {QStringLiteral("missingGroupRejected"),
                         erasedGroupDetected}
                    });
                }
                QJsonArray widgetMissingSuffixControls;
                for (const QString& group : expectedTitleSuffixGroups)
                {
                    const QImage imageWithMissingGroup =
                        eraseExpectedSuffixGroup(widgetImage, expectedTitle,
                            expectedTitleSuffix, group, widgetTitle->font(),
                            toolbarBackground);
                    const QJsonObject missingGroupEvidence =
                        renderedSuffixEvidence(imageWithMissingGroup,
                            widgetFullTitleReference, expectedTitle,
                            expectedTitleSuffix, expectedTitleSuffixGroups,
                            widgetTitle->font(), toolbarBackground);
                    bool erasedGroupDetected = false;
                    for (const QJsonValue value : missingGroupEvidence.value(
                        QStringLiteral("suffixGroupsEvidence")).toArray())
                    {
                        const QJsonObject groupEvidence = value.toObject();
                        if (groupEvidence.value(QStringLiteral("text"))
                            .toString() == group)
                        {
                            erasedGroupDetected = !groupEvidence.value(
                                QStringLiteral("visible")).toBool();
                            break;
                        }
                    }
                    QVERIFY2(erasedGroupDetected,
                        qPrintable(QStringLiteral(
                            "The QWidget pixel oracle did not reject missing "
                            "suffix group '%1': %2")
                            .arg(group, QString::fromUtf8(QJsonDocument(
                                missingGroupEvidence).toJson(
                                    QJsonDocument::Compact)))));
                    widgetMissingSuffixControls.append(QJsonObject{
                        {QStringLiteral("erasedSuffixGroup"), group},
                        {QStringLiteral("missingGroupRejected"),
                         erasedGroupDetected}
                    });
                }
                QVERIFY2(qmlRenderedSuffix.value(
                            QStringLiteral("allSuffixGroupsVisible")).toBool(),
                    qPrintable(QStringLiteral(
                        "The QML capture is missing rendered suffix groups: %1")
                        .arg(QString::fromUtf8(QJsonDocument(
                            qmlRenderedSuffix).toJson(
                                QJsonDocument::Compact)))));
                QVERIFY2(widgetRenderedSuffix.value(
                            QStringLiteral("allSuffixGroupsVisible")).toBool(),
                    qPrintable(QStringLiteral(
                        "The QWidget capture is missing rendered suffix groups: %1")
                        .arg(QString::fromUtf8(QJsonDocument(
                            widgetRenderedSuffix).toJson(
                                QJsonDocument::Compact)))));

                const qreal widgetTitleAdvance =
                    QFontMetricsF(widgetTitle->font())
                        .horizontalAdvance(widgetTitle->text());
                QVERIFY(widgetTitle->contentsRect().width() > 0);
                const QPoint titlePosition = widgetTitle->mapTo(
                    widgetResult.view->widget(), QPoint(0, 0));
                const QRect titleImageRect(titlePosition,
                    widgetTitle->size());
                QVERIFY(widgetImage.rect().contains(titleImageRect));
                const QImage renderedTitleImage =
                    widgetImage.copy(titleImageRect);

                const int referenceTitleWidth = qMax(
                    4096,
                    qCeil(widgetTitleAdvance)
                        + widgetTitle->contentsMargins().left()
                        + widgetTitle->contentsMargins().right() + 32);
                QLabel fullTitleReference;
                fullTitleReference.setText(expectedTitle);
                fullTitleReference.setFont(widgetTitle->font());
                fullTitleReference.setPalette(widgetTitle->palette());
                fullTitleReference.setAlignment(widgetTitle->alignment());
                fullTitleReference.setWordWrap(false);
                fullTitleReference.setMargin(widgetTitle->margin());
                fullTitleReference.setIndent(widgetTitle->indent());
                fullTitleReference.setContentsMargins(
                    widgetTitle->contentsMargins());
                fullTitleReference.setTextFormat(widgetTitle->textFormat());
                fullTitleReference.resize(
                    referenceTitleWidth, titleImageRect.height());
                fullTitleReference.ensurePolished();

                QImage fullTitleReferenceImage(
                    referenceTitleWidth, titleImageRect.height(),
                    QImage::Format_ARGB32);
                fullTitleReferenceImage.fill(toolbarBackground);
                {
                    QPainter painter(&fullTitleReferenceImage);
                    fullTitleReference.render(&painter, QPoint(0, 0),
                        QRegion(), QWidget::DrawChildren);
                }
                const int referenceCropX =
                    (referenceTitleWidth - titleImageRect.width()) / 2;
                const QImage expectedTitleImage =
                    fullTitleReferenceImage.copy(referenceCropX, 0,
                        titleImageRect.width(), titleImageRect.height());
                QCOMPARE(renderedTitleImage.size(), expectedTitleImage.size());

                qint64 renderedTitlePixelMismatchCount = 0;
                qint64 renderedTitleInkPixelCount = 0;
                qint64 expectedTitleInkPixelCount = 0;
                for (int y = 0; y < expectedTitleImage.height(); ++y)
                {
                    for (int x = 0; x < expectedTitleImage.width(); ++x)
                    {
                        const QColor expectedPixel =
                            expectedTitleImage.pixelColor(x, y);
                        const QColor renderedPixel =
                            renderedTitleImage.pixelColor(x, y);
                        if (expectedPixel != renderedPixel)
                        {
                            ++renderedTitlePixelMismatchCount;
                        }
                        if (expectedPixel != toolbarBackground)
                        {
                            ++expectedTitleInkPixelCount;
                        }
                        if (renderedPixel != toolbarBackground)
                        {
                            ++renderedTitleInkPixelCount;
                        }
                    }
                }
                const bool renderedTitleMatchesExpected =
                    renderedTitlePixelMismatchCount == 0
                    && renderedTitleInkPixelCount
                        == expectedTitleInkPixelCount;
                const bool titleFitsOneLine =
                    widgetTitleAdvance
                        <= widgetTitle->contentsRect().width() + 0.5
                    && renderedTitleMatchesExpected
                    && qmlRenderedSuffix.value(QStringLiteral(
                        "allSuffixGroupsVisible")).toBool()
                    && widgetRenderedSuffix.value(QStringLiteral(
                        "allSuffixGroupsVisible")).toBool();
                QVERIFY2(renderedTitleMatchesExpected,
                    qPrintable(QStringLiteral(
                        "Rendered title differs from the complete reference: "
                        "%1 pixels differ; expected ink=%2, rendered ink=%3, "
                        "title='%4'.")
                        .arg(renderedTitlePixelMismatchCount)
                        .arg(expectedTitleInkPixelCount)
                        .arg(renderedTitleInkPixelCount)
                        .arg(expectedTitle)));
                QVERIFY(titleFitsOneLine);

                const QString baseName = QStringLiteral("calendar-%1-%2-%3")
                    .arg(localeTag, paletteTag, contextTag);
                const QString qmlPath = outputDirectory + QLatin1Char('/')
                    + baseName + QStringLiteral("-qml.png");
                const QString widgetPath = outputDirectory + QLatin1Char('/')
                    + baseName + QStringLiteral("-widget.png");
                QVERIFY(qmlImage.save(qmlPath));
                QVERIFY(widgetImage.save(widgetPath));

                const int width = qmlImage.width();
                const int height = qmlImage.height();
                const qint64 comparedPixelCount =
                    static_cast<qint64>(width) * height;
                qint64 changedPixels = 0;
                qint64 rgbChangedPixels = 0;
                qint64 alphaChangedPixels = 0;
                qint64 rgbChannelDifference = 0;
                qint64 alphaDifference = 0;
                int maximumRgbChannelDelta = 0;
                int maximumAlphaDelta = 0;
                for (int y = 0; y < height; ++y)
                {
                    for (int x = 0; x < width; ++x)
                    {
                        const QColor qmlColor = qmlImage.pixelColor(x, y);
                        const QColor widgetColor = widgetImage.pixelColor(x, y);
                        const int redDelta =
                            qAbs(qmlColor.red() - widgetColor.red());
                        const int greenDelta =
                            qAbs(qmlColor.green() - widgetColor.green());
                        const int blueDelta =
                            qAbs(qmlColor.blue() - widgetColor.blue());
                        const int alphaDelta =
                            qAbs(qmlColor.alpha() - widgetColor.alpha());
                        const int rgbDelta = redDelta + greenDelta + blueDelta;
                        if (rgbDelta != 0 || alphaDelta != 0)
                        {
                            ++changedPixels;
                        }
                        if (rgbDelta != 0)
                        {
                            ++rgbChangedPixels;
                        }
                        if (alphaDelta != 0)
                        {
                            ++alphaChangedPixels;
                        }
                        rgbChannelDifference += redDelta + greenDelta + blueDelta;
                        alphaDifference += alphaDelta;
                        maximumRgbChannelDelta = qMax(maximumRgbChannelDelta,
                            qMax(redDelta, qMax(greenDelta, blueDelta)));
                        maximumAlphaDelta = qMax(maximumAlphaDelta, alphaDelta);
                    }
                }
                const auto fileSha256 = [](const QString& path)
                {
                    QFile file(path);
                    if (!file.open(QIODevice::ReadOnly))
                    {
                        return QString{};
                    }
                    return QString::fromLatin1(
                        QCryptographicHash::hash(
                            file.readAll(), QCryptographicHash::Sha256).toHex());
                };
                const QString qmlResolvedFamily =
                    QFontInfo(qmlWidget->font()).family();
                const QString widgetResolvedFamily = QFontInfo(
                    widgetResult.view->widget()->font()).family();
                QCOMPARE(qmlResolvedFamily, expectedPrimaryFamily);
                QCOMPARE(widgetResolvedFamily, expectedPrimaryFamily);
                const QString qmlSha256 = fileSha256(qmlPath);
                const QString widgetSha256 = fileSha256(widgetPath);
                QVERIFY(!qmlSha256.isEmpty());
                QVERIFY(!widgetSha256.isEmpty());
                records.append(QJsonObject{
                    {QStringLiteral("locale"), localeTag},
                    {QStringLiteral("palette"), paletteTag},
                    {QStringLiteral("context"), contextTag},
                    {QStringLiteral("qmlImage"), QFileInfo(qmlPath).fileName()},
                    {QStringLiteral("qmlSha256"), qmlSha256},
                    {QStringLiteral("widgetImage"), QFileInfo(widgetPath).fileName()},
                    {QStringLiteral("widgetSha256"), widgetSha256},
                    {QStringLiteral("appearanceColors"), appearanceColors},
                    {QStringLiteral("calendarTitle"), QJsonObject{
                        {QStringLiteral("expectedFullText"), expectedTitle},
                        {QStringLiteral("expectedSuffix"),
                         expectedTitleSuffix},
                        {QStringLiteral("referenceRenderWidth"),
                         referenceTitleWidth},
                        {QStringLiteral("text"), widgetTitle->text()},
                        {QStringLiteral("accessibleName"),
                         widgetTitle->accessibleName()},
                        {QStringLiteral("toolTip"), widgetTitle->toolTip()},
                        {QStringLiteral("fontPixelSize"),
                         QFontInfo(widgetTitle->font()).pixelSize()},
                        {QStringLiteral("availableWidth"),
                         widgetTitle->contentsRect().width()},
                        {QStringLiteral("textAdvance"),
                         widgetTitleAdvance},
                        {QStringLiteral("renderedPixelMatch"),
                         renderedTitleMatchesExpected},
                        {QStringLiteral("renderedPixelMismatchCount"),
                         renderedTitlePixelMismatchCount},
                        {QStringLiteral("expectedInkPixelCount"),
                         expectedTitleInkPixelCount},
                        {QStringLiteral("renderedInkPixelCount"),
                         renderedTitleInkPixelCount},
                        {QStringLiteral("qmlRenderedSuffix"),
                         qmlRenderedSuffix},
                        {QStringLiteral("widgetRenderedSuffix"),
                         widgetRenderedSuffix},
                        {QStringLiteral("qmlMissingSuffixPixelControls"),
                         qmlMissingSuffixControls},
                        {QStringLiteral("widgetMissingSuffixPixelControls"),
                         widgetMissingSuffixControls},
                        {QStringLiteral("qmlRenderedComplete"),
                         qmlRenderedSuffix.value(QStringLiteral(
                             "allSuffixGroupsVisible")).toBool()},
                        {QStringLiteral("widgetRenderedComplete"),
                         widgetRenderedSuffix.value(QStringLiteral(
                             "allSuffixGroupsVisible")).toBool()},
                        {QStringLiteral("fitsSingleLine"),
                         titleFitsOneLine}
                    }},
                    {QStringLiteral("fontResolution"), QJsonObject{
                        {QStringLiteral("requestedApplicationFamilies"),
                         QJsonArray::fromStringList(QApplication::font().families())},
                        {QStringLiteral("resolvedApplicationFamily"),
                         QFontInfo(QApplication::font()).family()},
                        {QStringLiteral("resolvedQmlFamily"), qmlResolvedFamily},
                        {QStringLiteral("resolvedWidgetFamily"),
                         widgetResolvedFamily}
                    }},
                    {QStringLiteral("imageSize"), QJsonArray{
                        qmlImage.width(), qmlImage.height()}},
                    {QStringLiteral("comparedSize"), QJsonArray{width, height}},
                    {QStringLiteral("changedPixelCount"), changedPixels},
                    {QStringLiteral("comparedPixelCount"), comparedPixelCount},
                    {QStringLiteral("rgbChangedPixelCount"), rgbChangedPixels},
                    {QStringLiteral("alphaChangedPixelCount"), alphaChangedPixels},
                    {QStringLiteral("rgbChannelDifferenceSum"),
                     rgbChannelDifference},
                    {QStringLiteral("alphaDifferenceSum"), alphaDifference},
                    {QStringLiteral("maximumAbsoluteRgbChannelDelta"),
                     maximumRgbChannelDelta},
                    {QStringLiteral("maximumAbsoluteAlphaDelta"),
                     maximumAlphaDelta},
                    {QStringLiteral("rgbMeanAbsoluteChannelDelta"),
                     comparedPixelCount > 0
                        ? static_cast<double>(rgbChannelDifference)
                            / (static_cast<double>(comparedPixelCount) * 3.0)
                        : 0.0},
                    {QStringLiteral("alphaMeanAbsoluteDelta"),
                     comparedPixelCount > 0
                        ? static_cast<double>(alphaDifference)
                            / static_cast<double>(comparedPixelCount)
                        : 0.0},
                    {QStringLiteral("meanAbsoluteChannelDelta"),
                     comparedPixelCount > 0
                        ? static_cast<double>(rgbChannelDifference
                            + alphaDifference)
                            / (static_cast<double>(comparedPixelCount) * 4.0)
                        : 0.0}
                });
                qmlContainer.close();
                widgetContainer.close();
            }
        }
        QVERIFY(fixture.host->activatePage(QStringLiteral("other")));
    }

    const auto sha256File = [](const QString& path)
    {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly))
        {
            return QString{};
        }
        return QString::fromLatin1(
            QCryptographicHash::hash(
                file.readAll(), QCryptographicHash::Sha256).toHex());
    };
    const QString sourceRoot = QStringLiteral(CLASSMNGR_SOURCE_DIR);
    const QStringList sourcePaths{
        QStringLiteral("tests/next/calendar_page_tests.cpp"),
        QStringLiteral("cmake/tests/next.cmake"),
        QStringLiteral("src/next/bootstrap/application_bootstrap.cpp"),
        QStringLiteral("src/next/platform/qt_font_family_availability_adapter.cpp"),
        QStringLiteral("src/next/ui/font_size_presenter.h"),
        QStringLiteral("src/next/resources/resource_loader.cpp"),
        QStringLiteral("src/next/ui/calendar_view.cpp"),
        QStringLiteral("src/next/ui/calendar_view.h"),
        QStringLiteral("scripts/phase9/run_calendar_view_ab_probe.ps1"),
        QStringLiteral("src/features/calendar/ui/qml/EventCalendar.qml"),
        QStringLiteral("src/features/calendar/ui/qml/MonthGridDelegate.qml")
    };
    QJsonArray sourceFiles;
    for (const QString& relativePath : sourcePaths)
    {
        const QString path = QDir(sourceRoot).filePath(relativePath);
        QFile file(path);
        QVERIFY2(file.open(QIODevice::ReadOnly), qPrintable(path));
        const QByteArray bytes = file.readAll();
        sourceFiles.append(QJsonObject{
            {QStringLiteral("path"), relativePath},
            {QStringLiteral("sizeBytes"), bytes.size()},
            {QStringLiteral("sha256"), QString::fromLatin1(
                QCryptographicHash::hash(
                    bytes, QCryptographicHash::Sha256).toHex())}
        });
    }
    const QString sourceIdentitySha256 = QString::fromLatin1(
        QCryptographicHash::hash(
            QJsonDocument(sourceFiles).toJson(QJsonDocument::Compact),
            QCryptographicHash::Sha256).toHex());

    const QString executablePath = QFileInfo(
        QCoreApplication::applicationFilePath()).canonicalFilePath();
    const QString executableSha256 = sha256File(executablePath);
    QVERIFY(!executableSha256.isEmpty());
    const QString buildDirectory = QFileInfo(
        QStringLiteral(CLASSMNGR_CALENDAR_CAPTURE_DIR))
        .absoluteDir().absolutePath();
    const QString cmakeCachePath = QDir(buildDirectory).filePath(
        QStringLiteral("CMakeCache.txt"));
    const QString cmakeCacheSha256 = sha256File(cmakeCachePath);
    QVERIFY(!cmakeCacheSha256.isEmpty());
    const QString resourceCatalogPath = QStringLiteral(
        CLASSMNGR_NEXT_TEST_RESOURCE_CATALOG);
    const QString resourceCatalogSha256 = sha256File(resourceCatalogPath);
    QVERIFY(!resourceCatalogSha256.isEmpty());

    const QJsonObject fixtureIdentity{
        {QStringLiteral("visibleMonth"), QStringLiteral("2026-10")},
        {QStringLiteral("firstWeekday"), QStringLiteral("Monday")},
        {QStringLiteral("upcomingScope"), QStringLiteral("This month")},
        {QStringLiteral("seedEvents"), QJsonArray{
            QJsonObject{
                {QStringLiteral("title"), QStringLiteral("Campus meeting")},
                {QStringLiteral("type"), QStringLiteral("Meeting")},
                {QStringLiteral("date"), QStringLiteral("2026-10-06")}},
            QJsonObject{
                {QStringLiteral("title"), QStringLiteral("Term planning")},
                {QStringLiteral("type"), QStringLiteral("Workshop")},
                {QStringLiteral("date"), QStringLiteral("2026-10-15")}},
            QJsonObject{
                {QStringLiteral("title"), QStringLiteral("School holiday")},
                {QStringLiteral("type"), QStringLiteral("Holiday")},
                {QStringLiteral("date"), QStringLiteral("2026-10-26")},
                {QStringLiteral("allDay"), true}}}},
        {QStringLiteral("locales"), QJsonArray{
            QStringLiteral("en_US"), QStringLiteral("ko_KR")}},
        {QStringLiteral("palettes"), QJsonArray{
            QStringLiteral("light"), QStringLiteral("dark")}},
        {QStringLiteral("contexts"), QJsonArray{
            QStringLiteral("standalone"), QStringLiteral("embedded")}},
        {QStringLiteral("imageCapture"), QStringLiteral(
            "QWidget::grab without screenshot postprocessing")}
    };
    const QString fixtureIdentitySha256 = QString::fromLatin1(
        QCryptographicHash::hash(
            QJsonDocument(fixtureIdentity).toJson(QJsonDocument::Compact),
            QCryptographicHash::Sha256).toHex());

    QFile manifest(outputDirectory + QStringLiteral("/comparison.json"));
    QVERIFY(manifest.open(QIODevice::WriteOnly | QIODevice::NewOnly));
    const QByteArray manifestBytes = QJsonDocument(QJsonObject{
        {QStringLiteral("schema"), QStringLiteral(
            "classmngr-phase9-calendar-view-font-parity-v6")},
        {QStringLiteral("taskId"), QStringLiteral(
            "P9_07_CALENDAR_TITLE_PARITY_FIX")},
        {QStringLiteral("runId"), runId},
        {QStringLiteral("status"), QStringLiteral("complete")},
        {QStringLiteral("createdAtUtc"),
         runStartedAtUtc.toString(Qt::ISODateWithMs)},
        {QStringLiteral("completedAtUtc"),
         QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)},
        {QStringLiteral("captureTest"), QStringLiteral(
            "CalendarPageTests::calendarWidgetViewMatchesQmlCapturesAcrossContexts")},
        {QStringLiteral("repositoryRoot"), sourceRoot},
        {QStringLiteral("applicationIdentity"), QJsonObject{
            {QStringLiteral("executablePath"), executablePath},
            {QStringLiteral("executableSha256"), executableSha256},
            {QStringLiteral("qtVersion"), QStringLiteral(QT_VERSION_STR)},
            {QStringLiteral("buildCpuArchitecture"),
             QSysInfo::buildCpuArchitecture()},
            {QStringLiteral("currentCpuArchitecture"),
             QSysInfo::currentCpuArchitecture()}
        }},
        {QStringLiteral("buildIdentity"), QJsonObject{
            {QStringLiteral("directory"), buildDirectory},
            {QStringLiteral("configuration"),
             QFileInfo(QCoreApplication::applicationFilePath())
                .absoluteDir().dirName()},
            {QStringLiteral("cmakeCachePath"), cmakeCachePath},
            {QStringLiteral("cmakeCacheSha256"), cmakeCacheSha256}
        }},
        {QStringLiteral("sourceIdentity"), QJsonObject{
            {QStringLiteral("sha256"), sourceIdentitySha256},
            {QStringLiteral("files"), sourceFiles}
        }},
        {QStringLiteral("resourcePackageIdentity"), QJsonObject{
            {QStringLiteral("catalogPath"), resourceCatalogPath},
            {QStringLiteral("catalogSha256"), resourceCatalogSha256},
            {QStringLiteral("resourceRoot"), QStringLiteral(
                CLASSMNGR_NEXT_TEST_RESOURCE_ROOT)}
        }},
        {QStringLiteral("fontRegistrationRecords"), fontRegistrationRecords},
        {QStringLiteral("fontRegistrationMechanism"), QJsonObject{
            {QStringLiteral("bootstrapPath"), QStringLiteral(
                "ApplicationBootstrap::loadRequiredFonts")},
            {QStringLiteral("registrationScope"), QStringLiteral(
                "Startup/application-bootstrap")},
            {QStringLiteral("applicationFontPresenter"), QStringLiteral(
                "FontSizePresenter with QtFontFamilyAvailabilityAdapter")},
            {QStringLiteral("interRegistration"), QStringLiteral(
                "ResourceLoader::loadFileBackedFont for all six packaged weights")},
            {QStringLiteral("pretendardRegistration"), QStringLiteral(
                "ResourceLoader::loadFont for packaged variable font")}
        }},
        {QStringLiteral("fixtureIdentity"), fixtureIdentity},
        {QStringLiteral("fixtureIdentitySha256"), fixtureIdentitySha256},
        {QStringLiteral("qmlFactory"), QStringLiteral("createQmlCalendarView")},
        {QStringLiteral("widgetFactory"),
         QStringLiteral("createWidgetCalendarView")},
        {QStringLiteral("comparisonSemantics"), QJsonObject{
            {QStringLiteral("rgb"), QStringLiteral(
                "Sum absolute per-channel differences over R, G, and B; mean divides by compared pixels times three.")},
            {QStringLiteral("alpha"), QStringLiteral(
                "Absolute alpha differences; mean divides by compared pixels." )},
            {QStringLiteral("acceptanceThresholdApplied"), false}
        }},
        {QStringLiteral("contexts"), records}
    }).toJson(QJsonDocument::Indented);
    QCOMPARE(manifest.write(manifestBytes), qint64(manifestBytes.size()));
}

QTEST_MAIN(CalendarPageTests)

#include "calendar_page_tests.moc"
