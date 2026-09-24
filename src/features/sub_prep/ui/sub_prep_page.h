#pragma once

#include "next/domain/domain_types.h"
#include "next/application/sub_prep_campus_directory_query_port.h"
#include "features/sub_prep/ui/sub_prep_class_information_model.h"
#include "ui/shared/pages/basepage.h"

#include <QList>
#include <QStringList>

#include <memory>
#include <vector>

class ApplicationServices;
class QLabel;
class QLineEdit;
class QListView;
class QPushButton;
class QScrollArea;
class NavigationTabStrip;
class OnScreenKeyboard;
class QTextEdit;
class QTimer;
class QVBoxLayout;
class QWidget;
class ScheduleWidget;
class SectionCard;
class SubPrepClassInformationDataAccess;
class SubPrepClassInformationListModel;
namespace ClassMngr::Next::Application
{
class SubPrepClassDetailsReadPort;
class SubPrepClassInformationState;
class SubPrepPrintSourceReadPort;
class SubPrepScheduleSummaryReadPort;
}

enum class SubPrepSection
{
    ImportantInformation,
    SubNotes
};

struct SubPrepPageRuntimeMetrics
{
    int classInformationWidgetCount = 0;
    int classInformationTextEditCount = 0;
    int classInformationNavigationRowCount = 0;
    int classInformationSourceClassCount = 0;
    int classInformationVisibleClassCount = 0;
    int classInformationGroupCount = 0;
    int classInformationClassInfoLookupCount = 0;
    int classInformationTeacherLookupCount = 0;
    int classInformationRosterLookupCount = 0;
    int classInformationClassQueryCount = 0;
    int classInformationClassResultRowCount = 0;
    int classInformationClassInfoQueryCount = 0;
    int classInformationClassInfoResultRowCount = 0;
    int classInformationClassInfoScheduleRowCount = 0;
    int classInformationTeacherQueryCount = 0;
    int classInformationTeacherResultRowCount = 0;
    int classInformationRosterQueryCount = 0;
    int classInformationRosterResultRowCount = 0;
    int classInformationRosterStudentResultCount = 0;
    int classInformationRebuildCount = 0;
    int selectedClassId = -1;
};

class SubPrepPage : public BasePage
{
    Q_OBJECT

public:
    explicit SubPrepPage(
        ApplicationServices* services,
        QWidget* parent = nullptr
        );
    SubPrepPage(
        ApplicationServices* services,
        ClassMngr::Next::Application::SubPrepScheduleSummaryReadPort&
            summaryReadPort,
        ClassMngr::Next::Application::SubPrepClassDetailsReadPort&
            detailsReadPort,
        QWidget* parent = nullptr
        );
    SubPrepPage(
        ApplicationServices* services,
        ClassMngr::Next::Application::SubPrepScheduleSummaryReadPort&
            summaryReadPort,
        ClassMngr::Next::Application::SubPrepClassDetailsReadPort&
            detailsReadPort,
        ClassMngr::Next::Application::SubPrepPrintSourceReadPort&
            printSourceReadPort,
        QWidget* parent = nullptr
        );
    ~SubPrepPage() override;

    void saveData() override;
    bool saveChanges() override;
    bool hasUnsavedChanges() const override;
    void discardChanges() override;
    void refresh() override;
    void releaseFeatureResources() override;
    void clearDatabaseState() override;
    void retranslateUi() override;

    void scrollToSection(
        SubPrepSection section
        );
    void scrollToTop();
    QString currentSectionName() const;
    QString currentSectionKey() const;
    [[nodiscard]] SubPrepPageRuntimeMetrics runtimeMetrics() const;

    // Used by the opt-in heavy startup diagnostics to exercise a real class
    // selection without relying on native desktop automation.
    [[nodiscard]] bool selectClassForStartupDiagnostics(int classId);
    // Used by the opt-in heavy startup visual diagnostics to make the
    // class-information or empty-state content visible in the captured frame.
    void scrollToClassInformationForStartupDiagnostics(
        bool emptyState = false
        );

protected:
    bool eventFilter(
        QObject* watched,
        QEvent* event
        ) override;

private slots:
    void handleEditableChanged();
    void autosave();
    void generateSubPrep();

private:
    void initialize();
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
    int currentClassInformationId() const;
    void handleClassInformationGradeChanged(int index);
    void handleClassInformationSelectionChanged();
    void showSelectedClassInformation(
        const ClassMngr::Next::Domain::ClassId& classId
        );
    void renderSelectedClassInformation();
    void updateClassInformationEmptyState();
    bool buildPrintClassInformation(
        const QList<int>& classIds,
        const QStringList& selectedDays,
        bool useIntensive,
        QList<SubPrepClassInformation::TeacherGroup>* groups,
        QString* errorMessage
        );

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
    std::vector<
        ClassMngr::Next::Application::SubPrepCampusMetadata
        > m_campuses;

    QScrollArea* m_scrollArea = nullptr;
    QWidget* m_scrollContent = nullptr;
    QVBoxLayout* m_scrollContentLayout = nullptr;

    QLabel* m_titleLabel = nullptr;
    QLabel* m_subtitleLabel = nullptr;
    QPushButton* m_printButton = nullptr;
    QPushButton* m_koreanKeyboardButton = nullptr;
    OnScreenKeyboard* m_onScreenKeyboard = nullptr;
    QLabel* m_importantInformationHeading = nullptr;
    QLabel* m_scheduleHeading = nullptr;
    QLabel* m_classInformationHeading = nullptr;

    SectionCard* m_campusCard = nullptr;
    SectionCard* m_zoomCard = nullptr;
    SectionCard* m_materialsCard = nullptr;
    SectionCard* m_gradingCard = nullptr;
    SectionCard* m_scheduleCard = nullptr;

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
    QLabel* m_materialsLocationLabel = nullptr;
    QLabel* m_detailedClassNotesLabel = nullptr;
    QLabel* m_gradingInstructionsLabel = nullptr;
    QLabel* m_specialInstructionsLabel = nullptr;

    ScheduleWidget* m_scheduleWidget = nullptr;
    QWidget* m_classInformationContent = nullptr;
    QVBoxLayout* m_classInformationLayout = nullptr;
    NavigationTabStrip* m_classInformationGradeTabs = nullptr;
    QListView* m_classInformationListView = nullptr;
    SubPrepClassInformationListModel* m_classInformationModel = nullptr;
    QLabel* m_classInformationEmptyLabel = nullptr;
    SectionCard* m_classInformationDetailsCard = nullptr;
    QWidget* m_classInformationDetails = nullptr;
    QLabel* m_classInformationLevelValue = nullptr;
    QLabel* m_classInformationTimeValue = nullptr;
    QLabel* m_classInformationStudentCountValue = nullptr;
    QLabel* m_classInformationRoomValue = nullptr;
    QLabel* m_classInformationWifiNameValue = nullptr;
    QLabel* m_classInformationWifiPasswordValue = nullptr;
    QLabel* m_classInformationZoomIdValue = nullptr;
    QLabel* m_classInformationZoomPasswordValue = nullptr;
    QLabel* m_classInformationInternetValue = nullptr;
    QLabel* m_classInformationProjectionValue = nullptr;
    QLabel* m_classInformationClassNotesLabel = nullptr;
    QTextEdit* m_classInformationClassNotes = nullptr;
    QLabel* m_classInformationTeacherNotesLabel = nullptr;
    QTextEdit* m_classInformationTeacherNotes = nullptr;
    QStringList m_classInformationGrades;
    std::unique_ptr<SubPrepClassInformationDataAccess>
        m_classInformationDataAccess;
    std::unique_ptr<
        ClassMngr::Next::Application::SubPrepClassInformationState
        > m_classInformationState;
    bool m_updatingClassInformation = false;
    int m_selectedClassId = -1;
    int m_classInformationSourceClassCount = 0;
    int m_classInformationVisibleClassCount = 0;
    int m_classInformationGroupCount = 0;
    int m_classInformationNavigationRowCount = 0;
    int m_classInformationClassInfoLookupCount = 0;
    int m_classInformationTeacherLookupCount = 0;
    int m_classInformationRosterLookupCount = 0;
    int m_classInformationClassQueryCount = 0;
    int m_classInformationClassResultRowCount = 0;
    int m_classInformationClassInfoQueryCount = 0;
    int m_classInformationClassInfoResultRowCount = 0;
    int m_classInformationClassInfoScheduleRowCount = 0;
    int m_classInformationTeacherQueryCount = 0;
    int m_classInformationTeacherResultRowCount = 0;
    int m_classInformationRosterQueryCount = 0;
    int m_classInformationRosterResultRowCount = 0;
    int m_classInformationRosterStudentResultCount = 0;
    int m_classInformationRebuildCount = 0;

    QTimer* m_autosaveTimer = nullptr;
};
