#pragma once

#include <QPointer>
#include <QString>

#include <memory>

class QDialog;
class QWidget;
class ApplicationServices;

enum class SubPrepOutputDialogAutomationStatus
{
    NotFound,
    ControlsMissing,
    ValidationStateFailed,
    DialogCaptureFailed,
    Accepted
};

struct SubPrepOutputDialogAutomationResult
{
    SubPrepOutputDialogAutomationStatus status =
        SubPrepOutputDialogAutomationStatus::NotFound;
    QString validationMessage;
    bool validationButtonEnabled = false;
    bool validationCaptureSucceeded = false;
    bool dialogCaptureSucceeded = false;
};

class QtSubPrepWorkflowTestAdapter final
{
public:
    [[nodiscard]] static SubPrepOutputDialogAutomationResult configureAndAccept(
        const QString& targetFolder,
        const QString& userName,
        const QString& validationCapturePath,
        const QString& dialogCapturePath
        );
};

struct CalendarPreferencesUiSnapshot
{
    bool dialogFound = false;
    bool controlsReady = false;
    int calendarTabIndex = -1;
    QString statusText;
    bool statusVisible = false;
    bool importButtonEnabled = false;
    bool loadingCaptureAttempted = false;
    bool loadingControlsDisabled = false;
    bool loadingCaptureSucceeded = false;
    bool failureCaptureSucceeded = false;
};

class QtCalendarImportWorkflowTestAdapter final
{
public:
    [[nodiscard]] bool hasPreferencesAction(
        const QWidget& mainWindow
        ) const;

    [[nodiscard]] bool triggerPreferences(
        QWidget& mainWindow
        ) const;

    [[nodiscard]] CalendarPreferencesUiSnapshot selectCalendarTab(
        QWidget& mainWindow
        );

    [[nodiscard]] CalendarPreferencesUiSnapshot startImport(
        const QString& outputRoot
        );

    [[nodiscard]] CalendarPreferencesUiSnapshot observeImport(
        const QString& outputRoot
        );

    [[nodiscard]] CalendarPreferencesUiSnapshot observeFailure(
        const QString& outputRoot
        );

    [[nodiscard]] bool capturePreferences(
        const QString& path
        ) const;

    [[nodiscard]] bool captureCalendarPage(
        QWidget& page,
        const QString& path
        ) const;

    [[nodiscard]] bool closePreferences();

private:
    [[nodiscard]] QDialog* findPreferencesDialog(QWidget& mainWindow);

    QPointer<QDialog> m_preferencesDialog;
    bool m_loadingCaptureAttempted = false;
};

struct ScheduleImportSourceSnapshot
{
    bool dialogOpen = false;
    bool statusReady = false;
    QString statusText;
    bool progressVisible = false;
    bool selectionReady = false;
    int selectedUserIndex = -1;
    bool nextAvailable = false;
    bool nextEnabled = false;
    bool loadingCaptureAttempted = false;
    bool loadingProgressVisible = false;
    bool loadingControlsDisabled = false;
    bool loadingCaptureSucceeded = false;
};

struct ScheduleImportReviewSnapshot
{
    bool reviewFound = false;
    bool importButtonFound = false;
    bool reviewVisible = false;
    bool importEnabled = false;
};

struct ScheduleImportReviewCaptures
{
    bool sourceCaptured = false;
    bool reviewCaptured = false;
};

class QtScheduleImportWorkflowTestAdapter final
{
public:
    [[nodiscard]] bool open(
        ApplicationServices* services,
        QWidget& parent,
        const QString& filePath
        );

    [[nodiscard]] ScheduleImportSourceSnapshot beginSourceLoad(
        const QString& outputRoot
        );

    [[nodiscard]] ScheduleImportSourceSnapshot observeSource(
        const QString& outputRoot
        );

    [[nodiscard]] bool beginReview();

    [[nodiscard]] ScheduleImportReviewSnapshot inspectReview() const;

    void acknowledgeWarnings();

    [[nodiscard]] ScheduleImportReviewCaptures captureReview(
        const QString& outputRoot
        );

    [[nodiscard]] bool clickImport();

    [[nodiscard]] bool sourceDialogVisible() const;

    [[nodiscard]] bool rejectReview();

    [[nodiscard]] bool rejectSourceDialog();

private:
    class State;
    std::shared_ptr<State> m_state;
};
