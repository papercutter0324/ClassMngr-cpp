#pragma once

#include "domain/models/schedule_import.h"

#include <QDialog>
#include <QList>
#include <QString>

class ApplicationServices;
class QCheckBox;
class QComboBox;
class QDialogButtonBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QRadioButton;
class QStackedWidget;
class QVBoxLayout;
class ScheduleWidget;

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
    struct TeacherControl
    {
        QString teacherKey;
        QComboBox* action = nullptr;
        QComboBox* room = nullptr;
    };

    struct ClassControl
    {
        int candidateIndex = -1;
        QString teacherKey;
        QComboBox* action = nullptr;
        QPushButton* colorButton = nullptr;
        QString color;
        QLabel* details = nullptr;
    };

    void buildUi();
    QWidget* buildSourcePage();
    QWidget* buildUserPage();
    QWidget* buildReviewPage();
    void browseForFile();
    bool loadWorkbook();
    void prepareUserSelection();
    void updateUserSelection();
    bool buildReview();
    void rebuildResolutionControls();
    void chooseClassColor(int candidateIndex);
    void updateClassColorButton(ClassControl* control);
    void updateReviewState();
    void updateNavigation();
    void goBack();
    void goNext();
    void applyImport();

    [[nodiscard]] ScheduleImportKind selectedKind() const;
    [[nodiscard]] const ScheduleImportSheet* selectedSheet() const;
    [[nodiscard]] const ScheduleImportUserBlock* selectedUser() const;
    [[nodiscard]] ScheduleImportPlan importPlan() const;

    ApplicationServices* m_services = nullptr;
    ScheduleImportWorkbook m_workbook;
    ScheduleImportPreview m_preview;
    QString m_loadedFilePath;
    ScheduleImportKind m_loadedKind =
        ScheduleImportKind::Normal;
    bool m_workbookLoaded = false;
    QString m_profileName;

    QStackedWidget* m_pages = nullptr;
    QLineEdit* m_fileEdit = nullptr;
    QPushButton* m_browseButton = nullptr;
    QRadioButton* m_normalRadio = nullptr;
    QRadioButton* m_intensiveRadio = nullptr;
    QLabel* m_sourceStatus = nullptr;
    QLabel* m_sheetLabel = nullptr;
    QComboBox* m_sheetCombo = nullptr;

    QLabel* m_userStatus = nullptr;
    QComboBox* m_userCombo = nullptr;
    QCheckBox* m_nameConfirmation = nullptr;

    ScheduleWidget* m_previewWidget = nullptr;
    QLabel* m_warningLabel = nullptr;
    QCheckBox* m_warningAcknowledgement = nullptr;
    QWidget* m_resolutionContent = nullptr;
    QVBoxLayout* m_resolutionLayout = nullptr;
    QLabel* m_reviewStatus = nullptr;
    QLabel* m_reviewSummary = nullptr;
    QList<TeacherControl> m_teacherControls;
    QList<ClassControl> m_classControls;

    QDialogButtonBox* m_buttons = nullptr;
    QPushButton* m_backButton = nullptr;
    QPushButton* m_nextButton = nullptr;
    QPushButton* m_importButton = nullptr;
};
