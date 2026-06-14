#pragma once

#include "models/campus.h"
#include "ui/pages/basepage.h"

#include <QList>

class ApplicationServices;
class QComboBox;
class QEvent;
class QLabel;
class QLineEdit;
class QScrollArea;
class QShowEvent;
class QTextEdit;
class QTimer;
class QVBoxLayout;
class QWidget;

enum class SubPrepSection
{
    ImportantInformation,
    ClassInformation,
    SubComments
};

class SubPrepPage : public BasePage
{
    Q_OBJECT

public:
    explicit SubPrepPage(
        ApplicationServices* services,
        QWidget* parent = nullptr
        );

    void saveData() override;
    bool saveChanges() override;
    bool hasUnsavedChanges() const override;
    void discardChanges() override;
    void refresh() override;

    void scrollToSection(
        SubPrepSection section
        );

protected:
    void showEvent(
        QShowEvent* event
        ) override;

    bool eventFilter(
        QObject* watched,
        QEvent* event
        ) override;

private slots:
    void handleEditableChanged();
    void handleCampusChanged(
        int index
        );
    void autosave();

private:
    void buildUi();
    void loadPageData();
    void loadStoredSettings();
    void loadCampuses();
    void loadCampusFields(
        int campusId
        );

    bool saveSubPrepInternal();
    bool saveCurrentCampus();
    int ensureDefaultCampus();

    void refreshGeneratedContent();
    void rebuildTimeFillerActivities();
    void rebuildClassInformation();

    bool normalizeProtectedFields();
    bool normalizeLineEdit(
        QLineEdit* edit,
        const QString& defaultText
        );
    bool restoreBookReportDefaultIfNeeded();
    QString defaultBookReportText() const;

    QLabel* createTopLevelHeading(
        const QString& text,
        QWidget* parent
        ) const;
    QLabel* createFieldLabel(
        const QString& text,
        QWidget* parent
        ) const;
    QTextEdit* createTextEdit(
        int minimumLines,
        bool readOnly,
        QWidget* parent
        ) const;
    void clearClassInformation();
    void clearDirty();

private:
    ApplicationServices* m_services = nullptr;

    bool m_loading = false;
    bool m_dirty = false;
    int m_currentCampusId = -1;
    QList<CampusRecord> m_campuses;

    QScrollArea* m_scrollArea = nullptr;
    QWidget* m_scrollContent = nullptr;
    QVBoxLayout* m_scrollContentLayout = nullptr;

    QLabel* m_titleLabel = nullptr;
    QLabel* m_subtitleLabel = nullptr;

    QLineEdit* m_zoomEmailEdit = nullptr;
    QLineEdit* m_zoomPasswordEdit = nullptr;

    QComboBox* m_campusCombo = nullptr;
    QLineEdit* m_officeNumberEdit = nullptr;
    QLineEdit* m_officeWifiEdit = nullptr;
    QLineEdit* m_officeWifiPasswordEdit = nullptr;
    QLineEdit* m_photocopierCodeEdit = nullptr;

    QTextEdit* m_classMaterialsEdit = nullptr;
    QTextEdit* m_timeFillerActivitiesEdit = nullptr;
    QTextEdit* m_bookReportGradingEdit = nullptr;
    QTextEdit* m_subCommentsEdit = nullptr;

    QLabel* m_importantInformationHeading = nullptr;
    QLabel* m_classInformationHeading = nullptr;
    QLabel* m_subCommentsHeading = nullptr;

    QWidget* m_classInformationContent = nullptr;
    QVBoxLayout* m_classInformationLayout = nullptr;

    QTimer* m_autosaveTimer = nullptr;
};
