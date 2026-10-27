#pragma once

#include "next/application/upcoming_birthday_schedule_use_case.h"

#include <QDialog>
#include <QList>

class QCheckBox;
class QEvent;
class QLabel;
class QVBoxLayout;
class QWidget;

namespace ClassMngr::Next::Ui
{

class UpcomingBirthdaysDialog final : public QDialog
{
    Q_OBJECT

public:
    explicit UpcomingBirthdaysDialog(
        const Application::UpcomingBirthdaySchedule& schedule,
        QWidget* parent = nullptr
        );

    [[nodiscard]] bool dismissForToday() const;

protected:
    void changeEvent(QEvent* event) override;

private:
    struct EntryPresentation final
    {
        Application::UpcomingBirthdayOccurrence birthday;
        QLabel* name = nullptr;
        QLabel* detail = nullptr;
    };

    QWidget* createSection(
        const QString& objectPrefix,
        const std::vector<Application::UpcomingBirthdayOccurrence>& birthdays,
        bool centered
        );
    QWidget* createEntry(
        const QString& objectPrefix,
        int index,
        const Application::UpcomingBirthdayOccurrence& birthday,
        bool centered
        );
    [[nodiscard]] QString detailText(
        const Application::UpcomingBirthdayOccurrence& birthday
        ) const;
    [[nodiscard]] QString groupText(
        Application::UpcomingBirthdayStaffGroup group
        ) const;
    void updateText();

    Application::UpcomingBirthdaySchedule m_schedule;
    QVBoxLayout* m_dialogLayout = nullptr;
    QLabel* m_headerTitle = nullptr;
    QLabel* m_headerSubtitle = nullptr;
    QWidget* m_todaySection = nullptr;
    QLabel* m_todayTitle = nullptr;
    QLabel* m_thisWeekTitle = nullptr;
    QLabel* m_nextWeekTitle = nullptr;
    QLabel* m_thisWeekEmpty = nullptr;
    QLabel* m_nextWeekEmpty = nullptr;
    QCheckBox* m_dismissForToday = nullptr;
    QList<EntryPresentation> m_entries;
};

} // namespace ClassMngr::Next::Ui
