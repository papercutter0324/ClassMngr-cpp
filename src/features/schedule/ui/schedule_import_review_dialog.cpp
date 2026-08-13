#include "schedule_import_review_dialog.h"
#include "ui/shared/dialogs/user_prompt_service.h"

#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "core/fontmanager.h"
#include "core/utils/colorutils.h"
#include "domain/models/classroom.h"
#include "domain/rules/schedule_import_rules.h"
#include "features/schedule/ui/schedule_import_dialog_shared.h"
#include "features/schedule/ui/schedule_import_review_presentation.h"
#include "features/schedule/ui/schedule_import_resolution_view.h"
#include "features/schedule/services/schedule_import_review_model.h"
#include "features/schedule/services/schedule_import_review_summary.h"
#include "features/schedule/ui/schedule_view_model.h"
#include "features/schedule/ui/schedule_widget.h"
#include "ui/shared/constants/gui_constants.h"
#include "ui/shared/widgets/text_fit_dialog_button_box.h"

#include <QCheckBox>
#include <QColor>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFrame>
#include <QGroupBox>
#include <QHash>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QMap>
#include <QPalette>
#include <QPushButton>
#include <QRadioButton>
#include <QScrollArea>
#include <QScreen>
#include <QSet>
#include <QSizePolicy>
#include <QSplitter>
#include <QTableWidget>
#include <QTabWidget>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>
#include <utility>

namespace
{
using namespace ScheduleImportReviewPresentation;
using namespace ScheduleImportResolutionView;
constexpr int ReviewDialogHeight = 820;
constexpr int MaximumReviewDialogWidth = 1180;
constexpr int MaximumPreviewVisibleRows = 6;
constexpr int InitialPreviewWidth = 540;
constexpr int PreviewHeadingSpacer = 16;
constexpr int PreferredResolutionPaneWidth = 380;
}

ScheduleImportReviewDialog::ScheduleImportReviewDialog(
    ApplicationServices* services,
    ScheduleImportReviewRequest request,
    QWidget* parent
    )
    : DialogShell(QStringLiteral("scheduleImportReview"), parent)
    , m_services(services)
    , m_request(std::move(request))
{
    setWindowTitle(tr("Review & Reconcile"));
    setModal(true);
    buildUi();
}

void ScheduleImportReviewDialog::buildUi()
{
    auto* outerLayout = contentLayout();
    m_reviewPage =
        new QWidget(this);
    m_reviewPage->setSizePolicy(
        QSizePolicy::Ignored,
        QSizePolicy::Ignored
        );
    auto* layout =
        new QVBoxLayout(m_reviewPage);
    auto* heading =
        new QLabel(
            tr("Review & Reconcile"),
            m_reviewPage
            );
    heading->setObjectName(
        QStringLiteral("pageTitle")
        );
    heading->setFont(
        FontManager::getUiFont(
            UiConstants::Pages::TitleFontSize,
            QFont::Bold
            )
        );
    layout->addWidget(heading);

    auto* subtitle =
        new QLabel(
            tr("Review imported classes and resolve any conflicts before continuing."),
            m_reviewPage
            );
    subtitle->setObjectName(
        QStringLiteral("pageSubtitle")
        );
    subtitle->setFont(
        FontManager::getUiFont(
            UiConstants::Pages::SubtitleFontSize
            )
        );
    layout->addWidget(subtitle);

    m_intensiveModeSection =
        new QGroupBox(
            tr("Existing intensive schedule"),
            m_reviewPage
            );
    m_intensiveModeSection->setObjectName(
        QStringLiteral("scheduleImportIntensiveModeSection")
        );
    auto* intensiveModeLayout =
        new QVBoxLayout(m_intensiveModeSection);
    auto* intensiveModePrompt =
        new QLabel(
            tr("How should this import handle the existing intensive schedule?"),
            m_intensiveModeSection
            );
    intensiveModePrompt->setWordWrap(true);
    m_updateIntensiveRadio =
        new QRadioButton(
            tr("Update existing intensive schedule"),
            m_intensiveModeSection
            );
    m_updateIntensiveRadio->setObjectName(
        QStringLiteral("scheduleImportUpdateIntensiveRadio")
        );
    m_replaceIntensiveRadio =
        new QRadioButton(
            tr("Create a brand-new intensive schedule"),
            m_intensiveModeSection
            );
    m_replaceIntensiveRadio->setObjectName(
        QStringLiteral("scheduleImportReplaceIntensiveRadio")
        );
    m_updateIntensiveRadio->setChecked(true);
    intensiveModeLayout->addWidget(intensiveModePrompt);
    intensiveModeLayout->addWidget(m_updateIntensiveRadio);
    intensiveModeLayout->addWidget(m_replaceIntensiveRadio);
    m_intensiveModeSection->setVisible(false);
    layout->addWidget(m_intensiveModeSection);

    m_reviewSplitter =
        new QSplitter(
            Qt::Horizontal,
            m_reviewPage
            );
    m_reviewSplitter->setObjectName(
        QStringLiteral("scheduleImportReviewSplitter")
        );
    m_reviewSplitter->setChildrenCollapsible(false);

    m_previewPane =
        new QWidget(m_reviewSplitter);
    m_previewPane->setObjectName(
        QStringLiteral("scheduleImportPreviewPane")
        );
    auto* previewLayout =
        new QVBoxLayout(m_previewPane);
    previewLayout->setContentsMargins(0, 0, 0, 0);
    previewLayout->setSpacing(0);
    m_previewHeading =
        new QLabel(
            tr("Schedule Preview"),
            m_previewPane
            );
    m_previewHeading->setObjectName(
        QStringLiteral("scheduleImportPreviewHeading")
        );
    m_previewHeading->setAlignment(
        Qt::AlignHCenter | Qt::AlignTop
        );
    previewLayout->addWidget(
        m_previewHeading,
        0,
        Qt::AlignTop
        );
    previewLayout->addSpacing(PreviewHeadingSpacer);
    m_previewWidget =
        new ScheduleWidget(
            m_services,
            m_previewPane,
            ScheduleMode::ReadOnly
            );
    m_previewWidget->setObjectName(
        QStringLiteral("scheduleImportPreview")
        );
    m_previewWidget->setCompactPreview(true);
    m_previewWidget->setMaximumVisibleRows(
        MaximumPreviewVisibleRows
        );
    m_previewWidget->setSizePolicy(
        QSizePolicy::Expanding,
        QSizePolicy::Expanding
        );
    previewLayout->addWidget(
        m_previewWidget,
        1
        );

    m_resolutionTabs =
        new QTabWidget(m_reviewSplitter);
    m_resolutionTabs->setObjectName(
        QStringLiteral("scheduleImportResolutionTabs")
        );

    m_teacherScrollArea =
        new QScrollArea(m_resolutionTabs);
    m_teacherScrollArea->setObjectName(
        QStringLiteral("scheduleImportTeacherScrollArea")
        );
    m_teacherScrollArea->setWidgetResizable(true);
    m_teacherScrollArea->setHorizontalScrollBarPolicy(
        Qt::ScrollBarAlwaysOff
        );
    m_teacherContent =
        new QWidget(m_teacherScrollArea);
    m_teacherContent->setObjectName(
        QStringLiteral("scheduleImportTeacherGroup")
        );
    m_teacherLayout =
        new QVBoxLayout(m_teacherContent);
    m_teacherScrollArea->setWidget(m_teacherContent);
    m_classScrollArea =
        new QScrollArea(m_resolutionTabs);
    m_classScrollArea->setObjectName(
        QStringLiteral("scheduleImportClassScrollArea")
        );
    m_classScrollArea->setWidgetResizable(true);
    m_classScrollArea->setHorizontalScrollBarPolicy(
        Qt::ScrollBarAlwaysOff
        );
    m_classContent =
        new QWidget(m_classScrollArea);
    m_classContent->setObjectName(
        QStringLiteral("scheduleImportClassesGroup")
        );
    m_classLayout =
        new QVBoxLayout(m_classContent);
    m_classScrollArea->setWidget(m_classContent);
    m_resolutionTabs->addTab(
        m_classScrollArea,
        tr("Classes")
        );
    m_resolutionTabs->addTab(
        m_teacherScrollArea,
        tr("Korean Teachers")
        );
    m_resolutionTabs->setCurrentWidget(m_classScrollArea);

    m_reviewSplitter->addWidget(m_previewPane);
    m_reviewSplitter->addWidget(m_resolutionTabs);
    m_reviewSplitter->setStretchFactor(0, 0);
    m_reviewSplitter->setStretchFactor(1, 1);
    layout->addWidget(m_reviewSplitter, 1);

    m_reviewStatus =
        new QLabel(m_reviewPage);
    m_reviewStatus->setObjectName(
        QStringLiteral("scheduleImportReviewStatus")
        );
    m_reviewStatus->setWordWrap(true);
    layout->addWidget(m_reviewStatus);

    m_reviewSummary =
        new QLabel(m_reviewPage);
    m_reviewSummary->setObjectName(
        QStringLiteral("scheduleImportReviewSummary")
        );
    m_reviewSummary->setWordWrap(true);
    layout->addWidget(m_reviewSummary);
    outerLayout->addWidget(m_reviewPage, 1);

    m_buttons = addButtonBox(QDialogButtonBox::Cancel);
    m_backButton =
        m_buttons->addButton(
            tr("Back"),
            QDialogButtonBox::ActionRole
            );
    m_importButton =
        m_buttons->addButton(
            tr("Import"),
            QDialogButtonBox::ActionRole
            );
    m_backButton->setObjectName(
        QStringLiteral("scheduleImportBackButton")
        );
    m_importButton->setObjectName(
        QStringLiteral("scheduleImportAcceptButton")
        );
    m_importButton->setEnabled(false);
    m_importButton->setDefault(true);
    m_importButton->setAutoDefault(true);
    connect(
        m_backButton,
        &QPushButton::clicked,
        this,
        &QDialog::reject
        );
    connect(
        m_importButton,
        &QPushButton::clicked,
        this,
        &ScheduleImportReviewDialog::applyImport
        );
    connect(
        m_updateIntensiveRadio,
        &QRadioButton::toggled,
        this,
        &ScheduleImportReviewDialog::updateReviewState
        );
    connect(
        m_replaceIntensiveRadio,
        &QRadioButton::toggled,
        this,
        &ScheduleImportReviewDialog::updateReviewState
        );
}

bool ScheduleImportReviewDialog::prepare()
{
    if (m_prepared)
    {
        return true;
    }

    ScheduleService* scheduleService =
        openScheduleImportService(m_services);
    if (!scheduleService)
    {
        return false;
    }

    const auto preview =
        scheduleService->previewImport(
            m_request.user,
            m_request.kind
            );
    if (!preview)
    {
        DialogServices::showWarning(
            this,
            tr("Import Schedule"),
            preview.error()
            );
        return false;
    }

    m_preview = *preview;
    m_intensiveModeSection->setVisible(
        m_request.kind == ScheduleImportKind::Intensive
        && m_preview.inventory.hasIntensiveHours
        );
    m_updateIntensiveRadio->setChecked(true);
    m_previewWidget->setPreviewModel(
        previewModel(
            m_preview.user,
            m_request.kind == ScheduleImportKind::Intensive,
            m_previewWidget->displayState()
            )
        );
    rebuildResolutionControls();
    updateReviewState();
    m_prepared = true;
    resizeForReviewStage();
    QTimer::singleShot(
        0,
        this,
        &ScheduleImportReviewDialog::resizeForReviewStage
        );
    return true;
}

void ScheduleImportReviewDialog::rebuildResolutionControls()
{
    clearLayout(m_teacherLayout);
    clearLayout(m_classLayout);

    m_teacherControls.clear();
    m_classControls.clear();
    m_warningLabel = nullptr;
    m_warningAcknowledgement = nullptr;

    if (m_warningScrollArea)
    {
        const int warningTabIndex =
            m_resolutionTabs->indexOf(m_warningScrollArea);
        if (warningTabIndex >= 0)
        {
            m_resolutionTabs->removeTab(warningTabIndex);
        }
        delete m_warningScrollArea;
        m_warningScrollArea = nullptr;
    }

    ClassService* classService =
        m_services
            ? m_services->classService()
            : nullptr;
    TeacherService* teacherService =
        m_services
            ? m_services->teacherService()
            : nullptr;
    if (
        !classService
        || !classService->isAvailable()
        || !teacherService
        || !teacherService->isAvailable()
        )
    {
        return;
    }

    if (!m_preview.user.diagnostics.isEmpty())
    {
        m_warningScrollArea =
            new QScrollArea(m_resolutionTabs);
        m_warningScrollArea->setObjectName(
            QStringLiteral("scheduleImportWarningScrollArea")
            );
        m_warningScrollArea->setWidgetResizable(true);
        m_warningScrollArea->setHorizontalScrollBarPolicy(
            Qt::ScrollBarAlwaysOff
            );
        auto* warnings =
            new QWidget(m_warningScrollArea);
        auto* warningsLayout =
            new QVBoxLayout(warnings);
        QStringList lines;
        for (const ScheduleImportDiagnostic& diagnostic :
             m_preview.user.diagnostics)
        {
            lines.append(
                tr("%1: %2")
                    .arg(
                        diagnostic.cellReference,
                        diagnostic.value.trimmed()
                        )
                );
        }
        m_warningLabel =
            new QLabel(
                lines.join(QLatin1Char('\n')),
                warnings
                );
        m_warningLabel->setWordWrap(true);
        m_warningAcknowledgement =
            new QCheckBox(
                tr("I reviewed these cells and want to skip them."),
                warnings
                );
        m_warningAcknowledgement->setObjectName(
            QStringLiteral("scheduleImportWarningAcknowledgement")
            );
        warningsLayout->addWidget(m_warningLabel);
        warningsLayout->addWidget(
            m_warningAcknowledgement
            );
        warningsLayout->addStretch();
        m_warningScrollArea->setWidget(warnings);
        m_resolutionTabs->insertTab(
            2,
            m_warningScrollArea,
            tr("Unrecognized cells")
            );
        m_resolutionTabs->setCurrentWidget(m_classScrollArea);
        connect(
            m_warningAcknowledgement,
            &QCheckBox::toggled,
            this,
            &ScheduleImportReviewDialog::updateReviewState
            );
    }


    const ScheduleImportResolutionControls::BuildResult controls =
        ScheduleImportResolutionControls::build(
            {
                this,
                m_teacherContent,
                m_classContent,
                m_teacherLayout,
                m_classLayout,
                classService,
                teacherService,
                &m_preview,
                m_request.kind,
                [this]()
                {
                    updateReviewState();
                },
                [this](int candidateIndex)
                {
                    chooseClassColor(candidateIndex);
                }
            }
            );
    m_teacherControls = controls.teacherControls;
    m_classControls = controls.classControls;
}

void ScheduleImportReviewDialog::chooseClassColor(
    int candidateIndex
    )
{
    for (ClassControl& control : m_classControls)
    {
        if (control.candidateIndex != candidateIndex)
        {
            continue;
        }

        const QColor selected =
            ColorUtils::getColor(
                QColor(control.color),
                this,
                tr("Select Imported Class Color"),
                m_services
                    ? m_services->settingsService()
                    : nullptr
                );
        if (!selected.isValid())
        {
            return;
        }

        control.color =
            selected.name(QColor::HexRgb)
                .toUpper();
        ScheduleImportResolutionControls::updateClassColorButton(
            &control
            );
        updateReviewState();
        return;
    }
}

void ScheduleImportReviewDialog::updateScheduleConflictWarning(
    const QStringList& conflicts
    )
{
    QStringList uniqueConflicts = conflicts;
    uniqueConflicts.removeDuplicates();
    const QString signature =
        uniqueConflicts.join(QLatin1Char('\x1f'));
    m_activeScheduleConflictSignature = signature;

    if (signature.isEmpty())
    {
        m_lastWarnedScheduleConflictSignature.clear();
        m_pendingScheduleConflictSignature.clear();
        m_pendingScheduleConflictMessage.clear();
        return;
    }

    if (signature == m_lastWarnedScheduleConflictSignature)
    {
        return;
    }

    m_pendingScheduleConflictSignature = signature;
    m_pendingScheduleConflictMessage =
        tr("Review these schedule conflicts before importing:\n\n%1\n\n"
           "Choose a different existing class, create a new class, or skip an imported class.")
            .arg(uniqueConflicts.join(QLatin1Char('\n')));

    if (m_scheduleConflictWarningQueued)
    {
        return;
    }

    m_scheduleConflictWarningQueued = true;
    QTimer::singleShot(
        0,
        this,
        [this]()
        {
            m_scheduleConflictWarningQueued = false;
            if (
                m_pendingScheduleConflictSignature.isEmpty()
                || m_pendingScheduleConflictSignature
                    != m_activeScheduleConflictSignature
                || m_pendingScheduleConflictSignature
                    == m_lastWarnedScheduleConflictSignature
                )
            {
                return;
            }

            m_lastWarnedScheduleConflictSignature =
                m_pendingScheduleConflictSignature;
            DialogServices::prompts().showMessageAsync(
                PromptRequest{
                    .parent = this,
                    .objectName = QStringLiteral("scheduleImportConflictWarning"),
                    .title = tr("Schedule Import Conflict"),
                    .message = m_pendingScheduleConflictMessage,
                    .severity = PromptSeverity::Warning
                }
                );
        }
        );
}

void ScheduleImportReviewDialog::updateReviewState()
{
    bool valid = true;
    QString message;
    const bool preservesAbsentIntensiveClasses =
        m_request.kind == ScheduleImportKind::Intensive
        && m_updateIntensiveRadio
        && m_updateIntensiveRadio->isChecked();
    QSet<QString> skippedTeachers;
    QHash<QString, int> teacherActions;
    QHash<QString, int> teacherTargets;
    QHash<QString, QString> teacherRooms;

    for (const TeacherControl& control : m_teacherControls)
    {
        const int action =
            control.action->currentData(
                ActionRole
                ).toInt();
        const QString room =
            control.room->currentData().toString();
        teacherActions.insert(control.teacherKey, action);
        teacherTargets.insert(
            control.teacherKey,
            control.action->currentData(TargetRole).toInt()
            );
        teacherRooms.insert(control.teacherKey, room);

        if (action < 0)
        {
            valid = false;
            if (message.isEmpty())
            {
                message =
                    tr("Choose a resolution for every Korean teacher.");
            }
            continue;
        }
        if (
            (
                action == static_cast<int>(
                    ScheduleImportTeacherAction::Create
                    )
                || action == static_cast<int>(
                    ScheduleImportTeacherAction::UpdateRoom
                    )
                || (
                    action == static_cast<int>(
                        ScheduleImportTeacherAction::Reuse
                        )
                    && control.room->findData(QString()) >= 0
                    )
                )
            && room.isEmpty()
            )
        {
            valid = false;
            if (message.isEmpty())
            {
                message =
                    tr("Choose one imported room for this Korean teacher resolution.");
            }
            continue;
        }
        if (
            action == static_cast<int>(
                ScheduleImportTeacherAction::Skip
                )
            )
        {
            skippedTeachers.insert(
                control.teacherKey
                );
        }
        else if (
            action == static_cast<int>(
                ScheduleImportTeacherAction::Create
                )
            )
        {
        }
        else if (
            action == static_cast<int>(
                ScheduleImportTeacherAction::UpdateRoom
                )
            )
        {
        }
    }

    ClassService* classService =
        m_services
            ? m_services->classService()
            : nullptr;
    if (classService && !classService->isAvailable())
    {
        classService = nullptr;
    }
    TeacherService* teacherService =
        m_services
            ? m_services->teacherService()
            : nullptr;
    if (teacherService && !teacherService->isAvailable())
    {
        teacherService = nullptr;
    }
    QSet<int> targets;
    QMap<int, QList<int>> candidateIndexesByTarget;
    QHash<int, int> classActions;
    QHash<int, int> classTargets;
    QStringList scheduleConflicts;

    for (const ClassControl& control : m_classControls)
    {
        if (skippedTeachers.contains(control.teacherKey))
        {
            const int skipIndex =
                findActionIndex(
                    control.action,
                    static_cast<int>(
                        ScheduleImportClassAction::Skip
                        )
                    );
            if (skipIndex >= 0)
            {
                control.action->setCurrentIndex(skipIndex);
            }
        }

        const int action =
            control.action->currentData(
                ActionRole
                ).toInt();
        const int target =
            control.action->currentData(
                TargetRole
                ).toInt();
        classActions.insert(control.candidateIndex, action);
        classTargets.insert(control.candidateIndex, target);

        if (action < 0)
        {
            valid = false;
            if (message.isEmpty())
            {
                message =
                    tr("Choose an action for every class.");
            }
            if (control.details)
            {
                control.details->setText(
                    tr("Choose how this imported row should be reconciled.")
                    );
            }
            continue;
        }

        if (
            action == static_cast<int>(
                ScheduleImportClassAction::UpdateExisting
                )
            )
        {
            if (target <= 0)
            {
                valid = false;
                message =
                    tr("Choose an existing class to update.");
            }
            if (control.details)
            {
                control.details->setText(
                    classDifferences(
                        classService,
                        teacherService,
                        m_preview.user.classes[
                            control.candidateIndex
                        ],
                        target,
                        m_request.kind,
                        control.color,
                        control.details->palette().color(
                            QPalette::Link
                            ),
                        control.details->palette().color(
                            QPalette::Text
                            )
                        )
                    );
            }
        }
        else if (
            action == static_cast<int>(
                ScheduleImportClassAction::CreateNew
                )
            )
        {
            if (control.details)
            {
                control.details->setText(
                    classDifferences(
                        classService,
                        teacherService,
                        m_preview.user.classes[
                            control.candidateIndex
                        ],
                        -1,
                        m_request.kind,
                        control.color,
                        control.details->palette().color(
                            QPalette::Link
                            ),
                        control.details->palette().color(
                            QPalette::Text
                            )
                        )
                    );
            }
        }
        else
        {
            if (control.details)
            {
                control.details->setText(
                    target > 0
                        ? tr("The imported row will be skipped and its unique existing match will keep its current schedule.")
                        : tr("The imported row will be skipped.")
                );
            }
        }

        if (control.colorButton)
        {
            control.colorButton->setEnabled(
                action != static_cast<int>(
                    ScheduleImportClassAction::Skip
                    )
                );
        }

        if (
            action != static_cast<int>(
                ScheduleImportClassAction::Skip
                )
            )
        {
            const ScheduleImportClassCandidate& candidate =
                m_preview.user.classes[
                    control.candidateIndex
                    ];
            if (!candidate.meetingPatternError.isEmpty())
            {
                valid = false;
                if (message.isEmpty())
                {
                    message =
                        tr("%1 %2 has an invalid meeting pattern. Update the spreadsheet or skip this class.")
                            .arg(
                                candidate.classGrade,
                                candidate.classLevel
                                );
                }
                if (control.details)
                {
                    control.details->setText(
                        control.details->text()
                        + QStringLiteral(
                            "<br><span style=\"color:#b91c1c\"><b>%1</b> %2</span>"
                            )
                            .arg(
                                tr("Meeting pattern:").toHtmlEscaped(),
                                candidate.meetingPatternError
                                    .toHtmlEscaped()
                                )
                        );
                }
            }
            if (
                candidate.importedColors.size() > 1
                && control.details
                )
            {
                control.details->setText(
                    control.details->text()
                    + QStringLiteral(
                        "<br><span style=\"color:%1\">%2</span>"
                        )
                        .arg(
                            control.details->palette()
                                .color(QPalette::Link)
                                .name(QColor::HexRgb),
                            tr("The spreadsheet uses multiple colors for this class; confirm the selected color.")
                                .toHtmlEscaped()
                            )
                    );
            }
        }

        if (target > 0)
        {
            targets.insert(target);
            candidateIndexesByTarget[target].append(
                control.candidateIndex
                );
        }
    }

    for (
        auto iterator = candidateIndexesByTarget.cbegin();
        iterator != candidateIndexesByTarget.cend();
        ++iterator
        )
    {
        if (iterator.value().size() < 2)
        {
            continue;
        }

        valid = false;
        if (message.isEmpty())
        {
            message =
                tr("Multiple imported classes cannot use the same existing class.");
        }

        QStringList importedClasses;
        for (int candidateIndex : iterator.value())
        {
            if (
                candidateIndex < 0
                || candidateIndex >= m_preview.user.classes.size()
                )
            {
                continue;
            }
            importedClasses.append(
                importedClassConflictLabel(
                    m_preview.user.classes[candidateIndex]
                    )
                );
        }
        const QString targetLabel =
            classService && teacherService
                ? classLabel(
                    classService,
                    teacherService,
                    iterator.key(),
                    m_request.kind
                    )
                : tr("Class %1").arg(iterator.key());
        scheduleConflicts.append(
            tr("Multiple imported classes are assigned to %1: %2.")
                .arg(
                    targetLabel,
                    importedClasses.join(QStringLiteral(", "))
                    )
            );
    }

    ScheduleImportUserBlock projected;
    projected.name = m_preview.user.name;
    projected.intensiveSlotStates =
        m_preview.user.intensiveSlotStates;

    if (classService && teacherService)
    {
        for (const ClassControl& control : m_classControls)
        {
            const int action =
                classActions.value(
                    control.candidateIndex,
                    -1
                    );
            const int target =
                classTargets.value(
                    control.candidateIndex,
                    -1
                    );

            if (
                action == static_cast<int>(
                    ScheduleImportClassAction::Skip
                    )
                )
            {
                if (target <= 0)
                {
                    continue;
                }

                const ClassInfo info =
                    classService->classInfo(target);
                ScheduleImportClassCandidate preserved;
                preserved.teacherKr =
                    teacherService->teacher(
                        info.teacherId
                        ).teacherKr;
                preserved.rooms = {
                    teacherService->teacher(
                        info.teacherId
                        ).roomNumber
                };
                preserved.classGrade = info.classGrade;
                preserved.classLevel = info.classLevel;
                preserved.importedColors = {
                    info.classColor.isEmpty()
                        ? QStringLiteral("#FFFFFF")
                        : info.classColor
                };
                preserved.times =
                    m_request.kind
                            == ScheduleImportKind::Intensive
                        ? info.intensiveTimes
                        : info.classTimes;
                projected.classes.append(preserved);
                continue;
            }

            if (action < 0)
            {
                continue;
            }

            ScheduleImportClassCandidate candidate =
                m_preview.user.classes[
                    control.candidateIndex
                    ];
            candidate.importedColors = {
                control.color
            };
            const int teacherAction =
                teacherActions.value(
                    candidate.teacherKey,
                    -1
                    );
            QString room =
                teacherRooms.value(candidate.teacherKey);
            if (
                teacherAction == static_cast<int>(
                    ScheduleImportTeacherAction::Reuse
                    )
                )
            {
                room =
                    teacherService->teacher(
                        teacherTargets.value(
                            candidate.teacherKey,
                            -1
                            )
                        ).roomNumber;
            }
            candidate.rooms =
                room.trimmed().isEmpty()
                    ? QStringList{}
                    : QStringList{room.trimmed()};
            projected.classes.append(candidate);
        }

        if (preservesAbsentIntensiveClasses)
        {
            for (const Classroom& classroom : classService->classes())
            {
                if (targets.contains(classroom.id))
                {
                    continue;
                }

                const ClassInfo info =
                    classService->classInfo(classroom.id);
                if (info.intensiveTimes.isEmpty())
                {
                    continue;
                }

                ScheduleImportClassCandidate preserved;
                const Teacher teacher =
                    teacherService->teacher(info.teacherId);
                preserved.teacherKr = teacher.teacherKr;
                preserved.rooms = {teacher.roomNumber};
                preserved.classGrade = info.classGrade;
                preserved.classLevel = info.classLevel;
                preserved.importedColors = {
                    info.classColor.isEmpty()
                        ? QStringLiteral("#FFFFFF")
                        : info.classColor
                };
                preserved.times = info.intensiveTimes;
                projected.classes.append(preserved);
            }
        }
    }
    else
    {
        projected = m_preview.user;
    }

    m_previewWidget->setPreviewModel(
        previewModel(
            projected,
            m_request.kind == ScheduleImportKind::Intensive,
            m_previewWidget->displayState()
            )
        );
    updatePreviewVisibleRows();
    const QStringList projectedConflicts =
        projectedScheduleConflicts(projected);
    if (!projectedConflicts.isEmpty())
    {
        valid = false;
        if (message.isEmpty())
        {
            message =
                tr("The proposed schedule has a conflict: %1")
                    .arg(projectedConflicts.first());
        }
        for (const QString& conflict : projectedConflicts)
        {
            scheduleConflicts.append(
                tr("The proposed schedule has a conflict: %1")
                    .arg(conflict)
                );
        }
    }

    updateScheduleConflictWarning(scheduleConflicts);

    if (
        m_warningAcknowledgement
        && !m_warningAcknowledgement->isChecked()
        )
    {
        valid = false;
        if (message.isEmpty())
        {
            message =
                tr("Acknowledge the unrecognized cells before importing.");
            }
    }

    int cleared = 0;
    if (classService && !preservesAbsentIntensiveClasses)
    {
        for (const Classroom& classroom : classService->classes())
        {
            const ClassInfo info =
                classService->classInfo(classroom.id);
            const bool hasSelectedTimes =
                m_request.kind == ScheduleImportKind::Intensive
                    ? !info.intensiveTimes.isEmpty()
                    : !info.classTimes.isEmpty();
            if (
                hasSelectedTimes
                && !targets.contains(classroom.id)
                )
            {
                ++cleared;
            }
        }
    }

    m_reviewStatus->setText(
        valid
            ? tr("All required resolutions are complete.")
            : message
        );
    QString profileNameChange;
    if (m_request.updateProfileName)
    {
        profileNameChange =
            tr(" My Information name will be updated to “%1”.")
                .arg(m_preview.user.name);
    }
    else if (m_request.profileName.trimmed().isEmpty())
    {
        profileNameChange =
            tr(" My Information name will be set to “%1”.")
                .arg(m_preview.user.name);
    }

    const ScheduleImportReviewSummary summary =
        ScheduleImportReviewSummaryBuilder::build(
            importPlan(),
            cleared
            );
    m_reviewSummary->setText(
        tr("Proposed import: %1 teacher(s) created, %2 room update(s), %3 teacher group(s) skipped; "
           "%4 class(es) created, %5 updated, %6 skipped; %7 existing schedule(s) cleared; "
           "%8 occupied cell(s) acknowledged and ignored.%9%10")
            .arg(summary.teachersCreated)
            .arg(summary.teacherRoomsUpdated)
            .arg(summary.teachersSkipped)
            .arg(summary.classesCreated)
            .arg(summary.classesUpdated)
            .arg(summary.classesSkipped)
            .arg(summary.schedulesCleared)
            .arg(summary.ignoredCells)
            .arg(profileNameChange)
            .arg(
                m_request.kind != ScheduleImportKind::Intensive
                    ? QString()
                    : preservesAbsentIntensiveClasses
                        ? tr(" Existing intensive hours for classes absent from the workbook will be retained.")
                        : tr(" A brand-new intensive schedule will replace existing intensive hours not explicitly preserved.")
                )
        );
    m_importButton->setEnabled(valid);
}

void ScheduleImportReviewDialog::resizeForReviewStage()
{
    if (
        !m_reviewSplitter
        || !m_resolutionTabs
        || !m_previewWidget
        || !m_buttons
        || !layout()
        )
    {
        return;
    }

    m_teacherLayout->activate();
    m_classLayout->activate();
    if (m_reviewPage)
    {
        if (m_reviewPage->layout())
        {
            m_reviewPage->layout()->activate();
        }
    }
    layout()->activate();

    const int resolutionWidth =
        PreferredResolutionPaneWidth;

    const int previewWidth =
        InitialPreviewWidth;

    int reviewContentWidth =
        previewWidth
        + resolutionWidth
        + m_reviewSplitter->handleWidth();
    if (m_reviewPage)
    {
        if (m_reviewPage->layout())
        {
            const QMargins pageMargins =
                m_reviewPage->layout()->contentsMargins();
            reviewContentWidth +=
                pageMargins.left()
                + pageMargins.right();
        }
    }

    const QMargins outerMargins =
        layout()->contentsMargins();
    int targetWidth =
        std::max(
            reviewContentWidth,
            m_buttons->sizeHint().width()
            )
        + outerMargins.left()
        + outerMargins.right();
    targetWidth =
        std::min(
            targetWidth,
            MaximumReviewDialogWidth
            );
    int targetHeight =
        ReviewDialogHeight;

    if (QScreen* targetScreen = screen())
    {
        const QSize available =
            targetScreen->availableGeometry().size();
        targetWidth =
            std::min(targetWidth, available.width());
        targetHeight =
            std::min(targetHeight, available.height());
    }
    resize(
        std::max(1, targetWidth),
        std::max(1, targetHeight)
        );
    m_reviewSplitter->setSizes(
        {previewWidth, resolutionWidth}
        );
    updatePreviewVisibleRows();
}

void ScheduleImportReviewDialog::resizeEvent(
    QResizeEvent* event
    )
{
    QDialog::resizeEvent(event);

    if (!m_prepared)
    {
        return;
    }

    if (m_previewPane && m_previewPane->layout())
    {
        m_previewPane->layout()->activate();
    }
    updatePreviewVisibleRows();
}

void ScheduleImportReviewDialog::updatePreviewVisibleRows()
{
    if (
        !m_previewPane
        || !m_previewHeading
        || !m_previewWidget
        )
    {
        return;
    }

    auto* table =
        m_previewWidget->findChild<QTableWidget*>(
            QStringLiteral("scheduleTable")
            );
    if (!table)
    {
        return;
    }

    int maximumVisibleRows =
        MaximumPreviewVisibleRows;
    if (height() > ReviewDialogHeight)
    {
        const int availableTableHeight =
            m_previewPane->contentsRect().height()
            - m_previewHeading->height()
            - PreviewHeadingSpacer;
        int usedHeight =
            table->horizontalHeader()->height()
            + (2 * table->frameWidth());
        int visibleRows = 0;

        for (int row = 0; row < table->rowCount(); ++row)
        {
            const int rowHeight = table->rowHeight(row);
            if (usedHeight + rowHeight > availableTableHeight)
            {
                break;
            }

            usedHeight += rowHeight;
            ++visibleRows;
        }

        maximumVisibleRows =
            std::max(
                MaximumPreviewVisibleRows,
                visibleRows
                );
    }

    m_previewWidget->setMaximumVisibleRows(
        maximumVisibleRows
        );
}

void ScheduleImportReviewDialog::applyImport()
{
    updateReviewState();
    if (!m_importButton->isEnabled())
    {
        return;
    }

    const ScheduleImportPlan plan =
        importPlan();
    const QString confirmation =
        m_reviewSummary->text()
        + QStringLiteral("\n\n")
        + tr("Is this schedule valid and ready to import?");

    if (
        DialogServices::confirm(
            this,
            tr("Confirm Schedule Import"),
            confirmation,
            tr("Import"),
            tr("Cancel")
            ) != PromptChoice::Accepted
        )
    {
        return;
    }

    ScheduleService* scheduleService =
        openScheduleImportService(m_services);
    const auto summary =
        scheduleService
            ? scheduleService->importSchedule(plan)
            : Result<ScheduleImportSummary>(
                std::unexpected(
                    tr("No Teacher Profile is open.")
                    )
                );

    if (!summary)
    {
        DialogServices::showWarning(
            this,
            tr("Import Schedule"),
            summary.error()
            );
        return;
    }

    DialogServices::showInformation(
        this,
        tr("Import Schedule"),
        tr("Schedule imported successfully.\n"
           "Korean teachers created: %1\n"
           "Korean teacher rooms updated: %2\n"
           "Classes created: %3\n"
           "Classes updated: %4\n"
           "Classes skipped: %5\n"
           "Schedules cleared: %6\n"
           "Ignored occupied cells: %7%8")
            .arg(summary->teachersCreated)
            .arg(summary->teachersUpdated)
            .arg(summary->classesCreated)
            .arg(summary->classesUpdated)
            .arg(summary->classesSkipped)
            .arg(summary->schedulesCleared)
            .arg(summary->ignoredCells)
            .arg(
                summary->profileNameUpdated
                    ? tr("\nMy Information name was updated.")
                    : QString()
                )
        );
    accept();
}

ScheduleImportPlan ScheduleImportReviewDialog::importPlan() const
{
    ScheduleImportReviewContext context;
    context.kind = m_request.kind;
    context.intensiveMode =
        m_replaceIntensiveRadio
        && m_replaceIntensiveRadio->isChecked()
            ? ScheduleImportIntensiveMode::ReplaceWithNew
            : ScheduleImportIntensiveMode::UpdateExisting;
    context.selectedUserName =
        m_preview.user.name;
    context.saveProfileNameIfBlank =
        m_request.profileName.trimmed().isEmpty();
    context.updateProfileName =
        m_request.updateProfileName;
    context.unknownCellsAcknowledged =
        !m_warningAcknowledgement
        || m_warningAcknowledgement->isChecked();
    context.candidates =
        m_preview.user.classes;
    context.intensiveSlotStates =
        m_preview.user.intensiveSlotStates;
    context.diagnostics =
        m_preview.user.diagnostics;

    QList<ScheduleImportTeacherResolution> teachers;
    for (const TeacherControl& control : m_teacherControls)
    {
        ScheduleImportTeacherResolution resolution;
        resolution.teacherKey =
            control.teacherKey;
        resolution.action =
            static_cast<ScheduleImportTeacherAction>(
                control.action->currentData(
                    ActionRole
                    ).toInt()
                );
        resolution.targetTeacherId =
            control.action->currentData(
                TargetRole
                ).toInt();
        resolution.selectedRoom =
            control.room->currentData().toString();
        teachers.append(resolution);
    }

    QList<ScheduleImportClassResolution> classes;
    for (const ClassControl& control : m_classControls)
    {
        ScheduleImportClassResolution resolution;
        resolution.candidateIndex =
            control.candidateIndex;
        resolution.action =
            static_cast<ScheduleImportClassAction>(
                control.action->currentData(
                    ActionRole
                    ).toInt()
                );
        resolution.targetClassId =
            control.action->currentData(
                TargetRole
                ).toInt();
        resolution.classColor =
            control.color;
        resolution.fontColor =
            ColorUtils::getContrastingFontColor(
                QColor(control.color)
                );
        classes.append(resolution);
    }

    return ScheduleImportReviewModel::buildPlan(
        context,
        teachers,
        classes
        );
}
