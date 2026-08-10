#pragma once

#include "features/schedule/ui/schedule_print_style.h"

#include <QDialog>
#include <QPageLayout>
#include <QString>

class QRadioButton;

class SchedulePrintDialog : public QDialog
{
    Q_OBJECT

public:
    enum class Action
    {
        Print,
        SaveAs
    };

    explicit SchedulePrintDialog(
        Action action,
        QWidget* parent = nullptr
        );

    [[nodiscard]] Action selectedAction() const;
    [[nodiscard]] QString selectedSavePath() const;
    [[nodiscard]] SchedulePrintStyle selectedStyle() const;
    [[nodiscard]] QPageLayout::Orientation selectedOrientation() const;

private:
    void acceptPrint();
    void chooseSavePath();

    Action m_selectedAction = Action::Print;
    QString m_selectedSavePath;
    QRadioButton* m_currentAppearanceButton = nullptr;
    QRadioButton* m_lightThemeButton = nullptr;
    QRadioButton* m_darkThemeButton = nullptr;
    QRadioButton* m_excelButton = nullptr;
    QRadioButton* m_landscapeButton = nullptr;
    QRadioButton* m_portraitButton = nullptr;
};
