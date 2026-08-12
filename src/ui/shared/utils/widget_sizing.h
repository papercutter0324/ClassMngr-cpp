#pragma once

#include <QSizePolicy>
#include <QStringList>

class QComboBox;
class QDateTimeEdit;
class QLabel;
class QLineEdit;
class QString;
class QWidget;

namespace WidgetSizing
{
inline constexpr int LineEditTextPadding = 28;
inline constexpr int ComboBoxTextPadding = 8;
inline constexpr int ComboBoxFallbackChromeWidth = 56;
inline constexpr int DateTimeEditTextPadding = 10;
inline constexpr int DateTimeEditFallbackChromeWidth = 48;

int textWidth(const QWidget* widget, const QString& text);
int labelMinimumWidth(const QLabel* label);
int comboChromeWidth(const QComboBox* combo, int probeWidth = 200);
int comboMinimumWidthForTexts(
    const QComboBox* combo,
    const QStringList& texts,
    int padding = 10
    );
void applyInitialFieldWidth(
    QWidget* widget,
    int width,
    QSizePolicy::Policy horizontalPolicy = QSizePolicy::Expanding
    );
int lineEditMinimumWidthForText(
    const QLineEdit* edit,
    int minimumWidth,
    int padding = LineEditTextPadding
    );
int comboMinimumWidthForText(
    const QComboBox* combo,
    int minimumWidth,
    int padding = ComboBoxTextPadding
    );
int dateTimeEditChromeWidth(
    const QDateTimeEdit* edit,
    int probeWidth = 200
    );
int dateTimeEditMinimumWidthForText(
    const QDateTimeEdit* edit,
    int minimumWidth,
    int padding = DateTimeEditTextPadding
    );
void updateTextAwareFieldWidth(
    QWidget* widget,
    int minimumWidth,
    bool lockToCalculatedWidth = false
    );
void installTextAwareFieldWidth(
    QWidget* widget,
    int minimumWidth,
    QSizePolicy::Policy horizontalPolicy = QSizePolicy::Expanding,
    bool lockToCalculatedWidth = false
    );
} // namespace WidgetSizing
