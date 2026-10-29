#pragma once
#include "next/output/speaking_evaluation_report_output.h"
#include <QDialog>
#include <functional>
#include <memory>

class QComboBox;
class QCheckBox;
class QLineEdit;
class QLabel;
class QPushButton;
class SpeakingEvalReportWidget;
class SpeakingEvalPrivateNotesEditor;
namespace ClassMngr::Next::Ui
{
struct SpeakingEvaluationReportActions
{
    SpeakingEvalReportAssetEnvironment assets;
    SpeakingEvalBatchReportService::Request engineOverrides;
    Output::SpeakingEvaluationReportRunActions run;
    std::function<QString(QWidget*, const QString&)> choosePdfPath;
    std::function<QString(QWidget*, const QString&)> chooseOutputDirectory;
    std::function<bool(QWidget*, int)> confirmOverwrite;
    std::function<void(QWidget*, bool, const QString&, const QString&)> showNotice;
    std::function<void(const QString&)> openOutputFolder;
    std::function<void(int, int, const QString&)> valueEdited;
    std::function<void(int)> previewAiPrompt;
    std::function<void(int)> copyAndOpenAiPrompt;
    bool showPowerPointDataAccessNotice = true;
};

class SpeakingEvaluationReportsDialog final : public QDialog
{
public:
    SpeakingEvaluationReportsDialog(QList<SpeakingEvalBatchReportService::StudentReport> reports,
        int current, SpeakingEvaluationReportActions actions, QWidget* parent = nullptr,
        bool interactive = true);
    ~SpeakingEvaluationReportsDialog() override;
    [[nodiscard]] QComboBox* studentSelector() const { return m_students; }
    [[nodiscard]] SpeakingEvalReportWidget* reportWidget() const { return m_report; }
    [[nodiscard]] const QList<SpeakingEvalBatchReportService::StudentReport>& reports() const { return m_reports; }
private:
    void updateReport();
    void saveCurrent();
    void printCurrent();
    void edit(int column, const QString& value);
    QList<SpeakingEvalBatchReportService::StudentReport> m_reports;
    SpeakingEvaluationReportActions m_actions;
    std::unique_ptr<SpeakingEvalReportAssetSession> m_assets;
    QComboBox* m_students = nullptr;
    SpeakingEvalReportWidget* m_report = nullptr;
    SpeakingEvalPrivateNotesEditor* m_notes = nullptr;
};

class SpeakingEvaluationReportBatchDialog final : public QDialog
{
public:
    enum class Mode { SaveAs, Print };
    SpeakingEvaluationReportBatchDialog(const QList<SpeakingEvalBatchReportService::StudentReport>& reports,
        int current, QString directory, Mode mode, SpeakingEvaluationReportActions actions,
        QWidget* parent = nullptr);
    [[nodiscard]] QComboBox* scopeSelector() const { return m_scope; }
    [[nodiscard]] QComboBox* rendererSelector() const { return m_renderer; }
    [[nodiscard]] QLineEdit* outputDirectoryEdit() const { return m_directory; }
    [[nodiscard]] QCheckBox* keepIndividualCheck() const { return m_keep; }
    [[nodiscard]] QPushButton* exportButton() const { return m_export; }
    [[nodiscard]] QLabel* statusLabel() const { return m_status; }
    [[nodiscard]] const Output::SpeakingEvaluationReportRunResult& lastResult() const { return m_lastResult; }
private:
    [[nodiscard]] QList<SpeakingEvalBatchReportService::StudentReport> selectedReports() const;
    void updateControls();
    void exportReports();
    const QList<SpeakingEvalBatchReportService::StudentReport>& m_reports;
    int m_current;
    Mode m_mode;
    SpeakingEvaluationReportActions m_actions;
    QComboBox* m_scope = nullptr;
    QComboBox* m_renderer = nullptr;
    QLineEdit* m_directory = nullptr;
    QCheckBox* m_keep = nullptr;
    QCheckBox* m_open = nullptr;
    QLabel* m_note = nullptr;
    QLabel* m_status = nullptr;
    QPushButton* m_export = nullptr;
    Output::SpeakingEvaluationReportRunResult m_lastResult;
};
}
