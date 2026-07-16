#pragma once

#include "ui/shared/pages/basepage.h"

#include <QByteArray>

class ApplicationServices;
class QLabel;
class QCheckBox;
class QComboBox;
class QEvent;
class QLineEdit;
class QPushButton;
class QScrollArea;
class QShowEvent;
class QTabWidget;
class QTextEdit;
class QTimer;
class QVBoxLayout;
class QWidget;
class ScheduleSectionWidget;

enum class MyInfoSection
{
    ClassSchedule,
    ClassInformation,
    MyInformation
};

enum class MyInfoPageMode
{
    Information,
    Schedule,
    ClassInformation
};

class MyInfoPage : public BasePage
{
    Q_OBJECT

public:
    explicit MyInfoPage(
        ApplicationServices* services,
        MyInfoPageMode mode = MyInfoPageMode::Information,
        QWidget* parent = nullptr
        );

    void refresh() override;
    void retranslateUi() override;
    void saveData() override;
    bool saveChanges() override;
    bool hasUnsavedChanges() const override;
    void discardChanges() override;
    void setSaveMode(
        SaveMode mode
        ) override;

    void scrollToSection(
        MyInfoSection section
        );
    void scrollToTop();
    QString currentSectionName() const;
    QString currentSectionKey() const;

signals:
    void classInfoSaved(
        int classId
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
    void handleZoomNotAvailableChanged(
        bool checked
        );
    void chooseSignatureImage();
    void removeSignatureImage();
    void autosave();

private:
    void buildUi();
    QString pageTitle() const;
    QString pageSubtitle() const;
    bool includesMyInformation() const;
    bool includesClassSchedule() const;
    bool includesClassInformation() const;
    void buildClassScheduleSection();
    void buildClassInformationSection();
    void buildMyInformationSection();
    void buildSignatureSection();
    void loadPageData();
    void loadStoredSettings();
    bool saveMyInfoInternal();
    bool normalizeZoomFields();
    bool normalizeLineEdit(
        QLineEdit* edit,
        const QString& defaultText
        );
    void setZoomFieldsEnabled();
    void updateMyInformationFieldWidths();
    void updateSignaturePreview();
    void refreshGeneratedContent();
    void rebuildClassInformation();
    void clearClassInformation();
    void clearDirty();

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

private:
    ApplicationServices* m_services = nullptr;
    MyInfoPageMode m_mode = MyInfoPageMode::Information;
    bool m_loading = false;
    bool m_dirty = false;
    SaveMode m_saveMode = SaveMode::Automatic;
    MyInfoSection m_currentSection = MyInfoSection::MyInformation;

    QScrollArea* m_scrollArea = nullptr;
    QWidget* m_scrollContent = nullptr;
    QVBoxLayout* m_scrollContentLayout = nullptr;

    QLabel* m_titleLabel = nullptr;
    QLabel* m_subtitleLabel = nullptr;
    QLabel* m_classScheduleHeading = nullptr;
    QLabel* m_classInformationHeading = nullptr;
    QLabel* m_myInformationHeading = nullptr;
    QLabel* m_signatureHeading = nullptr;

    QLabel* m_nameLabel = nullptr;
    QLabel* m_campusLabel = nullptr;
    QLabel* m_zoomLoginIdLabel = nullptr;
    QLabel* m_zoomPasswordLabel = nullptr;
    QLabel* m_zoomLabel = nullptr;

    ScheduleSectionWidget* m_scheduleWidget = nullptr;

    QWidget* m_classInformationContent = nullptr;
    QVBoxLayout* m_classInformationLayout = nullptr;
    QTabWidget* m_classInformationTabs = nullptr;
    int m_selectedClassId = -1;

    QLineEdit* m_nameEdit = nullptr;
    QComboBox* m_campusCombo = nullptr;
    QLineEdit* m_zoomLoginIdEdit = nullptr;
    QLineEdit* m_zoomPasswordEdit = nullptr;
    QCheckBox* m_zoomNotAvailableCheck = nullptr;
    QLabel* m_signatureInstructionsLabel = nullptr;
    QLabel* m_signaturePreviewLabel = nullptr;
    QPushButton* m_chooseSignatureButton = nullptr;
    QPushButton* m_removeSignatureButton = nullptr;
    QByteArray m_signatureImageData;

    QTimer* m_autosaveTimer = nullptr;
};
