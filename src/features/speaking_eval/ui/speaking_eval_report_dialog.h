#pragma once

#include "domain/models/class_info.h"
#include "domain/models/speaking_evaluation.h"
#include "features/speaking_eval/ui/speaking_eval_report_widget.h"

#include <QDialog>

class QComboBox;

class SpeakingEvalReportDialog : public QDialog
{
public:
    explicit SpeakingEvalReportDialog(
        const SpeakingEvalRows& rows,
        const ClassInfo& classInfo,
        QWidget* parent = nullptr
        );

private:
    void updateReport();

    void moveToPreviousStudent();

    void moveToNextStudent();

    void printCurrentReport();

    SpeakingEvalReportData reportDataForRow(
        int row
        ) const;

    QString studentDisplayName(
        int row
        ) const;

private:
    SpeakingEvalRows m_rows;
    ClassInfo m_classInfo;
    QComboBox* m_studentSelector = nullptr;
    SpeakingEvalReportWidget* m_report = nullptr;
};
