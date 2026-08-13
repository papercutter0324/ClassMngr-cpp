#pragma once

#include <Qt>

class QComboBox;
class QFrame;
class QLayout;
class QString;
class QVBoxLayout;
class QWidget;

namespace ScheduleImportResolutionView
{
constexpr int ActionRole = Qt::UserRole;
constexpr int TargetRole = Qt::UserRole + 1;

void addResolutionItem(
    QComboBox* combo,
    const QString& text,
    int action,
    int target = -1
    );
void configureCompactActionCombo(QComboBox* combo);
QFrame* createReconciliationCard(
    QWidget* parent,
    const QString& objectName
    );
void addLabeledControlRow(
    QVBoxLayout* layout,
    const QString& labelText,
    QWidget* control,
    QWidget* parent
    );
void clearLayout(QLayout* layout);
int findActionIndex(
    QComboBox* combo,
    int action,
    int target = -2
    );
}
