#pragma once

#include "domain/models/schedule_import.h"

#include <QDialog>
#include <QString>

class ApplicationServices;
class QCheckBox;
class QComboBox;
class QDialogButtonBox;
class QGroupBox;
class QLabel;
class QLineEdit;
class QProgressBar;
class QPushButton;
class QRadioButton;
class ScheduleImportReviewDialog;

class ScheduleImportDialog final : public QDialog
{
    Q_OBJECT

public:
    explicit ScheduleImportDialog(
        ApplicationServices* services,
        QWidget* parent = nullptr
        );

    void setFilePath(
        const QString& filePath
        );

private:
    void buildUi();
    void browseForFile();
    bool loadWorkbook();
    void applyLoadedWorkbook(
        ScheduleImportWorkbook workbook,
        const QString& filePath,
        ScheduleImportKind kind
        );
    void setLoading(bool loading);
    void setSourceStatus(
        const QString& text,
        bool showContinuationHint = false
        );
    void resetUserSelection();
    void updateSelectedSheet();
    void prepareUserSelection();
    void updateUserSelection();
    void updateNavigation();
    void enforceStaticSize();
    void goNext();
    void openReviewDialog();

    [[nodiscard]] ScheduleImportKind selectedKind() const;
    [[nodiscard]] const ScheduleImportSheet* selectedSheet() const;
    [[nodiscard]] const ScheduleImportUserBlock* selectedUser() const;

    ApplicationServices* m_services = nullptr;
    ScheduleImportWorkbook m_workbook;
    QString m_loadedFilePath;
    ScheduleImportKind m_loadedKind =
        ScheduleImportKind::Normal;
    bool m_workbookLoaded = false;
    bool m_loading = false;
    quint64 m_loadRequestId = 0;
    QString m_profileName;

    QLineEdit* m_fileEdit = nullptr;
    QPushButton* m_browseButton = nullptr;
    QGroupBox* m_fileSection = nullptr;
    QGroupBox* m_scheduleTypeSection = nullptr;
    QRadioButton* m_normalRadio = nullptr;
    QRadioButton* m_intensiveRadio = nullptr;
    QLabel* m_sourceStatus = nullptr;
    QProgressBar* m_progressBar = nullptr;
    QGroupBox* m_worksheetSection = nullptr;
    QComboBox* m_sheetCombo = nullptr;

    QGroupBox* m_userSection = nullptr;
    QLabel* m_userStatus = nullptr;
    QComboBox* m_userCombo = nullptr;
    QCheckBox* m_nameConfirmation = nullptr;
    QWidget* m_continuationSpacer = nullptr;
    QLabel* m_continuationHint = nullptr;

    QDialogButtonBox* m_buttons = nullptr;
    QPushButton* m_nextButton = nullptr;
    ScheduleImportReviewDialog* m_reviewDialog = nullptr;
};
