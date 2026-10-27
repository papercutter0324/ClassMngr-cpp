#pragma once

#include "domain/models/schedule_import.h"
#include "next/application/schedule_import_apply_request.h"
#include "next/application/schedule_import_state_snapshot.h"

#include <QDialog>
#include <QByteArray>
#include <QList>
#include <QString>

#include <functional>
#include <expected>
#include <optional>
#include <cstdint>

class QCheckBox;
class QComboBox;
class QDialogButtonBox;
class QLabel;
class QLineEdit;
class QProgressBar;
class QPushButton;

namespace ClassMngr::Next::Ui
{

struct InitialSetupScheduleImportActions final
{
    std::function<Domain::Result<std::optional<QString>>()> chooseWorkbook;
    std::function<std::expected<ScheduleImportWorkbook, QString>(
        const QByteArray&,
        ScheduleImportKind
        )> parseWorkbook;
    std::function<Application::ScheduleImportStateSnapshotOutcome()> readState;
    std::function<Application::ScheduleImportApplyResult(
        const Application::ScheduleImportApplyRequest&)> apply;
    std::function<int()> validationTimeoutMilliseconds;
    std::function<void()> cancelSetup;
};

class InitialSetupScheduleImportDialog final : public QDialog
{
    Q_OBJECT

public:
    explicit InitialSetupScheduleImportDialog(
        InitialSetupScheduleImportActions actions,
        QString profileName,
        QWidget* parent = nullptr
        );

    [[nodiscard]] QString successSummary() const;

public slots:
    void reject() override;

private:
    struct ParseResult;

    void browseForWorkbook();
    void invalidateWorkbook();
    void parseSelectedWorkbook();
    void finishParse(ParseResult result);
    void populateVisibleSheets();
    void populateUsers();
    void updateSelectedUser();
    void openReview();
    void updateSourceControls();
    void setStatus(const QString& text);
    void setLoading(bool loading);

    InitialSetupScheduleImportActions m_actions;
    QString m_profileName;
    QLineEdit* m_filePathEdit = nullptr;
    QPushButton* m_browseButton = nullptr;
    QComboBox* m_kindCombo = nullptr;
    QPushButton* m_loadButton = nullptr;
    QLabel* m_statusLabel = nullptr;
    QLabel* m_userStatusLabel = nullptr;
    QLabel* m_diagnosticsLabel = nullptr;
    QProgressBar* m_progressBar = nullptr;
    QComboBox* m_sheetCombo = nullptr;
    QComboBox* m_userCombo = nullptr;
    QCheckBox* m_updateProfileNameCheck = nullptr;
    QDialogButtonBox* m_buttons = nullptr;
    QPushButton* m_nextButton = nullptr;
    QString m_successSummary;
    std::optional<ScheduleImportWorkbook> m_workbook;
    QList<int> m_visibleSheetIndexes;
    quint64 m_parseRequestId = 0;
    qint64 m_loadedSize = -1;
    qint64 m_loadedModified = -1;
    bool m_loading = false;
    bool m_cancelSetupCalled = false;
};

} // namespace ClassMngr::Next::Ui
