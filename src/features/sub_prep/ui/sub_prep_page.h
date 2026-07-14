#pragma once

#include "domain/models/campus_info.h"
#include "features/sub_prep/ui/sub_prep_class_information_model.h"
#include "ui/shared/pages/basepage.h"

#include <QList>

class ApplicationServices;
class QLabel;
class QLineEdit;
class QPushButton;
class QScrollArea;
class QShowEvent;
class QTextEdit;
class QTimer;
class QVBoxLayout;
class QWidget;
class ScheduleSectionWidget;
class SectionCard;
struct ScheduleViewModel;

enum class SubPrepSection
{
    ImportantInformation,
    SubNotes
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
    void retranslateUi() override;

    void scrollToSection(
        SubPrepSection section
        );
    void scrollToTop();
    QString currentSectionName() const;
    QString currentSectionKey() const;

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
    void autosave();
    void printSubPrep();

private:
    void buildUi();
    void loadPageData();
    void loadStoredSettings();
    void loadPersonalZoomInformation();
    void loadCampuses();
    void loadCampusFields(
        const QString& campusId
        );
    void updateReadOnlyFieldWidths();

    bool saveSubPrepInternal();

    void refreshGeneratedContent();
    void rebuildClassInformation();
    QList<SubPrepClassInformation::TeacherGroup> buildClassInformation() const;
    QList<SubPrepClassInformation::TeacherGroup> buildClassInformation(
        const ScheduleViewModel& schedule
        ) const;

    bool restoreGradingDefaultIfNeeded();
    QString defaultGradingInstructions() const;
    QString defaultSpecialInstructions() const;

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
    SubPrepSection m_currentSection = SubPrepSection::ImportantInformation;
    QList<CampusInfo> m_campuses;

    QScrollArea* m_scrollArea = nullptr;
    QWidget* m_scrollContent = nullptr;
    QVBoxLayout* m_scrollContentLayout = nullptr;

    QLabel* m_titleLabel = nullptr;
    QLabel* m_subtitleLabel = nullptr;
    QPushButton* m_printButton = nullptr;
    QLabel* m_importantInformationHeading = nullptr;
    QLabel* m_scheduleHeading = nullptr;
    QLabel* m_classInformationHeading = nullptr;
    QLabel* m_subNotesHeading = nullptr;

    SectionCard* m_campusCard = nullptr;
    SectionCard* m_zoomCard = nullptr;
    SectionCard* m_materialsCard = nullptr;
    SectionCard* m_gradingCard = nullptr;
    SectionCard* m_scheduleCard = nullptr;
    SectionCard* m_notesCard = nullptr;

    QLineEdit* m_officeNumberEdit = nullptr;
    QLineEdit* m_officeWifiEdit = nullptr;
    QLineEdit* m_officeWifiPasswordEdit = nullptr;
    QLineEdit* m_photocopierCodeEdit = nullptr;
    QLineEdit* m_zoomLoginIdEdit = nullptr;
    QLineEdit* m_zoomPasswordEdit = nullptr;

    QTextEdit* m_classMaterialsEdit = nullptr;
    QTextEdit* m_gradingInstructionsEdit = nullptr;
    QTextEdit* m_specialInstructionsEdit = nullptr;
    QTextEdit* m_subNotesEdit = nullptr;

    QLabel* m_officeNumberLabel = nullptr;
    QLabel* m_officeWifiLabel = nullptr;
    QLabel* m_officeWifiPasswordLabel = nullptr;
    QLabel* m_photocopierCodeLabel = nullptr;
    QLabel* m_zoomLoginIdLabel = nullptr;
    QLabel* m_zoomPasswordLabel = nullptr;
    QLabel* m_gradingInstructionsLabel = nullptr;
    QLabel* m_specialInstructionsLabel = nullptr;

    ScheduleSectionWidget* m_scheduleWidget = nullptr;
    QWidget* m_classInformationContent = nullptr;
    QVBoxLayout* m_classInformationLayout = nullptr;

    QTimer* m_autosaveTimer = nullptr;
};
