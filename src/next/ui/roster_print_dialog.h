#pragma once
#include "next/ui/roster_output_actions.h"
#include <QDialog>
#include <QTemporaryDir>
class QComboBox;
class QListWidget;
class QLabel;
namespace ClassMngr::Next::Ui
{
class RosterPrintDialog final : public QDialog
{
public:
    RosterPrintDialog(QString databasePath, Domain::ClassId current, bool testing,
        Output::RosterOutputChoices choices, RosterOutputActions actions, QWidget* parent = nullptr);
    [[nodiscard]] Output::RosterOutputRequest request() const;
private:
    void refreshOptions();
    void preview();
    void save();
    void print();
    QString generate();
    void showResult(const DocumentOutputResult& result);
    QString m_path;
    Domain::ClassId m_current;
    bool m_testing;
    Output::RosterOutputChoices m_choices;
    RosterOutputActions m_actions;
    QTemporaryDir m_temporary;
    QComboBox* m_template;
    QComboBox* m_scope;
    QComboBox* m_orientation;
    QListWidget* m_classes;
    QListWidget* m_extra;
    QLabel* m_status;
};
}
