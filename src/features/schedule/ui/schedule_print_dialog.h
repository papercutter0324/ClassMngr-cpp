#pragma once

#include "features/schedule/ui/schedule_print_style.h"

#include <QPageLayout>
#include <QDialog>

class QRadioButton;

class SchedulePrintDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SchedulePrintDialog(
        QWidget* parent = nullptr
        );

    [[nodiscard]] SchedulePrintStyle selectedStyle() const;
    [[nodiscard]] QPageLayout::Orientation selectedOrientation() const;

private:
    QRadioButton* m_currentAppearanceButton = nullptr;
    QRadioButton* m_lightThemeButton = nullptr;
    QRadioButton* m_darkThemeButton = nullptr;
    QRadioButton* m_excelButton = nullptr;
    QRadioButton* m_landscapeButton = nullptr;
    QRadioButton* m_portraitButton = nullptr;
};
