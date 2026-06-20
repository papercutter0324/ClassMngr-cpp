#pragma once

#include "domain/models/calendar_event.h"
#include "ui/shared/pages/basepage.h"

class ApplicationServices;
class AcademicCalendarProvider;
class CalendarEventModel;
class QLabel;
class QCheckBox;
class QComboBox;
class QEvent;
class QLineEdit;
class QQuickWidget;
class QScrollArea;
class QShowEvent;
class QTimer;
class QVBoxLayout;
class ScheduleSectionWidget;

enum class MyInfoSection
{
    ClassSchedule,
    MyInformation,
    MonthlyCalendar
};

class MyInfoPage : public BasePage
{
    Q_OBJECT

public:
    explicit MyInfoPage(
        ApplicationServices* services,
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
    void autosave();
    void handleCalendarDayActivated(
        int year,
        int month,
        int day
        );
    void handleCalendarEventActivated(
        int eventId
        );
    void handleCalendarConfigureRequested(
        int year,
        int month
        );

private:
    void buildUi();
    void buildClassScheduleSection();
    void buildMyInformationSection();
    void buildMonthlyCalendarSection();
    void loadPageData();
    void loadStoredSettings();
    bool saveMyInfoInternal();
    bool normalizeZoomFields();
    bool normalizeLineEdit(
        QLineEdit* edit,
        const QString& defaultText
        );
    void setZoomFieldsEnabled();
    void clearDirty();
    void openCalendarDialog(
        const CalendarEvent& event,
        bool existingEvent
        );

    QLabel* createTopLevelHeading(
        const QString& text,
        QWidget* parent
        ) const;
    QLabel* createFieldLabel(
        const QString& text,
        QWidget* parent
        ) const;

private:
    ApplicationServices* m_services = nullptr;
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
    QLabel* m_myInformationHeading = nullptr;
    QLabel* m_monthlyCalendarHeading = nullptr;

    QLabel* m_campusLabel = nullptr;
    QLabel* m_zoomLoginIdLabel = nullptr;
    QLabel* m_zoomPasswordLabel = nullptr;
    QLabel* m_zoomLabel = nullptr;

    ScheduleSectionWidget* m_scheduleWidget = nullptr;

    QComboBox* m_campusCombo = nullptr;
    QLineEdit* m_zoomLoginIdEdit = nullptr;
    QLineEdit* m_zoomPasswordEdit = nullptr;
    QCheckBox* m_zoomNotAvailableCheck = nullptr;

    CalendarEventModel* m_calendarModel = nullptr;
    AcademicCalendarProvider* m_academicCalendarProvider = nullptr;
    QQuickWidget* m_calendarView = nullptr;

    QTimer* m_autosaveTimer = nullptr;
};
