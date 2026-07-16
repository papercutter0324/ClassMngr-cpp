#pragma once

#include "domain/models/calendar_event.h"
#include "ui/shared/pages/basepage.h"

#include <array>

#include <QByteArray>
#include <QColor>
#include <QDate>
#include <QHash>
#include <QStringList>

class ApplicationServices;
class AcademicCalendarProvider;
class CalendarEventModel;
class QFont;
class QLabel;
class QCheckBox;
class QComboBox;
class QEvent;
class QLineEdit;
class QPushButton;
class QQuickWidget;
class QScrollArea;
class QShowEvent;
class QTabWidget;
class QTextEdit;
class QTimer;
class QVBoxLayout;
class QWidget;
class ScheduleSectionWidget;

enum class UpcomingEventsScope
{
    CurrentMonth = 0,
    Next30Days,
    Next10Events
};

constexpr int UpcomingEventsScopeCount = 3;

enum class MyInfoSection
{
    ClassSchedule,
    ClassInformation,
    MyInformation,
    MonthlyCalendar
};

enum class MyInfoPageMode
{
    Information,
    Calendar,
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
    void handleCalendarDisplayedMonthChanged(
        int year,
        int month
        );

private:
    void buildUi();
    QString pageTitle() const;
    QString pageSubtitle() const;
    bool includesMyInformation() const;
    bool includesClassSchedule() const;
    bool includesClassInformation() const;
    bool includesMonthlyCalendar() const;
    void buildClassScheduleSection();
    void buildClassInformationSection();
    void buildMyInformationSection();
    void buildSignatureSection();
    void buildMonthlyCalendarSection();
    void buildUpcomingEventsPanel(
        QVBoxLayout* cardLayout,
        QWidget* parent
        );
    QWidget* createUpcomingEventsPage(
        QVBoxLayout** pageLayout,
        QWidget* parent
        );
    QWidget* createEventTypeFilterRow(
        QWidget* parent
        );
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
    void openCalendarDialog(
        const CalendarEvent& event,
        bool existingEvent
        );
    void refreshUpcomingEvents();
    void updateCalendarCampusFilter();
    void renderUpcomingEvents(
        UpcomingEventsScope scope,
        const QList<CalendarEvent>& events,
        int dateColumnWidth,
        int timeColumnWidth,
        int eventTypeColumnWidth
        );
    QList<CalendarEvent> upcomingEventsForScope(
        UpcomingEventsScope scope
        ) const;
    QStringList activeCalendarEventTypes() const;
    QColor calendarEventTypeColor(
        const QString& eventType
        ) const;
    void saveCalendarEventTypeColor(
        const QString& eventType,
        const QColor& color
        );
    void chooseCalendarEventTypeColor(
        const QString& eventType
        );
    QString eventTypeBadgeStyle(
        const QString& eventType,
        const QFont& font
        ) const;
    QString eventTypeFilterButtonStyle(
        const QString& eventType,
        bool checked,
        const QFont& font
        ) const;
    void syncEventTypeFilterButtons();
    void syncCalendarEventTypeColors();
    void syncCalendarFontSize();
    QString upcomingEventDateText(
        const CalendarEvent& event
        ) const;
    QString upcomingEventTimeText(
        const CalendarEvent& event
        ) const;
    bool calendarEventVisible(
        const CalendarEvent& event
        ) const;
    bool showAllCalendarCampuses() const;
    bool hideStartOfTermEvents() const;
    QStringList currentCampusCodes() const;
    QStringList allCampusCodes() const;
    QWidget* createUpcomingEventRow(
        const CalendarEvent& event,
        int dateColumnWidth,
        int timeColumnWidth,
        int eventTypeColumnWidth,
        QWidget* parent
        );

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
    QLabel* m_monthlyCalendarHeading = nullptr;
    QLabel* m_upcomingEventsHeading = nullptr;

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

    CalendarEventModel* m_calendarModel = nullptr;
    AcademicCalendarProvider* m_academicCalendarProvider = nullptr;
    QQuickWidget* m_calendarView = nullptr;
    QTabWidget* m_upcomingEventsTabs = nullptr;
    std::array<QVBoxLayout*, UpcomingEventsScopeCount> m_upcomingEventLayouts{};
    QList<QPushButton*> m_eventTypeFilterButtons;
    QHash<QString, bool> m_eventTypeFilterStates;
    QDate m_calendarVisibleMonth;

    QTimer* m_autosaveTimer = nullptr;
};
