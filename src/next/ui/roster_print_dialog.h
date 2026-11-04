#pragma once
#include "next/ui/roster_output_actions.h"
#include "ui/shared/dialogs/dialog_shell.h"
#include <optional>
class QComboBox;
class QListWidget;
class QLabel;
namespace ClassMngr::Next::Ui
{
class RosterPrintDialog final : public DialogShell
{
public:
    RosterPrintDialog(QString databasePath, Domain::ClassId current, bool testing,
        Output::RosterOutputChoices choices, RosterOutputActions actions,
        DialogGeometryPreferencesCallbacks geometryPreferences,
        QWidget* parent = nullptr);
    [[nodiscard]] Output::RosterOutputRequest request() const;
private:
    void refreshOptions();
    void preview();
    void save();
    void print();
    bool prepareRequest(Output::RosterOutputRequest* selected);
    [[nodiscard]] DocumentOutputResult generate(
        const Output::RosterOutputRequest& selected,
        const QString& outputPath
        ) const;
    [[nodiscard]] std::optional<QString> chooseSavePath(
        QWidget* parent,
        QString* failureMessage
        ) const;
    void showResult(const DocumentOutputResult& result);
    DialogGeometryPreferencesCallbacks m_geometryPreferences;
    QString m_path;
    Domain::ClassId m_current;
    bool m_testing;
    Output::RosterOutputChoices m_choices;
    RosterOutputActions m_actions;
    QComboBox* m_template;
    QComboBox* m_scope;
    QComboBox* m_orientation;
    QListWidget* m_classes;
    QListWidget* m_extra;
    QLabel* m_status;
};
}
