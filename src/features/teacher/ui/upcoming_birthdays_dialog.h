#pragma once

#include "features/teacher/upcoming_birthday_schedule.h"
#include "ui/shared/dialogs/dialog_shell.h"

#include <QList>

class QLabel;
class QWidget;

class UpcomingBirthdaysDialog final : public DialogShell
{
    Q_OBJECT

public:
    explicit UpcomingBirthdaysDialog(
        const UpcomingBirthdaySchedule& schedule,
        QWidget* parent = nullptr
        );

protected:
    void retranslateDialog() override;

private:
    struct EntryPresentation
    {
        UpcomingBirthday birthday;
        QLabel* name = nullptr;
        QLabel* detail = nullptr;
    };

    QWidget* createSection(
        const QString& objectPrefix,
        const QList<UpcomingBirthday>& birthdays,
        bool centered
        );
    QWidget* createEntry(
        const QString& objectPrefix,
        int index,
        const UpcomingBirthday& birthday,
        bool centered
        );
    [[nodiscard]] QString detailText(const UpcomingBirthday& birthday) const;
    [[nodiscard]] QString groupText(UpcomingBirthdayGroup group) const;
    void updateText();

    UpcomingBirthdaySchedule m_schedule;
    QWidget* m_todaySection = nullptr;
    QLabel* m_todayTitle = nullptr;
    QLabel* m_thisWeekTitle = nullptr;
    QLabel* m_nextWeekTitle = nullptr;
    QLabel* m_thisWeekEmpty = nullptr;
    QLabel* m_nextWeekEmpty = nullptr;
    QList<EntryPresentation> m_entries;
};
