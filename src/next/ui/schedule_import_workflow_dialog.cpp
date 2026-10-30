#include "next/ui/schedule_import_workflow_dialog.h"

#include "features/schedule/import/schedule_workbook_parser.h"
#include "next/application/schedule_import_review_summary_projection.h"
#include "next/application/schedule_import_schedules_cleared_projection.h"
#include "next/ui/schedule_import_review_models.h"

#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDateTime>
#include <QDialogButtonBox>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QRadioButton>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QSplitter>
#include <QStackedWidget>
#include <QTabWidget>
#include <QTableView>
#include <QTextEdit>
#include <QTimer>
#include <QVBoxLayout>
#include <QFutureWatcher>
#include <QtConcurrentRun>

#include <algorithm>
#include <atomic>
#include <exception>
#include <memory>
#include <utility>

namespace ClassMngr::Next::Ui
{

struct ScheduleImportParseTaskResult final
{
    QString path;
    ScheduleImportKind kind = ScheduleImportKind::Normal;
    std::optional<ScheduleImportWorkbook> workbook;
    QString error;
    bool canceled = false;
    bool sourceMetadataStable = false;
};

namespace
{

constexpr int MaximumPreviewVisibleRows = 6;
constexpr QSize SourceAndSelectionDialogSize{436, 520};
constexpr QSize ReviewDialogSize{800, 800};

QString workflowText(const char* source)
{
    return QCoreApplication::translate("ScheduleImportWorkflowDialog", source);
}

QString reviewText(const char* source)
{
    return QCoreApplication::translate("ScheduleImportReview", source);
}

QString diagnosticText(const ScheduleImportDiagnostic& diagnostic)
{
    QString location;
    if (!diagnostic.sheetName.isEmpty())
    {
        location = diagnostic.sheetName;
    }
    if (!diagnostic.cellReference.isEmpty())
    {
        location += location.isEmpty()
            ? diagnostic.cellReference
            : QStringLiteral("!") + diagnostic.cellReference;
    }
    QString line = diagnostic.message;
    if (!diagnostic.value.isEmpty())
    {
        line += QStringLiteral(" — ") + diagnostic.value;
    }
    if (!diagnostic.userName.isEmpty())
    {
        line.prepend(diagnostic.userName + QStringLiteral(" · "));
    }
    if (!location.isEmpty())
    {
        line.prepend(location + QStringLiteral(" · "));
    }
    return line;
}

QString applyFailureText(const Application::ScheduleImportApplyFailure& failure)
{
    const QString detail = QString::fromStdU16String(failure.message).trimmed();
    const QString summary = workflowText("The schedule could not be imported.");
    if (failure.code == Domain::ErrorCode::Technical)
    {
        return detail.isEmpty() || detail == summary
            ? summary
            : summary + QLatin1Char('\n') + detail;
    }
    return detail.isEmpty() ? summary : detail;
}

QString utf8(const std::string& value)
{
    return QString::fromUtf8(value.data(), static_cast<qsizetype>(value.size()));
}

QString decisionStatus(
    const Application::ScheduleImportReviewReadinessResult& readiness
    )
{
    if (!readiness.decisions.accepted())
    {
        return reviewText("Resolve the required teacher and class choices.");
    }
    if (readiness.stateError)
    {
        using Code = Application::ScheduleImportStateValidationErrorCode;
        switch (readiness.stateError->code)
        {
        case Code::SelectedTeacherUnavailable:
        case Code::InvalidTeacherTarget:
            return reviewText("Choose an available teacher match.");
        case Code::MissingTeacherRoom:
            return reviewText("Choose an imported room for each room update.");
        case Code::SelectedClassUnavailable:
            return reviewText("Choose an available class match.");
        case Code::ProjectedScheduleOverlap:
            return reviewText("The proposed schedule contains an overlap.");
        case Code::InvalidProjectedTime:
            return reviewText("The proposed schedule contains an invalid time.");
        case Code::CreateNewClassHasTarget:
        case Code::SkippedClassNotUniqueExactMatch:
            return reviewText("Review the selected class targets.");
        case Code::InvalidStateKind:
        case Code::InvalidIntensiveMode:
        case Code::InvalidTeacherAction:
        case Code::InvalidClassAction:
        case Code::InvalidClassCandidateIndex:
            return reviewText("The review choices are invalid.");
        }
    }
    return reviewText("All required review choices are complete.");
}

class CancelableFileDevice final : public QIODevice
{
public:
    CancelableFileDevice(
        const QString& path,
        std::shared_ptr<std::atomic_bool> canceled
        )
        : m_file(path)
        , m_canceled(std::move(canceled))
    {
    }

    bool open(const OpenMode mode) override
    {
        if (!m_file.open(mode))
        {
            setErrorString(m_file.errorString());
            return false;
        }
        if (!QIODevice::open(mode))
        {
            m_file.close();
            return false;
        }
        return true;
    }

    [[nodiscard]] bool isSequential() const override
    {
        return false;
    }

    [[nodiscard]] qint64 size() const override
    {
        return m_file.size();
    }

    [[nodiscard]] qint64 pos() const override
    {
        return m_file.pos();
    }

    bool seek(const qint64 position) override
    {
        if (isCanceled() || !m_file.seek(position))
        {
            return false;
        }
        return QIODevice::seek(position);
    }

protected:
    qint64 readData(char* data, const qint64 maximumSize) override
    {
        if (isCanceled())
        {
            setErrorString(QStringLiteral("Workbook parsing was canceled."));
            return -1;
        }
        return m_file.read(data, maximumSize);
    }

    qint64 writeData(const char*, const qint64) override
    {
        return -1;
    }

private:
    [[nodiscard]] bool isCanceled() const noexcept
    {
        return m_canceled && m_canceled->load(std::memory_order_relaxed);
    }

    QFile m_file;
    std::shared_ptr<std::atomic_bool> m_canceled;
};

QString workbookCounts(const ScheduleImportWorkbook& workbook)
{
    int sheetCount = 0;
    int userCount = 0;
    int candidateCount = 0;
    for (const ScheduleImportSheet& sheet : workbook.sheets)
    {
        if (!sheet.visible)
        {
            continue;
        }
        ++sheetCount;
        userCount += sheet.users.size();
        for (const ScheduleImportUserBlock& user : sheet.users)
        {
            candidateCount += user.classes.size();
        }
    }
    return workflowText("Workbook loaded: %1 visible sheets, %2 schedule blocks, %3 classes.")
        .arg(sheetCount).arg(userCount).arg(candidateCount);
}

} // namespace

ScheduleImportWorkflowDialog::ScheduleImportWorkflowDialog(
    ScheduleImportWorkflowActions actions,
    QString expectedWorkspacePath,
    QString expectedWorkspaceIdentity,
    QWidget* parent
    )
    : QDialog(parent)
    , m_actions(std::move(actions))
    , m_expectedWorkspacePath(std::move(expectedWorkspacePath))
    , m_expectedWorkspaceIdentity(std::move(expectedWorkspaceIdentity))
{
    setObjectName(QStringLiteral("nextScheduleImportWorkflow"));
    setModal(false);
    setFixedSize(SourceAndSelectionDialogSize);
    buildUi();
    retranslateUi();
}

ScheduleImportWorkflowDialog::~ScheduleImportWorkflowDialog()
{
    releaseRetainedData();
}

QLineEdit* ScheduleImportWorkflowDialog::sourcePathEdit() const noexcept
{
    return m_sourcePath;
}

QComboBox* ScheduleImportWorkflowDialog::sheetCombo() const noexcept
{
    return m_sheet;
}

QComboBox* ScheduleImportWorkflowDialog::userCombo() const noexcept
{
    return m_user;
}

QTableView* ScheduleImportWorkflowDialog::teacherTable() const noexcept
{
    return m_teacherTable;
}

QTableView* ScheduleImportWorkflowDialog::classTable() const noexcept
{
    return m_classTable;
}

QPushButton* ScheduleImportWorkflowDialog::parseButton() const noexcept
{
    return m_primaryButton && m_pages->currentIndex() == 0
        ? m_primaryButton : nullptr;
}

QPushButton* ScheduleImportWorkflowDialog::primaryButton() const noexcept
{
    return m_primaryButton;
}

QLabel* ScheduleImportWorkflowDialog::statusLabel() const noexcept
{
    return m_status;
}

int ScheduleImportWorkflowDialog::reviewWidgetCount() const
{
    return findChildren<QWidget*>().size();
}

void ScheduleImportWorkflowDialog::reject()
{
    releaseRetainedData();
    QDialog::reject();
}

void ScheduleImportWorkflowDialog::changeEvent(QEvent* event)
{
    if (event && event->type() == QEvent::LanguageChange)
    {
        retranslateUi();
    }
    QDialog::changeEvent(event);
}

void ScheduleImportWorkflowDialog::buildUi()
{
    auto* const root = new QVBoxLayout(this);
    root->setContentsMargins(14, 14, 14, 12);
    root->setSpacing(9);

    m_pages = new QStackedWidget(this);
    m_pages->setObjectName(QStringLiteral("nextScheduleImportPages"));
    root->addWidget(m_pages, 1);

    auto* const sourcePage = new QWidget(m_pages);
    sourcePage->setObjectName(QStringLiteral("nextScheduleImportSourcePage"));
    auto* const sourceLayout = new QVBoxLayout(sourcePage);
    auto* const sourceTitle = new QLabel(sourcePage);
    sourceTitle->setObjectName(QStringLiteral("nextScheduleImportSourceTitle"));
    sourceTitle->setProperty("translationSource", QStringLiteral("Import Schedule"));
    sourceTitle->setStyleSheet(QStringLiteral("font-size:18px;font-weight:600;"));
    sourceLayout->addWidget(sourceTitle);
    m_status = new QLabel(sourcePage);
    m_status->setObjectName(QStringLiteral("nextScheduleImportSourceStatus"));
    m_status->setWordWrap(true);
    sourceLayout->addWidget(m_status);

    auto* const fileBox = new QGroupBox(sourcePage);
    fileBox->setObjectName(QStringLiteral("nextScheduleImportFileBox"));
    fileBox->setTitle(workflowText("Choose a spreadsheet"));
    auto* const fileRow = new QHBoxLayout(fileBox);
    m_sourcePath = new QLineEdit(fileBox);
    m_sourcePath->setObjectName(QStringLiteral("nextScheduleImportSourcePath"));
    m_sourcePath->setReadOnly(true);
    m_browseButton = new QPushButton(fileBox);
    m_browseButton->setObjectName(QStringLiteral("nextScheduleImportBrowse"));
    m_browseButton->setText(workflowText("Browse…"));
    fileRow->addWidget(m_sourcePath, 1);
    fileRow->addWidget(m_browseButton);
    sourceLayout->addWidget(fileBox);

    auto* const typeBox = new QGroupBox(sourcePage);
    typeBox->setObjectName(QStringLiteral("nextScheduleImportTypeBox"));
    typeBox->setTitle(workflowText("Schedule type"));
    auto* const typeLayout = new QHBoxLayout(typeBox);
    m_kind = new QComboBox(typeBox);
    m_kind->setObjectName(QStringLiteral("nextScheduleImportKind"));
    m_kind->addItem(workflowText("Regular"), static_cast<int>(ScheduleImportKind::Normal));
    m_kind->addItem(workflowText("Intensive"), static_cast<int>(ScheduleImportKind::Intensive));
    typeLayout->addWidget(m_kind);
    typeLayout->addStretch(1);
    sourceLayout->addWidget(typeBox);
    sourceLayout->addStretch(1);
    m_pages->addWidget(sourcePage);

    auto* const selectionPage = new QWidget(m_pages);
    selectionPage->setObjectName(QStringLiteral("nextScheduleImportSelectionPage"));
    auto* const selectionLayout = new QVBoxLayout(selectionPage);
    auto* const selectionTitle = new QLabel(selectionPage);
    selectionTitle->setProperty("translationSource",
                                QStringLiteral("Select a schedule block to continue."));
    selectionTitle->setStyleSheet(QStringLiteral("font-size:18px;font-weight:600;"));
    selectionLayout->addWidget(selectionTitle);
    m_selectionStatus = new QLabel(selectionPage);
    m_selectionStatus->setWordWrap(true);
    m_selectionStatus->setText(workflowText(
        "Choose a visible worksheet and the schedule block to review."));
    selectionLayout->addWidget(m_selectionStatus);
    auto* const selectionForm = new QFormLayout();
    selectionForm->setObjectName(QStringLiteral("nextScheduleImportSelectionForm"));
    m_sheet = new QComboBox(selectionPage);
    m_sheet->setObjectName(QStringLiteral("nextScheduleImportSheet"));
    m_user = new QComboBox(selectionPage);
    m_user->setObjectName(QStringLiteral("nextScheduleImportUser"));
    selectionForm->addRow(workflowText("Worksheet"), m_sheet);
    selectionForm->addRow(workflowText("Schedule block"), m_user);
    selectionLayout->addLayout(selectionForm);
    selectionLayout->addStretch(1);
    m_pages->addWidget(selectionPage);

    auto* const reviewPage = new QWidget(m_pages);
    reviewPage->setObjectName(QStringLiteral("nextScheduleImportReviewPage"));
    auto* const reviewLayout = new QVBoxLayout(reviewPage);
    reviewLayout->setContentsMargins(0, 0, 0, 0);
    reviewLayout->setSpacing(7);
    m_reviewTitle = new QLabel(reviewPage);
    m_reviewTitle->setObjectName(QStringLiteral("nextScheduleImportReviewTitle"));
    m_reviewTitle->setStyleSheet(QStringLiteral("font-size:18px;font-weight:600;"));
    reviewLayout->addWidget(m_reviewTitle);
    m_reviewStatus = new QLabel(reviewPage);
    m_reviewStatus->setObjectName(QStringLiteral("nextScheduleImportReviewStatus"));
    m_reviewStatus->setWordWrap(true);
    reviewLayout->addWidget(m_reviewStatus);

    m_intensiveGroup = new QGroupBox(reviewPage);
    m_intensiveGroup->setObjectName(QStringLiteral("nextScheduleImportIntensiveMode"));
    m_intensiveGroup->setTitle(workflowText("Existing intensive schedule"));
    auto* const intensiveLayout = new QHBoxLayout(m_intensiveGroup);
    m_updateIntensive = new QRadioButton(m_intensiveGroup);
    m_updateIntensive->setObjectName(QStringLiteral("nextScheduleImportUpdateIntensive"));
    m_updateIntensive->setText(workflowText("Update existing; keep absent classes"));
    m_updateIntensive->setChecked(true);
    m_replaceIntensive = new QRadioButton(m_intensiveGroup);
    m_replaceIntensive->setObjectName(QStringLiteral("nextScheduleImportReplaceIntensive"));
    m_replaceIntensive->setText(workflowText("Replace absent classes"));
    intensiveLayout->addWidget(m_updateIntensive);
    intensiveLayout->addWidget(m_replaceIntensive);
    intensiveLayout->addStretch(1);
    reviewLayout->addWidget(m_intensiveGroup);

    m_diagnosticsBox = new QGroupBox(reviewPage);
    m_diagnosticsBox->setObjectName(QStringLiteral("nextScheduleImportDiagnosticsBox"));
    m_diagnosticsBox->setTitle(workflowText("Workbook diagnostics"));
    auto* const diagnosticsLayout = new QVBoxLayout(m_diagnosticsBox);
    m_diagnosticsAcknowledged = new QCheckBox(m_diagnosticsBox);
    m_diagnosticsAcknowledged->setObjectName(
        QStringLiteral("nextScheduleImportDiagnosticsAcknowledged"));
    m_diagnosticsAcknowledged->setText(workflowText(
        "I reviewed the unrecognized cells; they will be ignored."));
    m_diagnostics = new QTextEdit(m_diagnosticsBox);
    m_diagnostics->setObjectName(QStringLiteral("nextScheduleImportDiagnostics"));
    m_diagnostics->setReadOnly(true);
    m_diagnostics->setMaximumHeight(90);
    diagnosticsLayout->addWidget(m_diagnostics);
    diagnosticsLayout->addWidget(m_diagnosticsAcknowledged);
    reviewLayout->addWidget(m_diagnosticsBox);

    auto* const reviewSplitter = new QSplitter(Qt::Horizontal, reviewPage);
    reviewSplitter->setObjectName(QStringLiteral("nextScheduleImportReviewSplitter"));
    auto* const previewPane = new QWidget(reviewSplitter);
    auto* const previewLayout = new QVBoxLayout(previewPane);
    previewLayout->setContentsMargins(0, 0, 0, 0);
    auto* const previewTitle = new QLabel(previewPane);
    previewTitle->setObjectName(QStringLiteral("nextScheduleImportPreviewTitle"));
    previewTitle->setText(workflowText("Schedule preview"));
    previewTitle->setAlignment(Qt::AlignHCenter);
    previewLayout->addWidget(previewTitle);
    m_previewTable = new QTableView(previewPane);
    m_previewTable->setObjectName(QStringLiteral("nextScheduleImportPreviewTable"));
    m_previewTable->setSelectionMode(QAbstractItemView::NoSelection);
    m_previewTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_previewTable->setWordWrap(false);
    m_previewTable->verticalHeader()->setVisible(false);
    m_previewTable->verticalHeader()->setDefaultSectionSize(28);
    m_previewTable->horizontalHeader()->setStretchLastSection(true);
    m_previewModel = new ScheduleImportCandidatePreviewModel(m_previewTable);
    m_previewTable->setModel(m_previewModel);
    m_previewTable->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_previewTable->setFixedHeight(
        m_previewTable->horizontalHeader()->height()
        + MaximumPreviewVisibleRows * m_previewTable->verticalHeader()->defaultSectionSize()
        + 4);
    previewLayout->addWidget(m_previewTable, 0, Qt::AlignTop);
    previewLayout->addStretch(1);

    auto* const resolutionTabs = new QTabWidget(reviewSplitter);
    resolutionTabs->setObjectName(QStringLiteral("nextScheduleImportResolutionTabs"));
    m_classTable = new QTableView(resolutionTabs);
    m_classTable->setObjectName(QStringLiteral("nextScheduleImportClasses"));
    m_classTable->setAlternatingRowColors(true);
    m_classTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_classTable->setEditTriggers(QAbstractItemView::DoubleClicked
                                  | QAbstractItemView::EditKeyPressed);
    m_classTable->verticalHeader()->setVisible(false);
    m_classTable->verticalHeader()->setDefaultSectionSize(34);
    m_classModel = new ScheduleImportClassReviewModel(m_classTable);
    m_classTable->setModel(m_classModel);
    resolutionTabs->addTab(m_classTable, reviewText("Classes"));

    m_teacherTable = new QTableView(resolutionTabs);
    m_teacherTable->setObjectName(QStringLiteral("nextScheduleImportTeachers"));
    m_teacherTable->setAlternatingRowColors(true);
    m_teacherTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_teacherTable->setEditTriggers(QAbstractItemView::DoubleClicked
                                    | QAbstractItemView::EditKeyPressed);
    m_teacherTable->verticalHeader()->setVisible(false);
    m_teacherTable->verticalHeader()->setDefaultSectionSize(34);
    m_teacherModel = new ScheduleImportTeacherReviewModel(m_teacherTable);
    m_teacherTable->setModel(m_teacherModel);
    resolutionTabs->addTab(m_teacherTable, reviewText("Korean teachers"));
    reviewSplitter->addWidget(previewPane);
    reviewSplitter->addWidget(resolutionTabs);
    reviewSplitter->setStretchFactor(0, 0);
    reviewSplitter->setStretchFactor(1, 1);
    reviewLayout->addWidget(reviewSplitter, 1);

    m_reviewSummary = new QLabel(reviewPage);
    m_reviewSummary->setObjectName(QStringLiteral("nextScheduleImportSummary"));
    m_reviewSummary->setWordWrap(true);
    reviewLayout->addWidget(m_reviewSummary);
    m_pages->addWidget(reviewPage);

    auto* const buttons = new QHBoxLayout();
    m_cancelButton = new QPushButton(this);
    m_cancelButton->setObjectName(QStringLiteral("nextScheduleImportCancel"));
    m_backButton = new QPushButton(this);
    m_backButton->setObjectName(QStringLiteral("nextScheduleImportBack"));
    m_primaryButton = new QPushButton(this);
    m_primaryButton->setObjectName(QStringLiteral("nextScheduleImportPrimary"));
    buttons->addWidget(m_cancelButton);
    buttons->addStretch(1);
    buttons->addWidget(m_backButton);
    buttons->addWidget(m_primaryButton);
    root->addLayout(buttons);

    m_choiceDelegate = std::make_unique<ScheduleImportChoiceDelegate>();
    m_teacherTable->setItemDelegateForColumn(
        ScheduleImportTeacherReviewModel::ActionColumn, m_choiceDelegate.get());
    m_teacherTable->setItemDelegateForColumn(
        ScheduleImportTeacherReviewModel::TargetColumn, m_choiceDelegate.get());
    m_teacherTable->setItemDelegateForColumn(
        ScheduleImportTeacherReviewModel::RoomColumn, m_choiceDelegate.get());
    m_classTable->setItemDelegateForColumn(
        ScheduleImportClassReviewModel::ActionColumn, m_choiceDelegate.get());
    m_classTable->setItemDelegateForColumn(
        ScheduleImportClassReviewModel::TargetColumn, m_choiceDelegate.get());

    connect(m_browseButton, &QPushButton::clicked, this,
            [this] { chooseWorkbook(); });
    connect(m_sheet, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this] { populateUsers(); });
    connect(m_primaryButton, &QPushButton::clicked, this, [this] {
        switch (m_pages->currentIndex())
        {
        case 0: beginParse(); break;
        case 1: continueToReview(); break;
        case 2: applyImport(); break;
        }
    });
    connect(m_backButton, &QPushButton::clicked, this, [this] {
        if (m_pages->currentIndex() == 2)
        {
            clearReviewData();
            m_pages->setCurrentIndex(1);
            setFixedSize(SourceAndSelectionDialogSize);
            m_primaryButton->setEnabled(m_user->currentIndex() >= 0);
        }
        else if (m_pages->currentIndex() == 1)
        {
            m_pages->setCurrentIndex(0);
            setFixedSize(SourceAndSelectionDialogSize);
            m_primaryButton->setEnabled(!m_sourcePath->text().trimmed().isEmpty());
        }
    });
    connect(m_cancelButton, &QPushButton::clicked, this,
            &ScheduleImportWorkflowDialog::reject);
    connect(m_diagnosticsAcknowledged, &QCheckBox::toggled, this,
            [this] { updateReviewState(); });
    connect(m_updateIntensive, &QRadioButton::toggled, this,
            [this] { updateReviewState(); });
    connect(m_replaceIntensive, &QRadioButton::toggled, this,
            [this] { updateReviewState(); });
    connect(m_teacherModel, &QAbstractItemModel::dataChanged, this,
            [this] { updateReviewState(); });
    connect(m_classModel, &QAbstractItemModel::dataChanged, this,
            [this] { updateReviewState(); });
    connect(m_kind, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this] {
        if (m_workbook)
        {
            m_workbook.reset();
            m_sheet->clear();
            m_user->clear();
            m_sourceMetadata.reset();
            m_status->setText(workflowText(
                "Schedule type changed. Read the workbook again."));
            m_pages->setCurrentIndex(0);
            setFixedSize(SourceAndSelectionDialogSize);
        }
    });
    m_backButton->setVisible(false);
    m_primaryButton->setEnabled(false);
    m_status->setText(workflowText("Choose a workbook and schedule type."));
}

void ScheduleImportWorkflowDialog::retranslateUi()
{
    if (!m_pages)
    {
        return;
    }
    for (QObject* object : findChildren<QObject*>())
    {
        const QVariant source = object->property("translationSource");
        if (!source.isValid())
        {
            continue;
        }
        const QString translated = workflowText(
            source.toString().toUtf8().constData());
        if (auto* label = qobject_cast<QLabel*>(object))
        {
            label->setText(translated);
        }
        else if (auto* button = qobject_cast<QPushButton*>(object))
        {
            button->setText(translated);
        }
    }
    if (auto* const fileBox = findChild<QGroupBox*>(
            QStringLiteral("nextScheduleImportFileBox")))
    {
        fileBox->setTitle(workflowText("Choose a spreadsheet"));
    }
    if (auto* const typeBox = findChild<QGroupBox*>(
            QStringLiteral("nextScheduleImportTypeBox")))
    {
        typeBox->setTitle(workflowText("Schedule type"));
    }
    if (auto* const selectionForm = findChild<QFormLayout*>(
            QStringLiteral("nextScheduleImportSelectionForm")))
    {
        if (auto* label = qobject_cast<QLabel*>(selectionForm->labelForField(m_sheet)))
        {
            label->setText(workflowText("Worksheet"));
        }
        if (auto* label = qobject_cast<QLabel*>(selectionForm->labelForField(m_user)))
        {
            label->setText(workflowText("Schedule block"));
        }
    }
    if (m_intensiveGroup)
    {
        m_intensiveGroup->setTitle(workflowText("Existing intensive schedule"));
    }
    if (m_updateIntensive)
    {
        m_updateIntensive->setText(workflowText(
            "Update existing; keep absent classes"));
    }
    if (m_replaceIntensive)
    {
        m_replaceIntensive->setText(workflowText("Replace absent classes"));
    }
    if (m_diagnosticsBox)
    {
        m_diagnosticsBox->setTitle(workflowText("Workbook diagnostics"));
    }
    if (m_diagnosticsAcknowledged)
    {
        m_diagnosticsAcknowledged->setText(workflowText(
            "I reviewed the unrecognized cells; they will be ignored."));
    }
    if (auto* const previewTitle = findChild<QLabel*>(
            QStringLiteral("nextScheduleImportPreviewTitle")))
    {
        previewTitle->setText(workflowText("Schedule preview"));
    }
    if (auto* const tabs = findChild<QTabWidget*>(
            QStringLiteral("nextScheduleImportResolutionTabs")))
    {
        if (tabs->count() > 0)
        {
            tabs->setTabText(0, reviewText("Classes"));
        }
        if (tabs->count() > 1)
        {
            tabs->setTabText(1, reviewText("Korean teachers"));
        }
    }
    if (!m_reviewUserName.isEmpty() && m_reviewTitle)
    {
        m_reviewTitle->setText(workflowText("Review schedule for %1")
            .arg(m_reviewUserName));
    }
    if (m_pages->currentIndex() == 0)
    {
        m_primaryButton->setText(workflowText("Read workbook"));
    }
    else if (m_pages->currentIndex() == 1)
    {
        m_primaryButton->setText(workflowText("Review selected schedule"));
    }
    else
    {
        m_primaryButton->setText(workflowText("Apply schedule"));
    }
    m_cancelButton->setText(workflowText("Cancel"));
    m_backButton->setText(workflowText("Back"));
    if (m_kind)
    {
        const QSignalBlocker blocker(m_kind);
        m_kind->setItemText(0, workflowText("Regular"));
        m_kind->setItemText(1, workflowText("Intensive"));
    }
    if (m_classModel)
    {
        m_classModel->retranslateHeaders();
    }
    if (m_teacherModel)
    {
        m_teacherModel->retranslateHeaders();
    }
    if (m_lastApplyFailure)
    {
        showReviewError(applyFailureText(*m_lastApplyFailure));
    }
    else if (m_pages->currentIndex() == 2
             && !m_reviewStatus->property("error").toBool())
    {
        updateReviewState();
    }
}

void ScheduleImportWorkflowDialog::chooseWorkbook()
{
    QString path;
    if (m_actions.chooseWorkbook)
    {
        path = m_actions.chooseWorkbook(this);
    }
    else
    {
        path = QFileDialog::getOpenFileName(this,
            workflowText("Import Schedule"), {},
            workflowText("Excel workbooks (*.xlsx)"));
    }
    if (path.trimmed().isEmpty())
    {
        return;
    }
    m_sourcePath->setText(path);
    m_workbook.reset();
    m_sourceMetadata.reset();
    m_sheet->clear();
    m_user->clear();
    m_primaryButton->setEnabled(true);
    m_status->setText(workflowText("Ready to read the workbook."));
}

void ScheduleImportWorkflowDialog::beginParse()
{
    const QString path = m_sourcePath->text().trimmed();
    if (QFileInfo(path).suffix().compare(QStringLiteral("xlsx"),
                                         Qt::CaseInsensitive) != 0)
    {
        showSourceError(workflowText("Choose an .xlsx workbook."));
        return;
    }
    const auto metadata = fileMetadata(path);
    if (!metadata)
    {
        showSourceError(workflowText("The selected workbook is unavailable."));
        return;
    }
    QString workspaceError;
    if (!workspaceIsCurrent(&workspaceError))
    {
        showSourceError(workspaceError);
        return;
    }

    ++m_loadRequestId;
    const std::uint64_t requestId = m_loadRequestId;
    const ScheduleImportKind kind = static_cast<ScheduleImportKind>(
        m_kind->currentData().toInt());
    auto canceled = std::make_shared<std::atomic_bool>(false);
    m_parseCancellation = canceled;
    m_sourceMetadata = metadata;
    m_sourceFilePath = path;
    m_status->setProperty("error", false);
    m_status->setText(workflowText("Reading the workbook…"));
    m_primaryButton->setEnabled(false);
    m_backButton->setEnabled(false);
    m_browseButton->setEnabled(false);
    m_kind->setEnabled(false);

    using TaskResult = std::shared_ptr<ScheduleImportParseTaskResult>;
    using Watcher = QFutureWatcher<TaskResult>;
    auto* const watcher = new Watcher(this);
    auto* const timeout = new QTimer(watcher);
    timeout->setSingleShot(true);
    m_parseWatcher = watcher;
    connect(watcher, &Watcher::finished, this,
        [this, watcher, timeout, requestId] {
            timeout->stop();
            TaskResult result;
            try
            {
                result = watcher->result();
            }
            catch (const std::exception& error)
            {
                result = std::make_shared<ScheduleImportParseTaskResult>();
                result->error = QString::fromUtf8(error.what());
            }
            catch (...)
            {
                result = std::make_shared<ScheduleImportParseTaskResult>();
                result->error = workflowText("The workbook could not be read.");
            }
            const bool currentRequest = requestId == m_loadRequestId
                && !m_releasing;
            if (m_parseWatcher == watcher)
            {
                m_parseWatcher = nullptr;
            }
            watcher->deleteLater();
            if (!currentRequest)
            {
                return;
            }
            m_parseCancellation.reset();
            m_browseButton->setEnabled(true);
            m_kind->setEnabled(true);
            m_backButton->setEnabled(true);
            handleParseFinished(requestId, std::move(result));
        });
    int timeoutMilliseconds = 30'000;
    if (m_actions.parseTimeoutMilliseconds)
    {
        try
        {
            const int configuredTimeout = m_actions.parseTimeoutMilliseconds();
            if (configuredTimeout > 0)
            {
                timeoutMilliseconds = configuredTimeout;
            }
        }
        catch (...)
        {
            // Keep the standard timeout if the preference adapter fails.
        }
    }
    const int timeoutSeconds = (timeoutMilliseconds + 999) / 1000;
    connect(timeout, &QTimer::timeout, this,
        [this, requestId, canceled, timeoutSeconds] {
            if (requestId != m_loadRequestId || m_releasing)
            {
                return;
            }
            ++m_loadRequestId;
            canceled->store(true, std::memory_order_relaxed);
            m_parseCancellation.reset();
            m_status->setProperty("error", true);
            m_status->setText(workflowText(
                "Loading the workbook timed out after %1 seconds. "
                "Try again or increase the timeout in Preferences.")
                                  .arg(timeoutSeconds));
            m_primaryButton->setEnabled(true);
            m_backButton->setEnabled(true);
            m_browseButton->setEnabled(true);
            m_kind->setEnabled(true);
        });

    const FileMetadata expected = *metadata;
    const auto parser = m_actions.parseWorkbook;
    watcher->setFuture(QtConcurrent::run(
        [path, expected, kind, canceled, parser]() -> TaskResult {
            auto result = std::make_shared<ScheduleImportParseTaskResult>();
            result->path = path;
            result->kind = kind;
            auto currentMetadata =
                ScheduleImportWorkflowDialog::fileMetadata(path);
            if (currentMetadata != std::optional{expected})
            {
                result->error = workflowText(
                    "The workbook changed before it could be read. Choose it again.");
                return result;
            }
            CancelableFileDevice device(path, canceled);
            if (!device.open(QIODevice::ReadOnly))
            {
                result->error = device.errorString();
                return result;
            }
            Result<ScheduleImportWorkbook> parsed = parser
                ? parser(device, kind)
                : parseScheduleImportWorkbook(device, kind);
            currentMetadata = ScheduleImportWorkflowDialog::fileMetadata(path);
            result->canceled = canceled->load(std::memory_order_relaxed);
            if (result->canceled)
            {
                return result;
            }
            result->sourceMetadataStable = currentMetadata == std::optional{expected};
            if (!result->sourceMetadataStable)
            {
                result->error = workflowText(
                    "The workbook changed while it was being read. Choose it again.");
                return result;
            }
            if (!parsed)
            {
                result->error = parsed.error();
                return result;
            }
            result->workbook.emplace(std::move(parsed.value()));
            return result;
        }));
    timeout->start(timeoutMilliseconds);
}

void ScheduleImportWorkflowDialog::handleParseFinished(
    const std::uint64_t requestId,
    std::shared_ptr<ScheduleImportParseTaskResult> result
    )
{
    if (requestId != m_loadRequestId || m_releasing
        || (result && result->canceled))
    {
        return;
    }
    if (!result)
    {
        showSourceError(workflowText("The workbook could not be read."));
        return;
    }
    QString workspaceError;
    if (!workspaceIsCurrent(&workspaceError))
    {
        showSourceError(workspaceError);
        return;
    }
    if (result->path != m_sourceFilePath
        || result->kind != static_cast<ScheduleImportKind>(
            m_kind->currentData().toInt()))
    {
        return;
    }
    if (!result->error.isEmpty())
    {
        showSourceError(result->error);
        m_primaryButton->setEnabled(true);
        return;
    }
    if (!result->workbook)
    {
        showSourceError(workflowText("The workbook contained no schedule data."));
        m_primaryButton->setEnabled(true);
        return;
    }
    QString sourceError;
    if (!sourceIsCurrent(&sourceError))
    {
        showSourceError(sourceError);
        m_primaryButton->setEnabled(true);
        return;
    }

    m_workbook = std::make_unique<ScheduleImportWorkbook>(
        std::move(*result->workbook));
    populateSheets();
    const QString counts = workbookCounts(*m_workbook);
    m_status->setProperty("error", false);
    m_status->setText(counts);
    if (m_sheet->count() == 0)
    {
        showSourceError(workflowText("The workbook has no visible worksheets."));
        m_primaryButton->setEnabled(true);
        return;
    }
    m_pages->setCurrentIndex(1);
    setFixedSize(SourceAndSelectionDialogSize);
    m_backButton->setVisible(true);
    m_primaryButton->setText(workflowText("Review selected schedule"));
    m_primaryButton->setEnabled(m_user->currentIndex() >= 0);
}

void ScheduleImportWorkflowDialog::populateSheets()
{
    const QSignalBlocker blocker(m_sheet);
    m_sheet->clear();
    if (!m_workbook)
    {
        return;
    }
    for (qsizetype index = 0; index < m_workbook->sheets.size(); ++index)
    {
        const ScheduleImportSheet& sheet = m_workbook->sheets.at(index);
        if (sheet.visible)
        {
            m_sheet->addItem(sheet.name, static_cast<int>(index));
        }
    }
    if (m_sheet->count() > 0)
    {
        m_sheet->setCurrentIndex(0);
    }
    populateUsers();
}

void ScheduleImportWorkflowDialog::populateUsers()
{
    const QSignalBlocker blocker(m_user);
    m_user->clear();
    const int sheetIndex = selectedSheetIndex();
    if (!m_workbook || sheetIndex < 0
        || sheetIndex >= m_workbook->sheets.size())
    {
        return;
    }
    const ScheduleImportSheet& sheet = m_workbook->sheets.at(sheetIndex);
    for (qsizetype index = 0; index < sheet.users.size(); ++index)
    {
        const ScheduleImportUserBlock& user = sheet.users.at(index);
        m_user->addItem(user.name, static_cast<int>(index));
    }
    if (m_user->count() == 0)
    {
        m_selectionStatus->setText(workflowText(
            "This worksheet has no recognized schedule blocks."));
    }
    else
    {
        m_selectionStatus->setText(workflowText("Select the schedule block to review."));
    }
}

void ScheduleImportWorkflowDialog::continueToReview()
{
    QString workspaceError;
    QString sourceError;
    if (!workspaceIsCurrent(&workspaceError))
    {
        showSourceError(workspaceError);
        m_pages->setCurrentIndex(1);
        return;
    }
    if (!sourceIsCurrent(&sourceError))
    {
        showSourceError(sourceError);
        m_pages->setCurrentIndex(0);
        return;
    }
    const int sheetIndex = selectedSheetIndex();
    const int userIndex = selectedUserIndex();
    if (!m_workbook || sheetIndex < 0 || userIndex < 0
        || sheetIndex >= m_workbook->sheets.size()
        || userIndex >= m_workbook->sheets.at(sheetIndex).users.size())
    {
        showSourceError(workflowText("Select a schedule block to continue."));
        return;
    }
    if (!m_actions.readImportState)
    {
        showSourceError(workflowText("Schedule import actions are unavailable."));
        return;
    }

    Application::ScheduleImportStateSnapshotOutcome state;
    try
    {
        state = m_actions.readImportState(m_expectedWorkspacePath);
    }
    catch (const std::exception& error)
    {
        showSourceError(QString::fromUtf8(error.what()));
        return;
    }
    catch (...)
    {
        showSourceError(workflowText("The current schedule could not be loaded."));
        return;
    }
    const auto* const snapshot =
        std::get_if<Application::ScheduleImportStateSnapshot>(&state);
    if (!snapshot)
    {
        const auto* const failure =
            std::get_if<Application::ScheduleImportStateSnapshotFailure>(&state);
        showSourceError(failure && !failure->message.empty()
            ? utf8(failure->message)
            : workflowText("The current schedule could not be loaded."));
        return;
    }

    const ScheduleImportKind kind = static_cast<ScheduleImportKind>(
        m_kind->currentData().toInt());
    const ScheduleImportSheet& sheet = m_workbook->sheets.at(sheetIndex);
    const ScheduleImportUserBlock& user = sheet.users.at(userIndex);
    auto mapped = mapScheduleImportReview(user, kind, *snapshot);
    if (!mapped)
    {
        showSourceError(mapped.error());
        return;
    }
    m_mapping = std::make_unique<ScheduleImportReviewMapping>(
        std::move(mapped.value()));
    m_mapping->applyTemplate.diagnostics.clear();
    for (const ScheduleImportDiagnostic& diagnostic : sheet.diagnostics)
    {
        m_mapping->applyTemplate.diagnostics.push_back({
            diagnostic.sheetName.toStdU16String(),
            diagnostic.userName.toStdU16String(),
            diagnostic.cellReference.toStdU16String(),
            diagnostic.value.toStdU16String(),
            diagnostic.message.toStdU16String()
        });
    }
    for (const ScheduleImportDiagnostic& diagnostic : user.diagnostics)
    {
        m_mapping->applyTemplate.diagnostics.push_back({
            diagnostic.sheetName.toStdU16String(),
            diagnostic.userName.toStdU16String(),
            diagnostic.cellReference.toStdU16String(),
            diagnostic.value.toStdU16String(),
            diagnostic.message.toStdU16String()
        });
    }

    m_previewModel->setUser(&m_mapping->user);
    m_teacherModel->setMapping(m_mapping.get());
    m_classModel->setMapping(m_mapping.get());
    m_reviewTitle->setText(workflowText("Review schedule for %1")
        .arg(user.name));
    m_reviewUserName = user.name;
    QStringList diagnosticLines;
    for (const ScheduleImportDiagnostic& diagnostic : sheet.diagnostics)
    {
        diagnosticLines.append(diagnosticText(diagnostic));
    }
    for (const ScheduleImportDiagnostic& diagnostic : user.diagnostics)
    {
        diagnosticLines.append(diagnosticText(diagnostic));
    }
    m_diagnostics->setPlainText(diagnosticLines.join(QLatin1Char('\n')));
    m_diagnosticsBox->setVisible(!diagnosticLines.isEmpty());
    m_diagnosticsAcknowledged->setVisible(!diagnosticLines.isEmpty());
    {
        const QSignalBlocker blocker(m_diagnosticsAcknowledged);
        m_diagnosticsAcknowledged->setChecked(diagnosticLines.isEmpty());
    }
    m_intensiveGroup->setVisible(kind == ScheduleImportKind::Intensive);
    m_reviewStatus->setProperty("error", false);
    m_pages->setCurrentIndex(2);
    setFixedSize(ReviewDialogSize);
    m_backButton->setVisible(true);
    m_primaryButton->setText(workflowText("Apply schedule"));
    m_teacherTable->resizeColumnsToContents();
    m_classTable->resizeColumnsToContents();
    m_previewTable->resizeColumnsToContents();
    updateReviewState();
}

void ScheduleImportWorkflowDialog::updateReviewState()
{
    if (!m_mapping || m_pages->currentIndex() != 2)
    {
        return;
    }
    m_lastApplyFailure.reset();
    m_reviewStatus->setProperty("error", false);
    m_reviewStatus->setStyleSheet(QString{});
    Application::ScheduleImportReviewDecisionRequest decisions =
        m_teacherModel->decisions();
    const Application::ScheduleImportReviewDecisionRequest classDecisions =
        m_classModel->decisions();
    decisions.classes = classDecisions.classes;
    const auto teacherIds = m_teacherModel->teacherIds();
    const auto mode = intensiveMode();
    const auto readiness = evaluateScheduleImportReview(
        *m_mapping, decisions, teacherIds, mode);
    const QString status = decisionStatus(readiness);
    m_reviewStatus->setText(status);
    const bool diagnosticsAcknowledged =
        m_mapping->applyTemplate.diagnostics.empty()
        || m_diagnosticsAcknowledged->isChecked();
    const bool ready = readiness.decisions.accepted()
        && readiness.stateStatus == Application::ScheduleImportStateEvaluationStatus::Passed
        && diagnosticsAcknowledged;
    m_primaryButton->setEnabled(ready);
    if (!diagnosticsAcknowledged)
    {
        m_reviewStatus->setText(workflowText(
            "Acknowledge the workbook diagnostics before applying."));
    }
    updateReviewSummary(decisions);
}

void ScheduleImportWorkflowDialog::updateReviewSummary(
    const Application::ScheduleImportReviewDecisionRequest& decisions
    )
{
    if (!m_mapping)
    {
        m_reviewSummary->clear();
        return;
    }
    const Application::ScheduleImportStateKind kind =
        m_mapping->kind == ScheduleImportKind::Intensive
            ? Application::ScheduleImportStateKind::Intensive
            : Application::ScheduleImportStateKind::Normal;
    const Application::ScheduleImportStateIntensiveMode mode =
        intensiveMode() == Application::ScheduleImportPlanIntensiveMode::ReplaceWithNew
            ? Application::ScheduleImportStateIntensiveMode::ReplaceWithNew
            : Application::ScheduleImportStateIntensiveMode::UpdateExisting;
    const auto schedulesCleared =
        Application::projectScheduleImportSchedulesCleared(
            m_mapping->snapshot, decisions, kind, mode);
    const auto summary = Application::projectScheduleImportReviewSummary(
        decisions,
        Application::ScheduleImportReviewCount{
            static_cast<int>(m_mapping->applyTemplate.diagnostics.size())
        },
        schedulesCleared);
    m_reviewSummary->setText(workflowText(
        "Proposed: %1 teachers created, %2 room updates, %3 teachers skipped; "
        "%4 classes created, %5 updated, %6 skipped; %7 schedules cleared; "
        "%8 diagnostic cells ignored.")
        .arg(summary.teachersCreated.rawValue())
        .arg(summary.teacherRoomsUpdated.rawValue())
        .arg(summary.teachersSkipped.rawValue())
        .arg(summary.classesCreated.rawValue())
        .arg(summary.classesUpdated.rawValue())
        .arg(summary.classesSkipped.rawValue())
        .arg(summary.schedulesCleared.rawValue())
        .arg(summary.ignoredCells.rawValue()));
}

void ScheduleImportWorkflowDialog::applyImport()
{
    if (!m_mapping)
    {
        return;
    }
    m_lastApplyFailure.reset();
    QString workspaceError;
    if (!workspaceIsCurrent(&workspaceError))
    {
        showReviewError(workspaceError);
        m_primaryButton->setEnabled(false);
        return;
    }
    QString sourceError;
    if (!sourceIsCurrent(&sourceError))
    {
        showReviewError(sourceError);
        m_primaryButton->setEnabled(false);
        return;
    }

    Application::ScheduleImportReviewDecisionRequest decisions =
        m_teacherModel->decisions();
    decisions.classes = m_classModel->decisions().classes;
    const auto teacherIds = m_teacherModel->teacherIds();
    const auto mode = intensiveMode();
    const bool acknowledged = m_mapping->applyTemplate.diagnostics.empty()
        || m_diagnosticsAcknowledged->isChecked();
    const auto readiness = evaluateScheduleImportReview(
        *m_mapping, decisions, teacherIds, mode);
    if (!readiness.decisions.accepted()
        || readiness.stateStatus != Application::ScheduleImportStateEvaluationStatus::Passed
        || !acknowledged)
    {
        updateReviewState();
        return;
    }
    const Application::ScheduleImportApplyRequest request =
        makeScheduleImportApplyRequest(
            *m_mapping,
            decisions,
            teacherIds,
            m_mapping->classColors,
            mode,
            acknowledged,
            false,
            false
            );
    if (!m_actions.applyImport)
    {
        showReviewError(workflowText("Schedule import actions are unavailable."));
        return;
    }

    m_primaryButton->setEnabled(false);
    m_reviewStatus->setProperty("error", false);
    m_reviewStatus->setText(workflowText("Applying schedule…"));
    Application::ScheduleImportApplyResult applied;
    try
    {
        applied = m_actions.applyImport(m_expectedWorkspacePath, request);
    }
    catch (const std::exception& error)
    {
        showReviewError(QString::fromUtf8(error.what()));
        m_primaryButton->setEnabled(true);
        return;
    }
    catch (...)
    {
        showReviewError(workflowText("The schedule could not be imported."));
        m_primaryButton->setEnabled(true);
        return;
    }
    if (!applied)
    {
        m_lastApplyFailure = applied.error();
        showReviewError(applyFailureText(*m_lastApplyFailure));
        m_primaryButton->setEnabled(true);
        return;
    }
    const Application::ScheduleImportApplySummary summary = applied.value();
    if (m_actions.importApplied)
    {
        try
        {
            m_actions.importApplied(summary);
        }
        catch (...)
        {
            // A completed transactional import remains successful even if a
            // page refresh callback fails; the page owns its retry surface.
        }
    }
    releaseRetainedData();
    QDialog::accept();
}

void ScheduleImportWorkflowDialog::showSourceError(const QString& message)
{
    m_status->setProperty("error", true);
    m_status->setStyleSheet(QStringLiteral("color:#a32626;font-weight:600;"));
    m_status->setText(message);
}

void ScheduleImportWorkflowDialog::showReviewError(const QString& message)
{
    m_reviewStatus->setProperty("error", true);
    m_reviewStatus->setStyleSheet(QStringLiteral("color:#a32626;font-weight:600;"));
    m_reviewStatus->setText(message);
}

void ScheduleImportWorkflowDialog::clearReviewData()
{
    m_lastApplyFailure.reset();
    m_teacherModel->setMapping(nullptr);
    m_classModel->setMapping(nullptr);
    m_previewModel->setUser(nullptr);
    m_mapping.reset();
    m_diagnostics->clear();
    m_reviewSummary->clear();
    m_reviewStatus->clear();
    m_reviewUserName.clear();
}

void ScheduleImportWorkflowDialog::releaseRetainedData()
{
    if (m_releasing)
    {
        return;
    }
    m_releasing = true;
    ++m_loadRequestId;
    if (m_parseCancellation)
    {
        m_parseCancellation->store(true, std::memory_order_relaxed);
    }
    if (m_parseWatcher)
    {
        QObject::disconnect(m_parseWatcher, nullptr, this, nullptr);
        m_parseWatcher->deleteLater();
        m_parseWatcher = nullptr;
    }
    clearReviewData();
    m_workbook.reset();
    m_sourceMetadata.reset();
    m_sourceFilePath.clear();
    m_parseCancellation.reset();
    m_sourcePath->clear();
    m_sheet->clear();
    m_user->clear();
}

bool ScheduleImportWorkflowDialog::workspaceIsCurrent(QString* error) const
{
    QString path;
    QString identity;
    try
    {
        path = m_actions.activeWorkspacePath
            ? m_actions.activeWorkspacePath().trimmed() : QString{};
        identity = m_actions.workspaceIdentity
            ? m_actions.workspaceIdentity() : path;
    }
    catch (...)
    {
        path.clear();
        identity.clear();
    }
    if (path == m_expectedWorkspacePath
        && identity == m_expectedWorkspaceIdentity
        && !path.isEmpty() && !identity.isEmpty())
    {
        return true;
    }
    if (error)
    {
        *error = workflowText(
            "The active profile changed. Close this import and start again.");
    }
    return false;
}

bool ScheduleImportWorkflowDialog::sourceIsCurrent(QString* error) const
{
    const auto current = fileMetadata(m_sourceFilePath);
    if (current && m_sourceMetadata && current == m_sourceMetadata)
    {
        return true;
    }
    if (error)
    {
        *error = workflowText(
            "The workbook changed after it was read. Read it again before applying.");
    }
    return false;
}

int ScheduleImportWorkflowDialog::selectedSheetIndex() const
{
    return m_sheet && m_sheet->currentIndex() >= 0
        ? m_sheet->currentData().toInt() : -1;
}

int ScheduleImportWorkflowDialog::selectedUserIndex() const
{
    return m_user && m_user->currentIndex() >= 0
        ? m_user->currentData().toInt() : -1;
}

Application::ScheduleImportPlanIntensiveMode
ScheduleImportWorkflowDialog::intensiveMode() const
{
    return m_replaceIntensive && m_replaceIntensive->isChecked()
        ? Application::ScheduleImportPlanIntensiveMode::ReplaceWithNew
        : Application::ScheduleImportPlanIntensiveMode::UpdateExisting;
}

std::optional<ScheduleImportWorkflowDialog::FileMetadata>
ScheduleImportWorkflowDialog::fileMetadata(const QString& path)
{
    QFileInfo info(path);
    info.refresh();
    if (!info.exists() || !info.isFile() || info.size() < 0)
    {
        return std::nullopt;
    }
    const QString canonicalPath = info.canonicalFilePath();
    if (canonicalPath.isEmpty())
    {
        return std::nullopt;
    }
    const QDateTime modified = info.lastModified();
    const QDateTime metadataChanged = info.metadataChangeTime();
    const QDateTime birth = info.birthTime();
    return FileMetadata{
        canonicalPath,
        info.size(),
        modified.isValid() ? modified.toMSecsSinceEpoch() : -1,
        metadataChanged.isValid() ? metadataChanged.toMSecsSinceEpoch() : -1,
        birth.isValid() ? birth.toMSecsSinceEpoch() : -1
    };
}

} // namespace ClassMngr::Next::Ui
