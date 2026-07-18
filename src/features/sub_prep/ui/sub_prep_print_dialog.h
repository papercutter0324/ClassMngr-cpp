#pragma once

#include "domain/models/calendar_event.h"
#include "features/schedule/ui/schedule_view_model.h"

#include <QDate>
#include <QDialog>
#include <QList>
#include <QStringList>

class ApplicationServices;
class QCheckBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QWidget;

class SubPrepPrintDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SubPrepPrintDialog(
        ApplicationServices* services,
        const ScheduleViewModel& schedule,
        const QList<CalendarEvent>& calendarEvents,
        const QDate& referenceDate = QDate::currentDate(),
        QWidget* parent = nullptr
        );

    explicit SubPrepPrintDialog(
        const QList<CalendarEvent>& calendarEvents,
        const QDate& referenceDate = QDate::currentDate(),
        QWidget* parent = nullptr
        );

    [[nodiscard]] QList<QDate> selectedDates() const;
    [[nodiscard]] QStringList selectedDays() const;
    [[nodiscard]] QList<int> selectedClassIds() const;
    [[nodiscard]] bool createFolder() const;
    [[nodiscard]] QString targetRoot() const;
    [[nodiscard]] QString userName() const;
    [[nodiscard]] bool printPaperCopies() const;
    [[nodiscard]] bool openFolderAfterGeneration() const;
    [[nodiscard]] bool replaceExisting() const;
    [[nodiscard]] QString outputDirectory() const;

    [[nodiscard]] static QStringList defaultSelectedDays(
        const QList<CalendarEvent>& calendarEvents,
        const QDate& referenceDate
        );

    [[nodiscard]] static QDate defaultWeekStart(
        const QList<CalendarEvent>& calendarEvents,
        const QDate& referenceDate
        );

private:
    void buildUi();
    void initializeDays(
        const QList<CalendarEvent>& calendarEvents,
        const QDate& referenceDate
        );
    void updateFolderControls();
    void updateOutputPreview();
    void updateAcceptEnabled();
    void chooseTargetRoot();
    void acceptGeneration();
    QStringList vacationDaysForWeek(
        const QDate& weekStart
        ) const;

    ApplicationServices* m_services = nullptr;
    ScheduleViewModel m_schedule;
    QList<CalendarEvent> m_initialCalendarEvents;
    bool m_replaceExisting = false;
    QString m_storedUserName;
    QDate m_weekStart;
    QList<QCheckBox*> m_dayChecks;

    QCheckBox* m_createFolderCheck = nullptr;
    QWidget* m_folderOptions = nullptr;
    QLineEdit* m_targetRootEdit = nullptr;
    QLabel* m_nameLabel = nullptr;
    QLineEdit* m_nameEdit = nullptr;
    QLabel* m_outputPreviewLabel = nullptr;
    QCheckBox* m_openFolderCheck = nullptr;
    QCheckBox* m_printPaperCheck = nullptr;
    QLabel* m_validationLabel = nullptr;
    QPushButton* m_okButton = nullptr;
};
