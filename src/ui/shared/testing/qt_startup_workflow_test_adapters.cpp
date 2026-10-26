#include "qt_startup_workflow_test_adapters.h"

#include "core/application_services.h"
#include "features/schedule/ui/schedule_import_dialog.h"
#include "features/schedule/ui/schedule_import_review_dialog.h"
#include "features/sub_prep/ui/sub_prep_print_dialog.h"

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDir>
#include <QLabel>
#include <QLineEdit>
#include <QPixmap>
#include <QProgressBar>
#include <QPushButton>
#include <QRadioButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QTabWidget>
#include <QWidget>

namespace
{

SubPrepPrintDialog* activeSubPrepDialog()
{
    if (
        auto* active = qobject_cast<SubPrepPrintDialog*>(
            QApplication::activeModalWidget()
            );
        active
        )
    {
        return active;
    }

    for (QWidget* widget : QApplication::topLevelWidgets())
    {
        if (auto* dialog = qobject_cast<SubPrepPrintDialog*>(widget))
        {
            return dialog;
        }
    }

    return nullptr;
}

bool saveDialogCapture(
    QWidget& widget,
    const QString& path
    )
{
    const QPixmap capture = widget.grab();
    return !capture.isNull() && capture.save(path, "PNG");
}

bool saveWidgetCapture(
    QWidget& widget,
    const QString& path
    )
{
    const QPixmap capture = widget.grab();
    return !capture.isNull() && capture.save(path, "PNG");
}

}

SubPrepOutputDialogAutomationResult
QtSubPrepWorkflowTestAdapter::configureAndAccept(
    const QString& targetFolder,
    const QString& userName,
    const QString& validationCapturePath,
    const QString& dialogCapturePath
    )
{
    SubPrepPrintDialog* const dialog = activeSubPrepDialog();
    if (!dialog)
    {
        return {};
    }

    QCheckBox* const createFolderCheck = dialog->findChild<QCheckBox*>(
        QStringLiteral("subPrepCreateFolderCheckBox")
        );
    QCheckBox* const printPaperCheck = dialog->findChild<QCheckBox*>(
        QStringLiteral("subPrepPrintPaperCopiesCheckBox")
        );
    QPushButton* const okButton = dialog->findChild<QPushButton*>(
        QStringLiteral("subPrepGenerateOkButton")
        );
    QLabel* const validationLabel = dialog->findChild<QLabel*>(
        QStringLiteral("subPrepGenerationValidationLabel")
        );
    if (!createFolderCheck || !printPaperCheck || !okButton || !validationLabel)
    {
        dialog->reject();
        return {
            .status = SubPrepOutputDialogAutomationStatus::ControlsMissing
        };
    }

    createFolderCheck->setChecked(false);
    printPaperCheck->setChecked(false);
    QApplication::processEvents();

    SubPrepOutputDialogAutomationResult result;
    result.validationCaptureSucceeded = saveDialogCapture(
        *dialog,
        validationCapturePath
        );
    result.validationButtonEnabled = okButton->isEnabled();
    result.validationMessage = validationLabel->text().simplified();
    if (!result.validationCaptureSucceeded || result.validationButtonEnabled)
    {
        result.status = SubPrepOutputDialogAutomationStatus::ValidationStateFailed;
        dialog->reject();
        return result;
    }

    createFolderCheck->setChecked(true);
    if (QLineEdit* const targetEdit = dialog->findChild<QLineEdit*>(
            QStringLiteral("subPrepTargetFolderEdit")
            ))
    {
        targetEdit->setText(targetFolder);
    }
    if (QLineEdit* const nameEdit = dialog->findChild<QLineEdit*>(
            QStringLiteral("subPrepUserNameEdit")
            ))
    {
        nameEdit->setText(userName);
    }
    if (QCheckBox* const createFolderOption = dialog->findChild<QCheckBox*>(
            QStringLiteral("subPrepCreateFolderCheckBox")
            ))
    {
        createFolderOption->setChecked(true);
    }
    if (QCheckBox* const openFolderOption = dialog->findChild<QCheckBox*>(
            QStringLiteral("subPrepOpenFolderCheckBox")
            ))
    {
        openFolderOption->setChecked(false);
    }
    if (QCheckBox* const printPaperOption = dialog->findChild<QCheckBox*>(
            QStringLiteral("subPrepPrintPaperCopiesCheckBox")
            ))
    {
        printPaperOption->setChecked(false);
    }
    for (QCheckBox* const dayCheck : dialog->findChildren<QCheckBox*>())
    {
        if (!dayCheck->property("day").toString().isEmpty())
        {
            dayCheck->setChecked(true);
        }
    }

    result.dialogCaptureSucceeded = saveDialogCapture(
        *dialog,
        dialogCapturePath
        );
    if (!result.dialogCaptureSucceeded)
    {
        result.status = SubPrepOutputDialogAutomationStatus::DialogCaptureFailed;
        dialog->reject();
        return result;
    }

    dialog->accept();
    result.status = SubPrepOutputDialogAutomationStatus::Accepted;
    return result;
}

bool QtCalendarImportWorkflowTestAdapter::hasPreferencesAction(
    const QWidget& mainWindow
    ) const
{
    return mainWindow.findChild<QAction*>(
        QStringLiteral("preferencesAction")
        );
}

bool QtCalendarImportWorkflowTestAdapter::triggerPreferences(
    QWidget& mainWindow
    ) const
{
    QAction* const action = mainWindow.findChild<QAction*>(
        QStringLiteral("preferencesAction")
        );
    if (!action)
    {
        return false;
    }

    action->trigger();
    return true;
}

QDialog* QtCalendarImportWorkflowTestAdapter::findPreferencesDialog(
    QWidget& mainWindow
    )
{
    if (m_preferencesDialog)
    {
        return m_preferencesDialog;
    }

    QDialog* dialog = mainWindow.findChild<QDialog*>(
        QStringLiteral("preferencesDialog")
        );
    if (!dialog)
    {
        dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
    }
    m_preferencesDialog = dialog;
    return dialog;
}

CalendarPreferencesUiSnapshot
QtCalendarImportWorkflowTestAdapter::selectCalendarTab(
    QWidget& mainWindow
    )
{
    CalendarPreferencesUiSnapshot snapshot;
    QDialog* const dialog = findPreferencesDialog(mainWindow);
    snapshot.dialogFound = dialog != nullptr;
    if (!dialog)
    {
        return snapshot;
    }

    QTabWidget* const tabs = dialog->findChild<QTabWidget*>(
        QStringLiteral("preferencesTabs")
        );
    QWidget* const calendarTab = dialog->findChild<QWidget*>(
        QStringLiteral("preferencesCalendarTab")
        );
    QWidget* const panel = dialog->findChild<QWidget*>(
        QStringLiteral("calendarPreferencesPanel")
        );
    QPushButton* const importButton = panel
        ? panel->findChild<QPushButton*>(
              QStringLiteral("preferencesCalendarImportEvents")
              )
        : nullptr;
    snapshot.controlsReady = tabs && calendarTab && panel && importButton;
    if (!snapshot.controlsReady)
    {
        return snapshot;
    }

    tabs->setCurrentWidget(calendarTab);
    QApplication::processEvents();
    snapshot.calendarTabIndex = tabs->indexOf(calendarTab);
    return snapshot;
}

CalendarPreferencesUiSnapshot
QtCalendarImportWorkflowTestAdapter::startImport(
    const QString& outputRoot
    )
{
    CalendarPreferencesUiSnapshot snapshot;
    snapshot.dialogFound = !m_preferencesDialog.isNull();
    if (!m_preferencesDialog)
    {
        return snapshot;
    }

    QWidget* const panel = m_preferencesDialog->findChild<QWidget*>(
        QStringLiteral("calendarPreferencesPanel")
        );
    QPushButton* const importButton = panel
        ? panel->findChild<QPushButton*>(
              QStringLiteral("preferencesCalendarImportEvents")
              )
        : nullptr;
    if (!importButton)
    {
        return snapshot;
    }

    importButton->click();
    return observeImport(outputRoot);
}

CalendarPreferencesUiSnapshot
QtCalendarImportWorkflowTestAdapter::observeImport(
    const QString& outputRoot
    )
{
    CalendarPreferencesUiSnapshot snapshot;
    snapshot.dialogFound = !m_preferencesDialog.isNull();
    if (!m_preferencesDialog)
    {
        return snapshot;
    }

    QWidget* const panel = m_preferencesDialog->findChild<QWidget*>(
        QStringLiteral("calendarPreferencesPanel")
        );
    QPushButton* const importButton = panel
        ? panel->findChild<QPushButton*>(
              QStringLiteral("preferencesCalendarImportEvents")
              )
        : nullptr;
    QLabel* const status = panel
        ? panel->findChild<QLabel*>(QStringLiteral("sectionSubtitle"))
        : nullptr;
    snapshot.controlsReady = importButton && status;
    if (!snapshot.controlsReady)
    {
        return snapshot;
    }

    snapshot.statusText = status->text();
    snapshot.statusVisible = status->isVisible();
    snapshot.importButtonEnabled = importButton->isEnabled();
    if (
        outputRoot.isEmpty()
        || m_loadingCaptureAttempted
        || !snapshot.statusText.startsWith(
            QStringLiteral("Importing events...")
            )
        )
    {
        return snapshot;
    }

    snapshot.loadingCaptureAttempted = true;
    m_loadingCaptureAttempted = true;
    snapshot.loadingControlsDisabled = !snapshot.importButtonEnabled;
    QScrollArea* const calendarScroll =
        m_preferencesDialog->findChild<QScrollArea*>(
            QStringLiteral("preferencesCalendarTab")
            );
    QScrollBar* const scrollBar = calendarScroll
        ? calendarScroll->verticalScrollBar()
        : nullptr;
    const int previousScrollValue = scrollBar ? scrollBar->value() : 0;
    if (scrollBar)
    {
        scrollBar->setValue(scrollBar->maximum());
    }
    QDir().mkpath(outputRoot);
    snapshot.loadingCaptureSucceeded = saveWidgetCapture(
        *m_preferencesDialog,
        QDir(outputRoot).filePath(
            QStringLiteral("calendar-import-loading.png")
            )
        );
    if (scrollBar)
    {
        scrollBar->setValue(previousScrollValue);
    }
    return snapshot;
}

CalendarPreferencesUiSnapshot
QtCalendarImportWorkflowTestAdapter::observeFailure(
    const QString& outputRoot
    )
{
    CalendarPreferencesUiSnapshot snapshot;
    snapshot.dialogFound = !m_preferencesDialog.isNull();
    if (!m_preferencesDialog)
    {
        return snapshot;
    }

    QWidget* const panel = m_preferencesDialog->findChild<QWidget*>(
        QStringLiteral("calendarPreferencesPanel")
        );
    QPushButton* const importButton = panel
        ? panel->findChild<QPushButton*>(
              QStringLiteral("preferencesCalendarImportEvents")
              )
        : nullptr;
    QLabel* const status = panel
        ? panel->findChild<QLabel*>(QStringLiteral("sectionSubtitle"))
        : nullptr;
    snapshot.controlsReady = importButton && status;
    if (!snapshot.controlsReady)
    {
        return snapshot;
    }

    snapshot.statusText = status->text();
    snapshot.statusVisible = status->isVisible();
    snapshot.importButtonEnabled = importButton->isEnabled();
    if (outputRoot.isEmpty())
    {
        return snapshot;
    }

    QScrollArea* const calendarScroll =
        m_preferencesDialog->findChild<QScrollArea*>(
            QStringLiteral("preferencesCalendarTab")
            );
    QScrollBar* const scrollBar = calendarScroll
        ? calendarScroll->verticalScrollBar()
        : nullptr;
    const int previousScrollValue = scrollBar ? scrollBar->value() : 0;
    if (scrollBar)
    {
        scrollBar->setValue(scrollBar->maximum());
        QApplication::processEvents();
    }
    QDir().mkpath(outputRoot);
    snapshot.failureCaptureSucceeded = saveWidgetCapture(
        *m_preferencesDialog,
        QDir(outputRoot).filePath(
            QStringLiteral("calendar-import-error.png")
            )
        );
    if (scrollBar)
    {
        scrollBar->setValue(previousScrollValue);
        QApplication::processEvents();
    }
    return snapshot;
}

bool QtCalendarImportWorkflowTestAdapter::capturePreferences(
    const QString& path
    ) const
{
    return m_preferencesDialog
        && saveWidgetCapture(*m_preferencesDialog, path);
}

bool QtCalendarImportWorkflowTestAdapter::captureCalendarPage(
    QWidget& page,
    const QString& path
    ) const
{
    return saveWidgetCapture(page, path);
}

bool QtCalendarImportWorkflowTestAdapter::closePreferences()
{
    if (!m_preferencesDialog)
    {
        return false;
    }

    m_preferencesDialog->reject();
    return true;
}

class QtScheduleImportWorkflowTestAdapter::State final
{
public:
    QPointer<ScheduleImportDialog> sourceDialog;
    bool loadingCaptureAttempted = false;
};

bool QtScheduleImportWorkflowTestAdapter::open(
    ApplicationServices* services,
    QWidget& parent,
    const QString& filePath
    )
{
    m_state = std::make_shared<State>();
    auto* const dialog = new ScheduleImportDialog(services, &parent);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    m_state->sourceDialog = dialog;
    dialog->setFilePath(filePath);
    dialog->show();
    QApplication::processEvents();

    QRadioButton* const normal = dialog->findChild<QRadioButton*>(
        QStringLiteral("scheduleImportNormalRadio")
        );
    QPushButton* const next = dialog->findChild<QPushButton*>(
        QStringLiteral("scheduleImportNextButton")
        );
    if (!normal || !next)
    {
        dialog->reject();
        return false;
    }

    normal->setChecked(true);
    return true;
}

ScheduleImportSourceSnapshot
QtScheduleImportWorkflowTestAdapter::beginSourceLoad(
    const QString& outputRoot
    )
{
    ScheduleImportSourceSnapshot snapshot;
    snapshot.dialogOpen = m_state && !m_state->sourceDialog.isNull();
    if (!snapshot.dialogOpen)
    {
        return snapshot;
    }

    QPushButton* const next = m_state->sourceDialog->findChild<QPushButton*>(
        QStringLiteral("scheduleImportNextButton")
        );
    if (!next)
    {
        return snapshot;
    }

    next->click();
    return observeSource(outputRoot);
}

ScheduleImportSourceSnapshot
QtScheduleImportWorkflowTestAdapter::observeSource(
    const QString& outputRoot
    )
{
    ScheduleImportSourceSnapshot snapshot;
    snapshot.dialogOpen = m_state && !m_state->sourceDialog.isNull();
    if (!snapshot.dialogOpen)
    {
        return snapshot;
    }

    ScheduleImportDialog* const dialog = m_state->sourceDialog;
    QLabel* const status = dialog->findChild<QLabel*>(
        QStringLiteral("scheduleImportSourceStatus")
        );
    QComboBox* const users = dialog->findChild<QComboBox*>(
        QStringLiteral("scheduleImportUserCombo")
        );
    QComboBox* const sheets = dialog->findChild<QComboBox*>(
        QStringLiteral("scheduleImportSheetCombo")
        );
    QPushButton* const next = dialog->findChild<QPushButton*>(
        QStringLiteral("scheduleImportNextButton")
        );
    QProgressBar* const progress = dialog->findChild<QProgressBar*>(
        QStringLiteral("scheduleImportProgressBar")
        );

    snapshot.statusReady = status != nullptr;
    snapshot.statusText = status ? status->text() : QString();
    snapshot.progressVisible = progress && !progress->isHidden();
    snapshot.nextAvailable = next != nullptr;
    snapshot.nextEnabled = next && next->isEnabled();

    if (snapshot.progressVisible)
    {
        if (
            outputRoot.isEmpty()
            || m_state->loadingCaptureAttempted
            )
        {
            return snapshot;
        }

        QLineEdit* const fileEdit = dialog->findChild<QLineEdit*>(
            QStringLiteral("scheduleImportFilePath")
            );
        QPushButton* const browse = dialog->findChild<QPushButton*>(
            QStringLiteral("scheduleImportBrowseButton")
            );
        QRadioButton* const normal = dialog->findChild<QRadioButton*>(
            QStringLiteral("scheduleImportNormalRadio")
            );
        QRadioButton* const intensive = dialog->findChild<QRadioButton*>(
            QStringLiteral("scheduleImportIntensiveRadio")
            );
        const bool controlsDisabled =
            fileEdit
            && browse
            && normal
            && intensive
            && next
            && !fileEdit->isEnabled()
            && !browse->isEnabled()
            && !normal->isEnabled()
            && !intensive->isEnabled()
            && !next->isEnabled();
        QDir().mkpath(outputRoot);
        const bool captured = saveWidgetCapture(
            *dialog,
            QDir(outputRoot).filePath(
                QStringLiteral("schedule-import-loading.png")
                )
            );

        snapshot.loadingCaptureAttempted = true;
        snapshot.loadingProgressVisible = progress->isVisible();
        snapshot.loadingControlsDisabled = controlsDisabled;
        snapshot.loadingCaptureSucceeded = captured;
        m_state->loadingCaptureAttempted = true;
        return snapshot;
    }

    if (sheets && users && !users->isVisible())
    {
        for (int index = 0; index < sheets->count(); ++index)
        {
            if (sheets->itemData(index).toInt() >= 0)
            {
                sheets->setCurrentIndex(index);
                QApplication::processEvents();
                break;
            }
        }
    }

    if (users && users->isVisible())
    {
        for (int index = 0; index < users->count(); ++index)
        {
            if (users->itemData(index).toInt() >= 0)
            {
                snapshot.selectedUserIndex = index;
                break;
            }
        }
    }

    if (!users || snapshot.selectedUserIndex < 0 || !next)
    {
        return snapshot;
    }

    users->setCurrentIndex(snapshot.selectedUserIndex);
    if (QCheckBox* const confirmation = dialog->findChild<QCheckBox*>(
            QStringLiteral("scheduleImportNameConfirmation")
            ))
    {
        confirmation->setChecked(true);
    }
    snapshot.selectionReady = true;
    snapshot.nextEnabled = next->isEnabled();
    return snapshot;
}

bool QtScheduleImportWorkflowTestAdapter::beginReview()
{
    if (!m_state || !m_state->sourceDialog)
    {
        return false;
    }

    QPushButton* const next = m_state->sourceDialog->findChild<QPushButton*>(
        QStringLiteral("scheduleImportNextButton")
        );
    if (!next)
    {
        return false;
    }

    next->click();
    QApplication::processEvents();
    return true;
}

ScheduleImportReviewSnapshot
QtScheduleImportWorkflowTestAdapter::inspectReview() const
{
    ScheduleImportReviewSnapshot snapshot;
    if (!m_state || !m_state->sourceDialog)
    {
        return snapshot;
    }

    ScheduleImportReviewDialog* const review =
        m_state->sourceDialog->findChild<ScheduleImportReviewDialog*>();
    snapshot.reviewFound = review != nullptr;
    if (!review)
    {
        return snapshot;
    }

    QPushButton* const import = review->findChild<QPushButton*>(
        QStringLiteral("scheduleImportAcceptButton")
        );
    snapshot.importButtonFound = import != nullptr;
    snapshot.reviewVisible = review->isVisible();
    snapshot.importEnabled = import && import->isEnabled();
    return snapshot;
}

void QtScheduleImportWorkflowTestAdapter::acknowledgeWarnings()
{
    const ScheduleImportReviewSnapshot snapshot = inspectReview();
    if (!snapshot.reviewFound || !m_state || !m_state->sourceDialog)
    {
        return;
    }

    ScheduleImportReviewDialog* const review =
        m_state->sourceDialog->findChild<ScheduleImportReviewDialog*>();
    QCheckBox* const acknowledgement = review
        ? review->findChild<QCheckBox*>(
              QStringLiteral("scheduleImportWarningAcknowledgement")
              )
        : nullptr;
    if (acknowledgement)
    {
        acknowledgement->setChecked(true);
        QApplication::processEvents();
    }
}

ScheduleImportReviewCaptures
QtScheduleImportWorkflowTestAdapter::captureReview(
    const QString& outputRoot
    )
{
    ScheduleImportReviewCaptures captures;
    if (outputRoot.isEmpty() || !m_state || !m_state->sourceDialog)
    {
        return captures;
    }

    ScheduleImportReviewDialog* const review =
        m_state->sourceDialog->findChild<ScheduleImportReviewDialog*>();
    if (!review)
    {
        return captures;
    }

    QDir().mkpath(outputRoot);
    captures.sourceCaptured = saveWidgetCapture(
        *m_state->sourceDialog,
        QDir(outputRoot).filePath(
            QStringLiteral("schedule-import-source.png")
            )
        );
    review->show();
    QApplication::processEvents();
    captures.reviewCaptured = saveWidgetCapture(
        *review,
        QDir(outputRoot).filePath(
            QStringLiteral("schedule-import-review.png")
            )
        );
    return captures;
}

bool QtScheduleImportWorkflowTestAdapter::clickImport()
{
    if (!m_state || !m_state->sourceDialog)
    {
        return false;
    }

    ScheduleImportReviewDialog* const review =
        m_state->sourceDialog->findChild<ScheduleImportReviewDialog*>();
    QPushButton* const import = review
        ? review->findChild<QPushButton*>(
              QStringLiteral("scheduleImportAcceptButton")
              )
        : nullptr;
    if (!import)
    {
        return false;
    }

    import->click();
    return true;
}

bool QtScheduleImportWorkflowTestAdapter::sourceDialogVisible() const
{
    return m_state
        && m_state->sourceDialog
        && m_state->sourceDialog->isVisible();
}

bool QtScheduleImportWorkflowTestAdapter::rejectReview()
{
    if (!m_state || !m_state->sourceDialog)
    {
        return false;
    }

    ScheduleImportReviewDialog* const review =
        m_state->sourceDialog->findChild<ScheduleImportReviewDialog*>();
    if (!review)
    {
        return false;
    }

    review->reject();
    return true;
}

bool QtScheduleImportWorkflowTestAdapter::rejectSourceDialog()
{
    if (!m_state || !m_state->sourceDialog)
    {
        return false;
    }

    m_state->sourceDialog->reject();
    return true;
}
