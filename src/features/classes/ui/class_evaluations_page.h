#pragma once

#include "domain/models/classroom.h"
#include "ui/shared/pages/basepage.h"

#include <QString>

class ApplicationServices;
class QComboBox;
class QLabel;
class QPushButton;
class QTableView;
class SpeakingEvalModel;

// Read-only, in-context view of the selected class's speaking evaluation.
class ClassEvaluationsPage : public BasePage
{
    Q_OBJECT

public:
    explicit ClassEvaluationsPage(
        ApplicationServices* services,
        bool embedded = false,
        QWidget* parent = nullptr
        );

    void loadClass(const Classroom& classroom);

    void clearDatabaseState() override;
    void refresh() override;
    void retranslateUi() override;

private slots:
    void onEvaluationChanged();

private:
    void buildUi();
    void rebuild();
    void setupTable();

    [[nodiscard]] QString selectedEvaluationName() const;

    ApplicationServices* m_services = nullptr;
    bool m_embedded = false;
    int m_classId = -1;
    bool m_rebuilding = false;

    QLabel* m_heading = nullptr;
    QLabel* m_evaluationLabel = nullptr;
    QComboBox* m_evaluationCombo = nullptr;
    QPushButton* m_importNamesButton = nullptr;
    QPushButton* m_reportEditorButton = nullptr;
    QPushButton* m_generateCommentsButton = nullptr;
    SpeakingEvalModel* m_model = nullptr;
    QTableView* m_table = nullptr;
};
