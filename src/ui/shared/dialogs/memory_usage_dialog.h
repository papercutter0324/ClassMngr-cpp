#pragma once

#include "core/memory_usage_diagnostics.h"

#include <QDialog>

#include <memory>

class QComboBox;
class QLabel;
class QPlainTextEdit;
class QPushButton;
class QShowEvent;
class QHideEvent;
class QCloseEvent;
class QTimer;
class PageManager;
class ApplicationServices;
class LanguageService;

// A developer-only, modeless process monitor. It deliberately never accepts
// keyboard focus so showing or clicking it cannot interrupt the active editor.
class MemoryUsageDialog final : public QDialog
{
    Q_OBJECT

public:
    explicit MemoryUsageDialog(
        QWidget* parent = nullptr,
        PageManager* pageManager = nullptr,
        std::unique_ptr<ProcessMemorySnapshotProvider> provider = nullptr,
        ApplicationServices* services = nullptr,
        LanguageService* languageService = nullptr
        );

    void retranslateUi();
    void refreshNow();
    void captureBaseline();
    void resetPeak();
    void recordMarker(const QString& marker);

    [[nodiscard]] QString summaryText() const;
    [[nodiscard]] const MemoryUsageHistory& history() const;

protected:
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    void closeEvent(QCloseEvent* event) override;

private:
    void buildUi();
    void updateSnapshotLabels();
    void updateHistoryText();
    void updateAttributionText();
    void updateApplicationHealthText();
    void updateTimerInterval(int index);
    void promptForMarker();
    void copySummary();
    void exportJson();
    void navigateToSelectedPage();
    void releasePdfDocument();
    void restoreSavedGeometry();
    void persistGeometry() const;
    void configureNoFocus(QWidget* widget) const;
    [[nodiscard]] QString metricText(quint64 value) const;
    [[nodiscard]] QString deltaText(
        quint64 value,
        quint64 baseline
        ) const;
    [[nodiscard]] QString applicationHealthText() const;

    std::unique_ptr<ProcessMemorySnapshotProvider> m_provider;
    PageManager* m_pageManager = nullptr;
    ApplicationServices* m_services = nullptr;
    LanguageService* m_languageService = nullptr;
    ProcessMemorySnapshot m_latestSnapshot;
    ProcessMemorySnapshot m_baselineSnapshot;
    bool m_hasBaseline = false;
    quint64 m_peakWorkingSetSinceReset = 0;
    bool m_geometryRestored = false;

    QTimer* m_timer = nullptr;
    QLabel* m_workingSetValue = nullptr;
    QLabel* m_peakWorkingSetValue = nullptr;
    QLabel* m_privateUsageValue = nullptr;
    QLabel* m_pagefileUsageValue = nullptr;
    QLabel* m_handleCountValue = nullptr;
    QLabel* m_threadCountValue = nullptr;
    QLabel* m_capturedAtValue = nullptr;
    QLabel* m_baselineDeltaValue = nullptr;
    QLabel* m_attributionHeading = nullptr;
    QLabel* m_attributionSummary = nullptr;
    QLabel* m_pageActionLabel = nullptr;
    QLabel* m_applicationHealthHeading = nullptr;
    QComboBox* m_refreshInterval = nullptr;
    QPlainTextEdit* m_historyText = nullptr;
    QPlainTextEdit* m_attributionText = nullptr;
    QPlainTextEdit* m_applicationHealthText = nullptr;
    QComboBox* m_pageActionPage = nullptr;
    QPushButton* m_captureBaselineButton = nullptr;
    QPushButton* m_resetPeakButton = nullptr;
#if defined(Q_OS_WIN) && defined(QT_DEBUG)
    QPushButton* m_trimWorkingSetButton = nullptr;
#endif
    QPushButton* m_markerButton = nullptr;
    QPushButton* m_copySummaryButton = nullptr;
    QPushButton* m_exportJsonButton = nullptr;
    QPushButton* m_closeButton = nullptr;
    QPushButton* m_openPageButton = nullptr;
    QPushButton* m_releasePdfButton = nullptr;
};
