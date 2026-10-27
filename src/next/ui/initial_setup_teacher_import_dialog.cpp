#include "next/ui/initial_setup_teacher_import_dialog.h"

#include "domain/models/teacher_import.h"
#include "features/teacher/import/teacher_import_file_validator.h"
#include "features/teacher/import/teacher_import_template_registry.h"
#include "next/ui/teacher_import_request_mapper.h"
#include "next/application/excel_import_timeout_preferences.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QCoreApplication>
#include <QDialogButtonBox>
#include <QFile>
#include <QFrame>
#include <QFutureWatcher>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QPushButton>
#include <QProgressBar>
#include <QRadioButton>
#include <QScrollArea>
#include <QSizePolicy>
#include <QTimer>
#include <QVBoxLayout>
#include <QtConcurrentRun>

#include <cstddef>
#include <cstdint>
#include <array>
#include <utility>

namespace ClassMngr::Next::Ui
{
namespace
{

QString importText(const char* sourceText)
{
    return QCoreApplication::translate(
        "InitialSetupTeacherImportDialog",
        sourceText
        );
}

QString utf8String(const std::string& value)
{
    return QString::fromUtf8(value.data(), static_cast<qsizetype>(value.size()));
}

}  // namespace

InitialSetupTeacherImportDialog::InitialSetupTeacherImportDialog(
    InitialSetupTeacherImportActions actions,
    QWidget* parent
    )
    : QDialog(parent)
    , m_actions(std::move(actions))
{
    setObjectName(QStringLiteral("classMngrNextTeacherImportDialog"));
    setWindowTitle(m_actions.returnReviewRequestOnly
        ? QCoreApplication::translate("TeacherImportDialog", "Import Teachers")
        : importText("Import Teachers from Workbook"));
    setModal(true);
    if (!m_actions.returnReviewRequestOnly)
    {
        resize(560, 700);
    }

    auto* layout = new QVBoxLayout(this);
    m_mainLayout = layout;
    layout->setContentsMargins(
        m_actions.returnReviewRequestOnly ? 18 : 24,
        m_actions.returnReviewRequestOnly ? 18 : 20,
        m_actions.returnReviewRequestOnly ? 18 : 24,
        m_actions.returnReviewRequestOnly ? 18 : 20);
    layout->setSpacing(m_actions.returnReviewRequestOnly ? 10 : 12);

    auto* fileLabel = new QLabel(importText("File to import from:"), this);
    fileLabel->setObjectName(QStringLiteral("classMngrNextTeacherImportFileLabel"));
    if (m_actions.returnReviewRequestOnly)
    {
        fileLabel->setAlignment(Qt::AlignLeft | Qt::AlignTop);
        fileLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
        fileLabel->setFixedHeight(fileLabel->sizeHint().height());
    }
    m_filePathEdit = new QLineEdit(this);
    m_filePathEdit->setObjectName(
        QStringLiteral("classMngrNextTeacherImportFilePath"));
    m_filePathEdit->setReadOnly(true);
    m_browseButton = new QPushButton(importText("Browse..."), this);
    m_browseButton->setObjectName(
        QStringLiteral("classMngrNextTeacherImportBrowse"));
    auto* fileRow = new QHBoxLayout;
    fileRow->addWidget(m_filePathEdit, 1);
    fileRow->addWidget(m_browseButton);
    layout->addWidget(fileLabel);
    layout->addLayout(fileRow);
    if (m_actions.returnReviewRequestOnly)
    {
        layout->setAlignment(fileRow, Qt::AlignTop);
    }

    m_statusLabel = new QLabel(
        importText("Choose an XLSX teacher workbook to preview."), this);
    m_statusLabel->setObjectName(
        QStringLiteral("classMngrNextTeacherImportStatus"));
    m_statusLabel->setWordWrap(true);
    if (m_actions.returnReviewRequestOnly)
    {
        m_statusLabel->setAlignment(Qt::AlignLeft | Qt::AlignTop);
        m_statusLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
        m_statusLabel->setFixedHeight(m_statusLabel->sizeHint().height());
    }
    layout->addWidget(m_statusLabel);

    auto* validationActions = new QHBoxLayout;
    m_progressBar = new QProgressBar(this);
    m_progressBar->setObjectName(
        QStringLiteral("classMngrNextTeacherImportProgress"));
    m_progressBar->setRange(0, 0);
    m_progressBar->setTextVisible(false);
    m_progressBar->hide();
    m_retryButton = new QPushButton(importText("Retry"), this);
    m_retryButton->setObjectName(
        QStringLiteral("classMngrNextTeacherImportRetry"));
    m_retryButton->hide();
    validationActions->addWidget(m_progressBar, 1);
    validationActions->addWidget(m_retryButton);
    layout->addLayout(validationActions);

    m_previewLabel = new QLabel(this);
    m_previewLabel->setObjectName(
        QStringLiteral("classMngrNextTeacherImportPreview"));
    m_previewLabel->setWordWrap(true);
    m_previewLabel->hide();
    layout->addWidget(m_previewLabel);

    m_automaticLabel = new QLabel(this);
    m_automaticLabel->setObjectName(
        QStringLiteral("classMngrNextTeacherImportAutomaticCounts"));
    m_automaticLabel->setWordWrap(true);
    m_automaticLabel->hide();
    layout->addWidget(m_automaticLabel);

    m_koreanHeadingLabel = new QLabel(
        importText("Korean Teachers to Import:"), this);
    m_koreanHeadingLabel->setObjectName(
        QStringLiteral("classMngrNextTeacherImportKoreanHeading"));
    m_koreanHeadingLabel->hide();
    layout->addWidget(m_koreanHeadingLabel);

    auto* reviewContent = new QWidget(this);
    reviewContent->setObjectName(
        QStringLiteral("classMngrNextTeacherImportReviewContent"));
    m_reviewLayout = new QVBoxLayout(reviewContent);
    m_reviewLayout->setContentsMargins(0, 0, 0, 0);
    m_reviewLayout->setSpacing(8);
    m_reviewWidget = reviewContent;

    m_candidateScrollArea = new QScrollArea(this);
    m_candidateScrollArea->setObjectName(
        QStringLiteral("classMngrNextTeacherImportCandidateScrollArea"));
    m_candidateScrollArea->setWidgetResizable(true);
    m_candidateScrollArea->setFrameShape(QFrame::NoFrame);
    if (m_actions.returnReviewRequestOnly)
    {
        m_candidateScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    }
    m_candidateScrollArea->setWidget(reviewContent);
    m_candidateScrollArea->hide();
    layout->addWidget(m_candidateScrollArea, 1);

    m_buttons = new QDialogButtonBox(this);
    m_buttons->setObjectName(QStringLiteral("classMngrNextTeacherImportButtons"));
    m_applyButton = m_buttons->addButton(
        m_actions.returnReviewRequestOnly
            ? QCoreApplication::translate("TeacherImportDialog", "Import")
            : importText("Apply Import"),
        QDialogButtonBox::AcceptRole);
    m_applyButton->setObjectName(
        QStringLiteral("classMngrNextTeacherImportApply"));
    m_buttons->addButton(
            importText(m_actions.returnReviewRequestOnly ? "Cancel" : "Cancel Setup"),
            QDialogButtonBox::RejectRole)
        ->setObjectName(QStringLiteral("classMngrNextTeacherImportCancel"));
    layout->addWidget(m_buttons);

    connect(m_browseButton, &QPushButton::clicked,
            this, &InitialSetupTeacherImportDialog::browseForWorkbook);
    connect(m_retryButton, &QPushButton::clicked,
            this, &InitialSetupTeacherImportDialog::validateSelectedFile);
    connect(m_buttons, &QDialogButtonBox::accepted,
            this, &InitialSetupTeacherImportDialog::applyImport);
    connect(m_buttons, &QDialogButtonBox::rejected,
            this, &InitialSetupTeacherImportDialog::reject);
    if (m_actions.returnReviewRequestOnly)
    {
        layout->activate();
        const int selectionHeight =
            QLocale().language() == QLocale::Korean ? 212 : 218;
        setFixedSize(460, selectionHeight);
    }
    updateApplyEnabled();
}

void InitialSetupTeacherImportDialog::setFilePath(const QString& filePath)
{
    m_filePathEdit->setText(filePath);
    validateSelectedFile();
}

QString InitialSetupTeacherImportDialog::successSummary() const
{
    return m_successSummary;
}

const std::optional<Application::TeacherImportUseCaseRequest>&
InitialSetupTeacherImportDialog::reviewedRequest() const noexcept
{
    return m_reviewedRequest;
}

void InitialSetupTeacherImportDialog::reject()
{
    if (!m_actions.returnReviewRequestOnly && !m_cancelSetupCalled)
    {
        m_cancelSetupCalled = true;
        if (m_actions.cancelSetup)
        {
            m_actions.cancelSetup();
        }
    }
    QDialog::reject();
}

void InitialSetupTeacherImportDialog::browseForWorkbook()
{
    if (!m_actions.chooseWorkbook)
    {
        setStatus(importText("The workbook selection service is unavailable."));
        return;
    }

    const Domain::Result<std::optional<QString>> chosen =
        m_actions.chooseWorkbook();
    if (!chosen)
    {
        setStatus(importText("Unable to choose a workbook: %1")
                      .arg(utf8String(chosen.error().message)));
        return;
    }
    if (!chosen.value().has_value())
    {
        return;
    }
    setFilePath(*chosen.value());
}

void InitialSetupTeacherImportDialog::validateSelectedFile()
{
    const quint64 requestId = ++m_validationRequestId;
    clearReviewControls();
    m_previewLabel->clear();
    m_previewLabel->hide();
    m_automaticLabel->clear();
    m_automaticLabel->hide();
    m_koreanHeadingLabel->hide();
    m_successSummary.clear();

    const QString filePath = m_filePathEdit->text();
    if (filePath.trimmed().isEmpty())
    {
        setStatus(importText("Choose an XLSX teacher workbook to preview."));
        updateApplyEnabled();
        return;
    }

    m_retryButton->hide();
    setStatus(importText("Reading and validating the workbook..."));
    setLoading(true);

    int timeoutMilliseconds =
        Application::kDefaultExcelImportTimeoutSeconds * 1000;
    if (m_actions.validationTimeoutMilliseconds)
    {
        const int configuredTimeout = m_actions.validationTimeoutMilliseconds();
        if (configuredTimeout >= 0)
        {
            timeoutMilliseconds = configuredTimeout;
        }
    }
    const int timeoutSeconds = (timeoutMilliseconds + 999) / 1000;

    auto* watcher = new QFutureWatcher<TeacherImportFileValidation>(this);
    auto* timeout = new QTimer(watcher);
    timeout->setSingleShot(true);
    connect(watcher,
            &QFutureWatcher<TeacherImportFileValidation>::finished,
            this,
            [this, watcher, timeout, requestId] {
                timeout->stop();
                const TeacherImportFileValidation validation = watcher->result();
                watcher->deleteLater();
                if (requestId != m_validationRequestId)
                {
                    return;
                }

                setLoading(false);
                showValidation(validation);
            });
    connect(timeout, &QTimer::timeout, this,
            [this, requestId, timeoutSeconds] {
                if (requestId != m_validationRequestId)
                {
                    return;
                }

                ++m_validationRequestId;
                setLoading(false);
                setStatus(importText(
                    "Loading the workbook timed out after %1 seconds. "
                    "Try again or increase the timeout in Preferences.")
                              .arg(timeoutSeconds));
                m_retryButton->show();
            });

    const auto validate = m_actions.validateWorkbook;
    watcher->setFuture(QtConcurrent::run([filePath, validate] {
        if (validate)
        {
            return validate(filePath);
        }
        return validateTeacherImportFile(filePath);
    }));
    timeout->start(timeoutMilliseconds);
}

void InitialSetupTeacherImportDialog::showValidation(
    const TeacherImportFileValidation& validation
    )
{
    if (!validation.isValid())
    {
        setStatus(importText("The selected workbook is invalid: %1")
                      .arg(validation.diagnostics.join(QLatin1Char('\n'))));
        updateApplyEnabled();
        return;
    }

    m_previewLabel->setText(
        importText("Template: %1\nSource date: %2")
            .arg(validation.templateName,
                 validation.sourceDate.toString(Qt::ISODate)));
    m_previewLabel->show();
    m_automaticLabel->setText(
        importText("Automatically importing %1 Native English Teachers and %2 GS Team members.")
            .arg(validation.previewCounts.nativeEnglishTeachers)
            .arg(validation.previewCounts.gsTeamMembers));
    m_automaticLabel->show();
    m_koreanHeadingLabel->show();
    buildReviewControls(validation.preview);
    setStatus(importText("Review the workbook, then apply the import."));
    updateApplyEnabled();
    if (m_actions.returnReviewRequestOnly)
    {
        m_mainLayout->setContentsMargins(18, 18, 18, 18);
        m_mainLayout->setSpacing(10);
        m_mainLayout->activate();
        const int height = QLocale().language() == QLocale::Korean ? 477 : 501;
        setFixedSize(460, height);
    }
}

void InitialSetupTeacherImportDialog::clearReviewControls()
{
    m_groupControls.clear();
    delete m_candidateScrollArea->takeWidget();

    auto* reviewContent = new QWidget(m_candidateScrollArea);
    reviewContent->setObjectName(
        QStringLiteral("classMngrNextTeacherImportReviewContent"));
    m_reviewLayout = new QVBoxLayout(reviewContent);
    m_reviewLayout->setContentsMargins(0, 0, 0, 0);
    m_reviewLayout->setSpacing(8);
    m_reviewWidget = reviewContent;
    m_candidateScrollArea->setWidget(reviewContent);
    m_candidateScrollArea->hide();
    m_preview.reset();
}

void InitialSetupTeacherImportDialog::buildReviewControls(
    const TeacherImportPreview& preview
    )
{
    clearReviewControls();
    m_preview = preview;

    auto* gridContainer = new QWidget(m_reviewWidget);
    auto* grid = new QGridLayout(gridContainer);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setHorizontalSpacing(16);
    grid->setColumnStretch(0, 1);
    grid->setColumnMinimumWidth(1, 40);
    grid->setColumnMinimumWidth(2, 55);
    grid->setColumnMinimumWidth(3, 45);

    const std::array<const char*, 4> headers{"Level", "All", "Select", "None"};
    for (int column = 0; column < static_cast<int>(headers.size()); ++column)
    {
        auto* label = new QLabel(importText(headers.at(
            static_cast<std::size_t>(column))), gridContainer);
        label->setObjectName(QStringLiteral("classMngrNextTeacherImportHeader_%1")
                                 .arg(column));
        grid->addWidget(label, 0, column,
                        column == 0 ? Qt::AlignLeft : Qt::AlignCenter);
    }

    auto* checklists = new QVBoxLayout;
    checklists->setContentsMargins(0, 8, 0, 0);
    for (int groupIndex = 0; groupIndex < preview.koreanGroups.size(); ++groupIndex)
    {
        const KoreanTeacherImportGroup& group = preview.koreanGroups.at(groupIndex);
        GroupControls controls;
        controls.level = group.level;
        controls.all = new QRadioButton(gridContainer);
        controls.selected = new QRadioButton(gridContainer);
        controls.none = new QRadioButton(gridContainer);
        controls.all->setObjectName(
            QStringLiteral("classMngrNextTeacherImportAll_%1").arg(groupIndex));
        controls.selected->setObjectName(
            QStringLiteral("classMngrNextTeacherImportSelect_%1").arg(groupIndex));
        controls.none->setObjectName(
            QStringLiteral("classMngrNextTeacherImportNone_%1").arg(groupIndex));
        auto* buttonGroup = new QButtonGroup(gridContainer);
        buttonGroup->addButton(controls.all);
        buttonGroup->addButton(controls.selected);
        buttonGroup->addButton(controls.none);
        controls.all->setChecked(true);

        auto* levelLabel = new QLabel(group.level, gridContainer);
        levelLabel->setObjectName(
            QStringLiteral("classMngrNextTeacherImportLevel_%1").arg(groupIndex));
        grid->addWidget(levelLabel, groupIndex + 1, 0);
        grid->addWidget(controls.all, groupIndex + 1, 1, Qt::AlignCenter);
        grid->addWidget(controls.selected, groupIndex + 1, 2, Qt::AlignCenter);
        grid->addWidget(controls.none, groupIndex + 1, 3, Qt::AlignCenter);

        auto* candidateBox = new QGroupBox(
            importText("Select %1 Teachers").arg(group.level), m_reviewWidget);
        candidateBox->setObjectName(
            QStringLiteral("classMngrNextTeacherImportCandidates_%1").arg(groupIndex));
        auto* candidateLayout = new QGridLayout(candidateBox);
        candidateLayout->setColumnMinimumWidth(1, 14);
        candidateLayout->setColumnMinimumWidth(3, 32);
        candidateLayout->setColumnStretch(4, 1);
        auto* nameHeader = new QLabel(importText("Name"), candidateBox);
        nameHeader->setObjectName(
            QStringLiteral("classMngrNextTeacherImportCandidateNameHeader_%1")
                .arg(groupIndex));
        auto* roomHeader = new QLabel(importText("Room"), candidateBox);
        roomHeader->setObjectName(
            QStringLiteral("classMngrNextTeacherImportCandidateRoomHeader_%1")
                .arg(groupIndex));
        candidateLayout->addWidget(nameHeader, 0, 2);
        candidateLayout->addWidget(roomHeader, 0, 4);

        for (int candidateIndex = 0;
             candidateIndex < group.candidates.size();
             ++candidateIndex)
        {
            const KoreanTeacherImportCandidate& candidate =
                group.candidates.at(candidateIndex);
            auto* check = new QCheckBox(candidateBox);
            check->setObjectName(
                QStringLiteral("classMngrNextTeacherImportCandidate_%1_%2")
                    .arg(groupIndex).arg(candidateIndex));
            check->setChecked(candidate.selectedByDefault);
            controls.candidateChecks.append(check);
            auto* name = new QLabel(candidate.teacher.teacherKr, candidateBox);
            name->setObjectName(
                QStringLiteral("classMngrNextTeacherImportCandidateName_%1_%2")
                    .arg(groupIndex).arg(candidateIndex));
            auto* room = new QLabel(candidate.teacher.roomNumber, candidateBox);
            room->setObjectName(
                QStringLiteral("classMngrNextTeacherImportCandidateRoom_%1_%2")
                    .arg(groupIndex).arg(candidateIndex));
            candidateLayout->addWidget(check, candidateIndex + 1, 0);
            candidateLayout->addWidget(name, candidateIndex + 1, 2);
            candidateLayout->addWidget(room, candidateIndex + 1, 4);
        }

        candidateBox->hide();
        controls.checklist = candidateBox;
        checklists->addWidget(candidateBox);
        connect(controls.selected, &QRadioButton::toggled,
                this, [this, candidateBox](const bool checked) {
                    candidateBox->setVisible(checked);
                    updateApplyEnabled();
                });
        connect(controls.all, &QRadioButton::toggled,
                this, &InitialSetupTeacherImportDialog::updateApplyEnabled);
        connect(controls.none, &QRadioButton::toggled,
                this, &InitialSetupTeacherImportDialog::updateApplyEnabled);
        for (QCheckBox* check : controls.candidateChecks)
        {
            connect(check, &QCheckBox::toggled,
                    this, &InitialSetupTeacherImportDialog::updateApplyEnabled);
        }
        m_groupControls.append(std::move(controls));
    }

    m_reviewLayout->addWidget(gridContainer);
    m_reviewLayout->addLayout(checklists);
    m_reviewLayout->addStretch(1);
    m_candidateScrollArea->show();
}

std::vector<Application::TeacherImportGroupDecision>
InitialSetupTeacherImportDialog::groupDecisions() const
{
    using namespace Application;
    std::vector<TeacherImportGroupDecision> decisions;
    decisions.reserve(static_cast<std::size_t>(m_groupControls.size()));
    for (const GroupControls& controls : m_groupControls)
    {
        TeacherImportGroupMode mode = TeacherImportGroupMode::All;
        if (controls.selected->isChecked())
        {
            mode = TeacherImportGroupMode::Selected;
        }
        else if (controls.none->isChecked())
        {
            mode = TeacherImportGroupMode::None;
        }

        TeacherImportGroupDecision decision{
            TeacherImportGroupKey(controls.level.toUtf8().toStdString()),
            mode,
            {}
        };
        if (mode == TeacherImportGroupMode::Selected)
        {
            for (int index = 0; index < controls.candidateChecks.size(); ++index)
            {
                if (controls.candidateChecks.at(index)->isChecked())
                {
                    decision.selectedCandidateIndexes.push_back(
                        TeacherImportRequestedCandidateIndex{
                            static_cast<std::int64_t>(index)
                        });
                }
            }
        }
        decisions.push_back(std::move(decision));
    }
    return decisions;
}

void InitialSetupTeacherImportDialog::applyImport()
{
    if (!m_preview
        || (!m_actions.apply && !m_actions.returnReviewRequestOnly))
    {
        setStatus(importText("The import workflow is unavailable."));
        updateApplyEnabled();
        return;
    }

    const auto request = teacherImportPreviewToUseCaseRequest(
        *m_preview,
        groupDecisions()
        );
    if (!request)
    {
        setStatus(importText("Review choices are invalid. Check the group selections."));
        updateApplyEnabled();
        return;
    }

    if (m_actions.returnReviewRequestOnly)
    {
        m_reviewedRequest = *request;
        accept();
        return;
    }

    const TeacherImportApplyResult applied = m_actions.apply(*request);
    if (!applied)
    {
        setStatus(importText(
            "Import failed. Your workbook and review choices are still available."));
        return;
    }

    const Application::TeacherImportUseCaseResult& result = *applied;
    m_successSummary = importText(
        "Import complete from %1.\n"
        "Korean teachers: %2 created, %3 updated, %4 unchanged.\n"
        "Native English teachers: %5 created, %6 updated, %7 unchanged.\n"
        "GS Team members: %8 created, %9 updated, %10 unchanged.")
        .arg(m_preview->sourceDate.toString(Qt::ISODate))
        .arg(result.koreanTeachers.created.rawValue())
        .arg(result.koreanTeachers.updated.rawValue())
        .arg(result.koreanTeachers.unchanged.rawValue())
        .arg(result.nativeEnglishTeachers.created.rawValue())
        .arg(result.nativeEnglishTeachers.updated.rawValue())
        .arg(result.nativeEnglishTeachers.unchanged.rawValue())
        .arg(result.gsTeamMembers.created.rawValue())
        .arg(result.gsTeamMembers.updated.rawValue())
        .arg(result.gsTeamMembers.unchanged.rawValue());
    accept();
}

void InitialSetupTeacherImportDialog::updateApplyEnabled()
{
    if (!m_applyButton)
    {
        return;
    }
    if (m_loading || !m_preview
        || (!m_actions.apply && !m_actions.returnReviewRequestOnly))
    {
        m_applyButton->setEnabled(false);
        return;
    }

    const auto request = teacherImportPreviewToUseCaseRequest(
        *m_preview,
        groupDecisions()
        );
    if (!request)
    {
        m_applyButton->setEnabled(false);
        return;
    }
    m_applyButton->setEnabled(
        !request->koreanTeachers.empty()
        || !request->nativeEnglishTeachers.empty()
        || !request->gsTeamMembers.empty());
}

void InitialSetupTeacherImportDialog::setLoading(const bool loading)
{
    m_loading = loading;
    m_progressBar->setVisible(loading);
    m_browseButton->setEnabled(!loading);
    m_retryButton->setEnabled(!loading);
    updateApplyEnabled();
}

void InitialSetupTeacherImportDialog::setStatus(const QString& text)
{
    if (m_actions.returnReviewRequestOnly)
    {
        m_statusLabel->setSizePolicy(
            QSizePolicy::Preferred, QSizePolicy::Preferred);
    }
    m_statusLabel->setText(text);
}

}  // namespace ClassMngr::Next::Ui
