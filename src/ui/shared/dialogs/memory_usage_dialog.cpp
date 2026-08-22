#include "memory_usage_dialog.h"

#include "core/settingsmanager.h"

#include <QClipboard>
#include <QCloseEvent>
#include <QComboBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QHideEvent>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSaveFile>
#include <QShowEvent>
#include <QScrollBar>
#include <QTimer>
#include <QVBoxLayout>

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
}

MemoryUsageDialog::MemoryUsageDialog(
    QWidget* parent,
    std::unique_ptr<ProcessMemorySnapshotProvider> provider
    )
    : QDialog(parent)
    , m_provider(
          provider
              ? std::move(provider)
              : std::make_unique<PlatformProcessMemorySnapshotProvider>()
          )
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
    m_markerButton->setText(tr("Add marker..."));
    m_copySummaryButton->setText(tr("Copy summary"));
    m_exportJsonButton->setText(tr("Export JSON..."));
    m_closeButton->setText(tr("Close"));
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

    auto* historyLabel = new QLabel(tr("Recent history (up to 10 minutes):"), this);
    configureNoFocus(historyLabel);
    layout->addWidget(historyLabel);
    m_historyText = new QPlainTextEdit(this);
    m_historyText->setObjectName(QStringLiteral("memoryUsageHistory"));
    m_historyText->setReadOnly(true);
    m_historyText->setMinimumHeight(150);
    configureNoFocus(m_historyText);
    layout->addWidget(m_historyText);

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
    connect(m_markerButton, &QPushButton::clicked, this, &MemoryUsageDialog::promptForMarker);
    connect(m_copySummaryButton, &QPushButton::clicked, this, &MemoryUsageDialog::copySummary);
    connect(m_exportJsonButton, &QPushButton::clicked, this, &MemoryUsageDialog::exportJson);
    connect(m_closeButton, &QPushButton::clicked, this, &MemoryUsageDialog::close);

    retranslateUi();
    resize(560, 510);
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
    const QString filePath = QFileDialog::getSaveFileName(
        parentWidget() ? parentWidget() : this,
        tr("Export memory diagnostics"),
        QStringLiteral("classmngr-memory-diagnostics.json"),
        tr("JSON Files (*.json)")
        );

    if (filePath.isEmpty())
    {
        return;
    }

    QSaveFile file(filePath);
    if (!file.open(QIODevice::WriteOnly))
    {
        return;
    }

    file.write(history().toJson().toJson(QJsonDocument::Indented));
    file.commit();
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
