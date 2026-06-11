#pragma once

#include "../basepage.h"

#include "models/classroom.h"
#include "models/speaking_evaluation.h"
#include "ui/pages/speakingeval/speaking_eval_table_view.h"

#include <QList>
#include <QString>
#include <QStringList>

class ApplicationServices;
class QLabel;
class QModelIndex;
class QPushButton;
class QUndoStack;
class SpeakingEvalDelegate;
class SpeakingEvalModel;

class SpeakingEvalPage : public BasePage
{
    Q_OBJECT

public:
    explicit SpeakingEvalPage(
        ApplicationServices* services,
        QWidget* parent = nullptr
        );

    void loadEvaluation(
        const Classroom& classroom,
        const QString& evaluationName
        );

    void saveData() override;

    void refresh() override;

private slots:
    void importNames();

    void openKoreanKeyboard();

    void updateActions();

private:
    void buildUi();

    void setupTable();

    void updateHeaderText();

    QList<SpeakingEvalCellEdit> nameImportChanges(
        const QStringList& rosterColumns,
        const QList<QStringList>& rosterRows
        ) const;

    void handleNameCellChanged(
        const QModelIndex& topLeft,
        const QModelIndex& bottomRight
        );

    void resolveDuplicateName(
        int row,
        int editedColumn
        );

    QList<QStringList> unmatchedRosterNamePairs() const;

    void selectEvaluationCell(
        int row,
        SpeakingEvalColumn column
        );

private:
    ApplicationServices* m_services = nullptr;
    Classroom m_classroom;
    QString m_evaluationName;
    bool m_loadingEvaluation = false;
    bool m_importingNames = false;
    bool m_resolvingDuplicateName = false;

    QLabel* m_titleLabel = nullptr;
    QLabel* m_subtitleLabel = nullptr;

    SpeakingEvalModel* m_model = nullptr;
    SpeakingEvalTableView* m_table = nullptr;
    SpeakingEvalDelegate* m_delegate = nullptr;
    QUndoStack* m_undoStack = nullptr;

    QPushButton* m_importNamesButton = nullptr;
    QPushButton* m_koreanKeyboardButton = nullptr;
    QPushButton* m_saveButton = nullptr;
};
