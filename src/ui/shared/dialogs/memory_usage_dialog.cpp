#include "memory_usage_dialog.h"

#include "core/application_services.h"
#include "core/build_info.h"
#include "core/language_service.h"
#include "core/settingsmanager.h"
#include "core/theme_service.h"
#include "features/campus/ui/campus_map_preview.h"
#include "ui/shared/dialogs/file_dialog_service.h"
#include "ui/shared/pages/pagemanager.h"
#include "ui/shared/pages/pdf_viewer_page.h"

#include <QClipboard>
#include <QCloseEvent>
#include <QComboBox>
#include <QCoreApplication>
#include <QFormLayout>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QHideEvent>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSaveFile>
#include <QShowEvent>
#include <QScrollBar>
#include <QScreen>
#include <QSysInfo>
#include <QTimer>
#include <QVBoxLayout>

#if defined(Q_OS_WIN) && defined(QT_DEBUG)
#include <windows.h>
#endif

namespace
{
constexpr auto GeometrySettingsKey = "ui/developer/memoryUsageDialog/geometry";
constexpr int DefaultRefreshIntervalMs = 1000;
constexpr int VisibleHistoryEntries = 60;

QString signedBytesText(qint64 value)
{
    const QString sign = value > 0 ? QStringLiteral("+") : QString();
    const quint64 magnitude = value < 0
        ? static_cast<quint64>(-(value + 1)) + 1
        : static_cast<quint64>(value);
    return sign + MemoryUsageMetrics::formatBytes(magnitude);
}

QString themeText(Theme theme)
{
    switch (theme)
    {
    case Theme::Dark:
        return QStringLiteral("dark");
    case Theme::Light:
        return QStringLiteral("light");
    case Theme::SystemDefault:
        return QStringLiteral("system default");
    }

    return QStringLiteral("unknown");
}

QString buildFlagsText()
{
#if defined(QT_DEBUG)
    return QStringLiteral("Debug");
#elif defined(_DEBUG)
    return QStringLiteral("Debug");
#else
    return QStringLiteral("Release");
#endif
}
}

MemoryUsageDialog::MemoryUsageDialog(
    QWidget* parent,
    PageManager* pageManager,
    std::unique_ptr<ProcessMemorySnapshotProvider> provider,
    ApplicationServices* services,
    LanguageService* languageService
    )
    : QDialog(parent)
    , m_provider(
          provider
              ? std::move(provider)
              : std::make_unique<PlatformProcessMemorySnapshotProvider>()
          )
    , m_pageManager(pageManager)
    , m_services(services)
    , m_languageService(languageService)
{
    setModal(false);
    setWindowFlags(
        Qt::Tool
        | Qt::WindowStaysOnTopHint
        | Qt::WindowDoesNotAcceptFocus
        );
    setAttribute(Qt::WA_ShowWithoutActivating, true);
    setFocusPolicy(Qt::NoFocus);
    setObjectName(QStringLiteral("memoryUsageDialog"));

    MemoryUsageDiagnostics::enable();
    buildUi();
}

void MemoryUsageDialog::retranslateUi()
{
    setWindowTitle(tr("Memory Usage Monitor"));
    m_captureBaselineButton->setText(tr("Capture baseline"));
    m_resetPeakButton->setText(tr("Reset peak"));
#if defined(Q_OS_WIN) && defined(QT_DEBUG)
    m_trimWorkingSetButton->setText(tr("Trim Working Set"));
#endif
    m_markerButton->setText(tr("Add marker..."));
    m_copySummaryButton->setText(tr("Copy summary"));
    m_exportJsonButton->setText(tr("Export JSON..."));
    m_closeButton->setText(tr("Close"));
    m_attributionHeading->setText(tr("Attributed retained memory"));
    m_applicationHealthHeading->setText(tr("Application health"));
    m_pageActionLabel->setText(tr("Page lifecycle actions:"));
    m_openPageButton->setText(tr("Show page"));
    m_releasePdfButton->setText(tr("Release PDF"));
}

void MemoryUsageDialog::refreshNow()
{
    if (!m_provider)
    {
        return;
    }

    m_latestSnapshot = m_provider->snapshot();

    if (!m_latestSnapshot.capturedAt.isValid())
    {
        m_latestSnapshot.capturedAt = QDateTime::currentDateTime();
    }

    if (m_latestSnapshot.isAvailable)
    {
        m_peakWorkingSetSinceReset = qMax(
            m_peakWorkingSetSinceReset,
            m_latestSnapshot.workingSetBytes
            );
    }

    MemoryUsageDiagnostics::recordSnapshot(m_latestSnapshot);
    updateSnapshotLabels();
    updateHistoryText();
    updateAttributionText();
    updateApplicationHealthText();
}

void MemoryUsageDialog::captureBaseline()
{
    refreshNow();

    if (m_latestSnapshot.isAvailable)
    {
        m_baselineSnapshot = m_latestSnapshot;
        m_hasBaseline = true;
    }

    updateSnapshotLabels();
}

void MemoryUsageDialog::resetPeak()
{
    m_peakWorkingSetSinceReset = m_latestSnapshot.isAvailable
        ? m_latestSnapshot.workingSetBytes
        : 0;
    updateSnapshotLabels();
}

void MemoryUsageDialog::recordMarker(const QString& marker)
{
    const QString trimmedMarker = marker.trimmed();

    if (trimmedMarker.isEmpty())
    {
        return;
    }

    MemoryUsageDiagnostics::recordEvent(
        QStringLiteral("marker"),
        trimmedMarker
        );
    updateHistoryText();
}

QString MemoryUsageDialog::summaryText() const
{
    if (!m_latestSnapshot.isAvailable)
    {
        return tr("ClassMngr process memory metrics are unavailable on this platform.");
    }

    QStringList lines{
        tr("ClassMngr memory summary"),
        tr("Captured: %1").arg(
            m_latestSnapshot.capturedAt.toString(Qt::ISODateWithMs)
            ),
        tr("Working set: %1").arg(metricText(m_latestSnapshot.workingSetBytes)),
        tr("Peak working set since reset: %1").arg(metricText(m_peakWorkingSetSinceReset)),
        tr("Process peak working set: %1").arg(metricText(m_latestSnapshot.peakWorkingSetBytes)),
        tr("Private usage: %1").arg(metricText(m_latestSnapshot.privateUsageBytes)),
        tr("Pagefile usage: %1").arg(metricText(m_latestSnapshot.pagefileUsageBytes)),
        tr("Handles: %1").arg(m_latestSnapshot.handleCount),
        tr("Threads: %1").arg(m_latestSnapshot.threadCount)
    };

    if (m_hasBaseline)
    {
        lines.append(
            tr("Working-set delta from baseline: %1")
                .arg(
                    deltaText(
                        m_latestSnapshot.workingSetBytes,
                        m_baselineSnapshot.workingSetBytes
                        )
                    )
            );
    }

    const QList<MemoryBreakdownEntry> attribution =
        MemoryUsageDiagnostics::collectMemoryBreakdown();
    quint64 attributedBytes = 0;
    for (const MemoryBreakdownEntry& entry : attribution)
    {
        attributedBytes += entry.retainedBytes;
    }
    lines.append(
        tr("Attributed retained memory (partial estimate): %1")
            .arg(metricText(attributedBytes))
        );
    lines.append(QString());
    lines.append(applicationHealthText());

    QStringList recentEvents;
    const QList<MemoryUsageHistoryEntry>& historyEntries =
        history().entries();
    for (
        int index = qMax(0, historyEntries.size() - 20);
        index < historyEntries.size();
        ++index
        )
    {
        const MemoryUsageHistoryEntry& entry = historyEntries.at(index);
        if (entry.kind != MemoryUsageHistoryEntryKind::Event)
        {
            continue;
        }

        recentEvents.append(
            QStringLiteral("%1  %2%3")
                .arg(
                    entry.snapshot.capturedAt.toString(QStringLiteral("HH:mm:ss")),
                    entry.eventType,
                    entry.eventDetail.isEmpty()
                        ? QString()
                        : QStringLiteral(": %1").arg(entry.eventDetail)
                    )
            );
    }
    if (!recentEvents.isEmpty())
    {
        lines.append(QString());
        lines.append(tr("Recent diagnostic events:"));
        lines.append(recentEvents.join(QLatin1Char('\n')));
    }

    return lines.join(QLatin1Char('\n'));
}

const MemoryUsageHistory& MemoryUsageDialog::history() const
{
    return MemoryUsageDiagnostics::history();
}

void MemoryUsageDialog::showEvent(QShowEvent* event)
{
    if (!m_geometryRestored)
    {
        restoreSavedGeometry();
        m_geometryRestored = true;
    }

    QDialog::showEvent(event);
    refreshNow();
    m_timer->start();
}

void MemoryUsageDialog::hideEvent(QHideEvent* event)
{
    m_timer->stop();
    QDialog::hideEvent(event);
}

void MemoryUsageDialog::closeEvent(QCloseEvent* event)
{
    persistGeometry();
    QDialog::closeEvent(event);
}

void MemoryUsageDialog::buildUi()
{
    auto* layout = new QVBoxLayout(this);
    auto* metrics = new QFormLayout;

    const auto addMetric =
        [this, metrics](const QString& label, const QString& objectName, const QString& toolTip)
        {
            auto* value = new QLabel(this);
            value->setObjectName(objectName);
            value->setTextInteractionFlags(Qt::TextSelectableByMouse);
            value->setToolTip(toolTip);
            configureNoFocus(value);
            metrics->addRow(label, value);
            return value;
        };

    m_workingSetValue = addMetric(
        tr("Working set (current):"),
        QStringLiteral("memoryUsageWorkingSet"),
        tr("Windows PROCESS_MEMORY_COUNTERS_EX::WorkingSetSize")
        );
    m_peakWorkingSetValue = addMetric(
        tr("Peak working set (since reset):"),
        QStringLiteral("memoryUsagePeakWorkingSet"),
        tr("Highest sampled working set since this monitor was reset")
        );
    m_privateUsageValue = addMetric(
        tr("Private usage:"),
        QStringLiteral("memoryUsagePrivateUsage"),
        tr("Windows PROCESS_MEMORY_COUNTERS_EX::PrivateUsage")
        );
    m_pagefileUsageValue = addMetric(
        tr("Pagefile usage:"),
        QStringLiteral("memoryUsagePagefileUsage"),
        tr("Windows PROCESS_MEMORY_COUNTERS_EX::PagefileUsage")
        );
    m_handleCountValue = addMetric(
        tr("Process handles:"),
        QStringLiteral("memoryUsageHandleCount"),
        tr("Windows GetProcessHandleCount")
        );
    m_threadCountValue = addMetric(
        tr("Process threads:"),
        QStringLiteral("memoryUsageThreadCount"),
        tr("Threads owned by this process")
        );
    m_capturedAtValue = addMetric(
        tr("Captured:"),
        QStringLiteral("memoryUsageCapturedAt"),
        QString()
        );
    m_baselineDeltaValue = addMetric(
        tr("Working-set delta:"),
        QStringLiteral("memoryUsageBaselineDelta"),
        QString()
        );
    layout->addLayout(metrics);

    auto* controls = new QHBoxLayout;
    auto* intervalLabel = new QLabel(tr("Refresh:"), this);
    configureNoFocus(intervalLabel);
    controls->addWidget(intervalLabel);
    m_refreshInterval = new QComboBox(this);
    m_refreshInterval->setObjectName(QStringLiteral("memoryUsageRefreshInterval"));
    m_refreshInterval->addItem(tr("0.5 seconds"), 500);
    m_refreshInterval->addItem(tr("1 second"), DefaultRefreshIntervalMs);
    m_refreshInterval->addItem(tr("2 seconds"), 2000);
    m_refreshInterval->addItem(tr("5 seconds"), 5000);
    m_refreshInterval->setCurrentIndex(1);
    configureNoFocus(m_refreshInterval);
    controls->addWidget(m_refreshInterval);
    controls->addStretch();
    layout->addLayout(controls);

    auto* historyLabel = new QLabel(
        tr("Recent timeline and diagnostic events (up to 10 minutes):"),
        this
        );
    configureNoFocus(historyLabel);
    layout->addWidget(historyLabel);
    m_historyText = new QPlainTextEdit(this);
    m_historyText->setObjectName(QStringLiteral("memoryUsageHistory"));
    m_historyText->setReadOnly(true);
    m_historyText->setMinimumHeight(120);
    configureNoFocus(m_historyText);
    layout->addWidget(m_historyText);

    m_attributionHeading = new QLabel(this);
    m_attributionHeading->setObjectName(QStringLiteral("memoryUsageAttributionHeading"));
    configureNoFocus(m_attributionHeading);
    layout->addWidget(m_attributionHeading);
    m_attributionSummary = new QLabel(this);
    m_attributionSummary->setObjectName(QStringLiteral("memoryUsageAttributionSummary"));
    m_attributionSummary->setWordWrap(true);
    configureNoFocus(m_attributionSummary);
    layout->addWidget(m_attributionSummary);
    m_attributionText = new QPlainTextEdit(this);
    m_attributionText->setObjectName(QStringLiteral("memoryUsageAttribution"));
    m_attributionText->setReadOnly(true);
    m_attributionText->setMinimumHeight(140);
    configureNoFocus(m_attributionText);
    layout->addWidget(m_attributionText);

    m_applicationHealthHeading = new QLabel(this);
    m_applicationHealthHeading->setObjectName(
        QStringLiteral("memoryUsageApplicationHealthHeading")
        );
    configureNoFocus(m_applicationHealthHeading);
    layout->addWidget(m_applicationHealthHeading);
    m_applicationHealthText = new QPlainTextEdit(this);
    m_applicationHealthText->setObjectName(
        QStringLiteral("memoryUsageApplicationHealth")
        );
    m_applicationHealthText->setReadOnly(true);
    m_applicationHealthText->setMinimumHeight(130);
    configureNoFocus(m_applicationHealthText);
    layout->addWidget(m_applicationHealthText);

    auto* pageActions = new QHBoxLayout;
    m_pageActionLabel = new QLabel(this);
    configureNoFocus(m_pageActionLabel);
    pageActions->addWidget(m_pageActionLabel);
    m_pageActionPage = new QComboBox(this);
    m_pageActionPage->setObjectName(QStringLiteral("memoryUsagePageActionPage"));
    for (const PageType type : {
             PageType::MyWorkspace,
             PageType::MyClasses,
             PageType::Schedule,
             PageType::Classes,
             PageType::TestingClasses,
             PageType::TeacherInfo,
             PageType::NativeEnglishTeachers,
             PageType::GsTeam,
             PageType::CampusDashboard,
             PageType::SubPrep,
             PageType::PdfViewer
         })
    {
        m_pageActionPage->addItem(
            PageManager::pageTypeIdentifier(type),
            static_cast<int>(type)
            );
    }
    configureNoFocus(m_pageActionPage);
    pageActions->addWidget(m_pageActionPage);
    m_openPageButton = new QPushButton(this);
    m_openPageButton->setObjectName(QStringLiteral("memoryUsageShowPageButton"));
    configureNoFocus(m_openPageButton);
    pageActions->addWidget(m_openPageButton);
    m_releasePdfButton = new QPushButton(this);
    m_releasePdfButton->setObjectName(QStringLiteral("memoryUsageReleasePdfButton"));
    configureNoFocus(m_releasePdfButton);
    pageActions->addWidget(m_releasePdfButton);
    pageActions->addStretch();
    layout->addLayout(pageActions);

    auto* buttons = new QHBoxLayout;
    const auto addButton =
        [this, buttons](const QString& objectName)
        {
            auto* button = new QPushButton(this);
            button->setObjectName(objectName);
            configureNoFocus(button);
            buttons->addWidget(button);
            return button;
        };

    m_captureBaselineButton = addButton(QStringLiteral("memoryUsageCaptureBaselineButton"));
    m_resetPeakButton = addButton(QStringLiteral("memoryUsageResetPeakButton"));
#if defined(Q_OS_WIN) && defined(QT_DEBUG)
    m_trimWorkingSetButton = addButton(QStringLiteral("memoryUsageTrimWorkingSetButton"));
#endif
    m_markerButton = addButton(QStringLiteral("memoryUsageAddMarkerButton"));
    m_copySummaryButton = addButton(QStringLiteral("memoryUsageCopySummaryButton"));
    m_exportJsonButton = addButton(QStringLiteral("memoryUsageExportJsonButton"));
    buttons->addStretch();
    m_closeButton = addButton(QStringLiteral("memoryUsageCloseButton"));
    layout->addLayout(buttons);

    m_timer = new QTimer(this);
    m_timer->setInterval(DefaultRefreshIntervalMs);

    connect(m_timer, &QTimer::timeout, this, &MemoryUsageDialog::refreshNow);
    connect(m_refreshInterval, &QComboBox::currentIndexChanged, this, &MemoryUsageDialog::updateTimerInterval);
    connect(m_captureBaselineButton, &QPushButton::clicked, this, &MemoryUsageDialog::captureBaseline);
    connect(m_resetPeakButton, &QPushButton::clicked, this, &MemoryUsageDialog::resetPeak);
#if defined(Q_OS_WIN) && defined(QT_DEBUG)
    connect(m_trimWorkingSetButton, &QPushButton::clicked, this, []
        {
            SetProcessWorkingSetSize(
                GetCurrentProcess(),
                static_cast<SIZE_T>(-1),
                static_cast<SIZE_T>(-1)
                );
        }
        );
#endif
    connect(m_markerButton, &QPushButton::clicked, this, &MemoryUsageDialog::promptForMarker);
    connect(m_copySummaryButton, &QPushButton::clicked, this, &MemoryUsageDialog::copySummary);
    connect(m_exportJsonButton, &QPushButton::clicked, this, &MemoryUsageDialog::exportJson);
    connect(m_closeButton, &QPushButton::clicked, this, &MemoryUsageDialog::close);
    connect(m_openPageButton, &QPushButton::clicked, this, &MemoryUsageDialog::navigateToSelectedPage);
    connect(m_releasePdfButton, &QPushButton::clicked, this, &MemoryUsageDialog::releasePdfDocument);

    retranslateUi();
    updateAttributionText();
    updateApplicationHealthText();
    resize(760, 900);
}

void MemoryUsageDialog::updateSnapshotLabels()
{
    const bool available = m_latestSnapshot.isAvailable;
    m_workingSetValue->setText(available ? metricText(m_latestSnapshot.workingSetBytes) : tr("Unavailable"));
    m_peakWorkingSetValue->setText(available ? metricText(m_peakWorkingSetSinceReset) : tr("Unavailable"));
    m_privateUsageValue->setText(available ? metricText(m_latestSnapshot.privateUsageBytes) : tr("Unavailable"));
    m_pagefileUsageValue->setText(available ? metricText(m_latestSnapshot.pagefileUsageBytes) : tr("Unavailable"));
    m_handleCountValue->setText(available ? QString::number(m_latestSnapshot.handleCount) : tr("Unavailable"));
    m_threadCountValue->setText(available ? QString::number(m_latestSnapshot.threadCount) : tr("Unavailable"));
    m_capturedAtValue->setText(
        m_latestSnapshot.capturedAt.isValid()
            ? m_latestSnapshot.capturedAt.toString(Qt::ISODateWithMs)
            : tr("Unavailable")
        );
    m_baselineDeltaValue->setText(
        available && m_hasBaseline
            ? deltaText(
                m_latestSnapshot.workingSetBytes,
                m_baselineSnapshot.workingSetBytes
                )
            : tr("No baseline")
        );
}

void MemoryUsageDialog::updateHistoryText()
{
    const QList<MemoryUsageHistoryEntry>& entries = history().entries();
    const int first = qMax(0, entries.size() - VisibleHistoryEntries);
    QStringList lines;
    lines.reserve(entries.size() - first);

    for (int index = first; index < entries.size(); ++index)
    {
        const MemoryUsageHistoryEntry& entry = entries.at(index);
        const QString time = entry.snapshot.capturedAt.toString(QStringLiteral("HH:mm:ss"));

        if (entry.kind == MemoryUsageHistoryEntryKind::Sample)
        {
            lines.append(
                entry.snapshot.isAvailable
                    ? QStringLiteral("%1  sample  working set %2; private %3")
                        .arg(
                            time,
                            metricText(entry.snapshot.workingSetBytes),
                            metricText(entry.snapshot.privateUsageBytes)
                            )
                    : QStringLiteral("%1  sample  unavailable").arg(time)
                );
        }
        else
        {
            lines.append(
                QStringLiteral("%1  %2%3")
                    .arg(
                        time,
                        entry.eventType,
                        entry.eventDetail.isEmpty()
                            ? QString()
                            : QStringLiteral(": %1").arg(entry.eventDetail)
                        )
                );
        }
    }

    m_historyText->setPlainText(lines.join(QLatin1Char('\n')));
    m_historyText->verticalScrollBar()->setValue(
        m_historyText->verticalScrollBar()->maximum()
        );
}

void MemoryUsageDialog::updateAttributionText()
{
    const QList<MemoryBreakdownEntry> attribution =
        MemoryUsageDiagnostics::collectMemoryBreakdown();
    quint64 attributedBytes = 0;
    QStringList lines;

    for (const MemoryBreakdownEntry& entry : attribution)
    {
        attributedBytes += entry.retainedBytes;
        const QString byteText = entry.retainedBytes == 0
            ? tr("no byte estimate")
            : metricText(entry.retainedBytes);
        lines.append(
            QStringLiteral("%1 — %2: %3%4; items=%5\n  %6")
                .arg(
                    entry.owner,
                    entry.category,
                    entry.isEstimated ? QStringLiteral("~") : QString(),
                    byteText,
                    QString::number(entry.itemCount),
                    entry.detail
                    )
            );
    }

    if (lines.isEmpty())
    {
        lines.append(tr("No feature-owned retained resources are instantiated yet."));
    }

    const QString comparison = !m_latestSnapshot.isAvailable
        ? tr("Unavailable because process private usage is unavailable.")
        : attributedBytes <= m_latestSnapshot.privateUsageBytes
              ? metricText(m_latestSnapshot.privateUsageBytes - attributedBytes)
              : tr("Not shown because the partial estimates exceed current private usage.");
    m_attributionSummary->setText(
        tr("Attributed retained memory: %1 across %2 entries. "
           "These feature-owned estimates are partial and do not equal process private usage. "
           "Unattributed/shared/runtime comparison: %3")
            .arg(
                metricText(attributedBytes),
                QString::number(attribution.size()),
                comparison
                )
        );

    const QList<PageLifecycleEntry> lifecycle =
        MemoryUsageDiagnostics::collectPageLifecycle();
    if (!lifecycle.isEmpty())
    {
        lines.append(QString());
        lines.append(tr("Page lifecycle:"));

        for (const PageLifecycleEntry& entry : lifecycle)
        {
            QString state;
            switch (entry.state)
            {
            case PageLifecycleState::Uncreated:
                state = tr("uncreated");
                break;
            case PageLifecycleState::Hidden:
                state = tr("instantiated, hidden");
                break;
            case PageLifecycleState::Current:
                state = tr("current");
                break;
            }

            lines.append(
                QStringLiteral("%1 — %2; created=%3; last active=%4")
                    .arg(
                        entry.pageIdentifier,
                        state,
                        entry.createdAt.isValid()
                            ? entry.createdAt.toString(Qt::ISODateWithMs)
                            : tr("never"),
                        entry.lastActivatedAt.isValid()
                            ? entry.lastActivatedAt.toString(Qt::ISODateWithMs)
                            : tr("never")
                        )
                );
        }
    }

    m_attributionText->setPlainText(lines.join(QLatin1Char('\n')));
    m_releasePdfButton->setEnabled(
        m_pageManager
        && m_pageManager->pdfViewerPage()
        && m_pageManager->pdfViewerPage()->hasLoadedDocument()
        );
    m_openPageButton->setEnabled(m_pageManager != nullptr);
}

void MemoryUsageDialog::updateApplicationHealthText()
{
    if (m_applicationHealthText)
    {
        m_applicationHealthText->setPlainText(applicationHealthText());
    }
}

QString MemoryUsageDialog::applicationHealthText() const
{
    QStringList lines{
        tr("Application version: %1").arg(QCoreApplication::applicationVersion()),
        tr("Build: revision=%1; timestamp=%2; flags=%3")
            .arg(
                QString::fromUtf8(BuildInfo::GitRevision),
                QString::fromUtf8(BuildInfo::BuildTimestamp),
                buildFlagsText()
                ),
        tr("Runtime: Qt %1; OS=%2; CPU=%3")
            .arg(
                QString::fromLatin1(qVersion()),
                QSysInfo::prettyProductName(),
                QSysInfo::currentCpuArchitecture()
                ),
        tr("Database: %1")
            .arg(
                m_services
                    ? (m_services->hasOpenDatabase()
                           ? tr("open")
                           : tr("closed"))
                    : tr("unavailable")
                ),
        tr("Current page: %1")
            .arg(
                m_pageManager
                    ? m_pageManager->currentPageIdentifier()
                    : tr("unavailable")
                ),
        tr("Language: %1; theme: %2")
            .arg(
                m_languageService
                    ? m_languageService->loadedLocaleName()
                    : tr("unavailable"),
                m_services && m_services->themeService()
                    ? themeText(m_services->themeService()->currentTheme())
                    : tr("unavailable")
                ),
        tr("Display scale: %1")
            .arg(
                QGuiApplication::primaryScreen()
                    ? QString::number(
                          QGuiApplication::primaryScreen()->devicePixelRatio(),
                          'f',
                          2
                          )
                    : tr("unavailable")
                ),
        tr("Memory policies: Calendar visible/30-day/prefetch/next-ten retention; "
           "Campus decoded-image cap %1 px")
            .arg(CampusMapPreview::MaximumDecodedImageDimension)
    };

    const QList<DeveloperBackgroundTask> tasks =
        MemoryUsageDiagnostics::activeBackgroundTasks();
    lines.append(
        tr("Active background tasks: %1")
            .arg(tasks.size())
        );
    for (const DeveloperBackgroundTask& task : tasks)
    {
        lines.append(
            QStringLiteral("  %1 — %2; started=%3; cancellation=%4")
                .arg(
                    task.category,
                    task.name,
                    task.startedAt.toString(Qt::ISODateWithMs),
                    task.cancellable
                        ? (task.cancellationRequested
                               ? tr("requested")
                               : tr("available"))
                        : tr("not supported")
                    )
            );
    }

    return lines.join(QLatin1Char('\n'));
}

void MemoryUsageDialog::updateTimerInterval(int index)
{
    const int interval = m_refreshInterval->itemData(index).toInt();
    m_timer->setInterval(interval > 0 ? interval : DefaultRefreshIntervalMs);
}

void MemoryUsageDialog::promptForMarker()
{
    QWidget* promptParent = parentWidget() ? parentWidget() : this;
    bool accepted = false;
    const QString marker = QInputDialog::getText(
        promptParent,
        tr("Add memory marker"),
        tr("Marker:"),
        QLineEdit::Normal,
        QString(),
        &accepted
        );

    if (accepted)
    {
        recordMarker(marker);
    }
}

void MemoryUsageDialog::copySummary()
{
    if (QClipboard* clipboard = QGuiApplication::clipboard())
    {
        clipboard->setText(summaryText());
    }
}

void MemoryUsageDialog::exportJson()
{
    const std::optional<QString> selection =
        DialogServices::fileDialogs().saveFile(
            SaveFileRequest{
                .parent = parentWidget() ? parentWidget() : this,
                .title = tr("Export memory diagnostics"),
                .purpose = FileDialogPurpose::ExportReport,
                .suggestedFileName =
                    QStringLiteral("classmngr-memory-diagnostics.json"),
                .nameFilters = {tr("JSON Files (*.json)")},
                .defaultSuffix = QStringLiteral("json")
            }
            );

    if (!selection)
    {
        return;
    }

    QSaveFile file(*selection);
    if (!file.open(QIODevice::WriteOnly))
    {
        return;
    }

    QJsonObject document = history().toJson().object();
    QJsonArray attribution;
    quint64 attributedBytes = 0;
    for (const MemoryBreakdownEntry& entry :
         MemoryUsageDiagnostics::collectMemoryBreakdown())
    {
        attributedBytes += entry.retainedBytes;
        attribution.append(
            QJsonObject{
                {QStringLiteral("category"), entry.category},
                {QStringLiteral("owner"), entry.owner},
                {QStringLiteral("retainedBytes"), static_cast<double>(entry.retainedBytes)},
                {QStringLiteral("itemCount"), static_cast<double>(entry.itemCount)},
                {QStringLiteral("detail"), entry.detail},
                {QStringLiteral("estimated"), entry.isEstimated}
            }
            );
    }
    document.insert(QStringLiteral("attributedRetainedBytes"), static_cast<double>(attributedBytes));
    document.insert(QStringLiteral("attribution"), attribution);

    QJsonArray lifecycle;
    for (const PageLifecycleEntry& entry :
         MemoryUsageDiagnostics::collectPageLifecycle())
    {
        lifecycle.append(
            QJsonObject{
                {QStringLiteral("page"), entry.pageIdentifier},
                {QStringLiteral("state"),
                 entry.state == PageLifecycleState::Uncreated
                     ? QStringLiteral("uncreated")
                     : (entry.state == PageLifecycleState::Current
                            ? QStringLiteral("current")
                            : QStringLiteral("hidden"))},
                {QStringLiteral("createdAt"), entry.createdAt.toString(Qt::ISODateWithMs)},
                {QStringLiteral("lastActivatedAt"), entry.lastActivatedAt.toString(Qt::ISODateWithMs)}
            }
            );
    }
    document.insert(QStringLiteral("pageLifecycle"), lifecycle);
    document.insert(
        QStringLiteral("applicationHealth"),
        applicationHealthText()
        );

    QJsonArray backgroundTasks;
    for (const DeveloperBackgroundTask& task :
         MemoryUsageDiagnostics::activeBackgroundTasks())
    {
        backgroundTasks.append(
            QJsonObject{
                {QStringLiteral("category"), task.category},
                {QStringLiteral("name"), task.name},
                {QStringLiteral("startedAt"), task.startedAt.toString(Qt::ISODateWithMs)},
                {QStringLiteral("cancellable"), task.cancellable},
                {QStringLiteral("cancellationRequested"), task.cancellationRequested}
            }
            );
    }
    document.insert(QStringLiteral("activeBackgroundTasks"), backgroundTasks);

    file.write(QJsonDocument(document).toJson(QJsonDocument::Indented));
    file.commit();
}

void MemoryUsageDialog::navigateToSelectedPage()
{
    if (!m_pageManager)
    {
        return;
    }

    if (!m_pageManager->confirmCurrentPageCanLeave())
    {
        return;
    }

    m_pageManager->showPage(
        static_cast<PageType>(m_pageActionPage->currentData().toInt())
        );
    updateAttributionText();
    updateApplicationHealthText();
}

void MemoryUsageDialog::releasePdfDocument()
{
    if (m_pageManager && m_pageManager->pdfViewerPage())
    {
        m_pageManager->pdfViewerPage()->releaseDocument();
    }

    updateAttributionText();
    updateApplicationHealthText();
}

void MemoryUsageDialog::restoreSavedGeometry()
{
    const QByteArray geometry = SettingsManager::instance()
                                    .get(QString::fromUtf8(GeometrySettingsKey))
                                    .toByteArray();

    if (!geometry.isEmpty())
    {
        restoreGeometry(geometry);
    }
}

void MemoryUsageDialog::persistGeometry() const
{
    if (m_geometryRestored)
    {
        SettingsManager::instance().set(
            QString::fromUtf8(GeometrySettingsKey),
            saveGeometry()
            );
    }
}

void MemoryUsageDialog::configureNoFocus(QWidget* widget) const
{
    if (widget)
    {
        widget->setFocusPolicy(Qt::NoFocus);
    }
}

QString MemoryUsageDialog::metricText(quint64 value) const
{
    return MemoryUsageMetrics::formatBytes(value);
}

QString MemoryUsageDialog::deltaText(
    quint64 value,
    quint64 baseline
    ) const
{
    const MemoryUsageDelta delta = MemoryUsageMetrics::calculateDelta(value, baseline);
    return delta.percentageAvailable
        ? tr("%1 (%2%)")
            .arg(
                signedBytesText(delta.absoluteBytes),
                QString::number(delta.percentage, 'f', 1)
                )
        : tr("%1 (baseline is zero)").arg(signedBytesText(delta.absoluteBytes));
}
