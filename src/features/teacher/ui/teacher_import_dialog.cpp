#include "teacher_import_dialog.h"

#include "features/teacher/import/teacher_import_file_validator.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QScrollArea>
#include <QVBoxLayout>

TeacherImportDialog::TeacherImportDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Import Teachers"));
    setModal(true);
    resize(720, 620);

    auto* mainLayout = new QVBoxLayout(this);
    auto* fileLayout = new QHBoxLayout;
    m_fileEdit = new QLineEdit(this);
    m_fileEdit->setObjectName(QStringLiteral("teacherImportFilePath"));
    m_fileEdit->setReadOnly(true);
    m_fileEdit->setPlaceholderText(tr("Select an XLSX teacher list..."));
    m_browseButton = new QPushButton(tr("Browse..."), this);
    m_browseButton->setObjectName(QStringLiteral("teacherImportBrowseButton"));
    fileLayout->addWidget(m_fileEdit, 1);
    fileLayout->addWidget(m_browseButton);
    mainLayout->addLayout(fileLayout);

    m_statusLabel = new QLabel(tr("Choose a teacher list to validate."), this);
    m_statusLabel->setObjectName(QStringLiteral("teacherImportValidationStatus"));
    m_statusLabel->setWordWrap(true);
    m_templateLabel = new QLabel(this);
    m_templateLabel->setObjectName(QStringLiteral("teacherImportTemplateName"));
    m_templateLabel->setWordWrap(true);
    m_automaticLabel = new QLabel(this);
    m_automaticLabel->setObjectName(QStringLiteral("teacherImportAutomaticCounts"));
    m_automaticLabel->setWordWrap(true);
    mainLayout->addWidget(m_statusLabel);
    mainLayout->addWidget(m_templateLabel);
    mainLayout->addWidget(m_automaticLabel);

    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    auto* optionsHost = new QWidget(scroll);
    m_optionsHostLayout = new QVBoxLayout(optionsHost);
    m_optionsHostLayout->setContentsMargins(0, 0, 0, 0);
    m_optionsHostLayout->addStretch();
    scroll->setWidget(optionsHost);
    mainLayout->addWidget(scroll, 1);

    m_buttons = new QDialogButtonBox(QDialogButtonBox::Cancel, this);
    m_importButton = m_buttons->addButton(tr("Import"), QDialogButtonBox::AcceptRole);
    m_importButton->setObjectName(QStringLiteral("teacherImportAcceptButton"));
    m_importButton->setEnabled(false);
    mainLayout->addWidget(m_buttons);

    connect(m_browseButton, &QPushButton::clicked, this, &TeacherImportDialog::browseForFile);
    connect(m_buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(m_buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

void TeacherImportDialog::setFilePath(const QString& filePath)
{
    m_fileEdit->setText(filePath);
    validateSelectedFile();
}

void TeacherImportDialog::browseForFile()
{
    const QString selected = QFileDialog::getOpenFileName(
        this,
        tr("Select Teacher Import File"),
        QFileInfo(m_fileEdit->text()).absolutePath(),
        tr("Excel Workbooks (*.xlsx)"));
    if (!selected.isEmpty())
    {
        setFilePath(selected);
    }
}

void TeacherImportDialog::validateSelectedFile()
{
    clearOptions();
    const TeacherImportFileValidation validation =
        validateTeacherImportFile(m_fileEdit->text());
    if (!validation.isValid())
    {
        m_valid = false;
        m_statusLabel->setText(
            validation.diagnostics.isEmpty()
                ? tr("The selected file is not a valid teacher import template.")
                : validation.diagnostics.join(QLatin1Char('\n')));
        m_templateLabel->clear();
        m_automaticLabel->clear();
        updateImportEnabled();
        return;
    }

    m_valid = true;
    m_preview = validation.preview;
    m_statusLabel->setText(tr("The selected teacher import file is valid."));
    m_templateLabel->setText(
        tr("Template: %1   Data date: %2")
            .arg(validation.templateName,
                 validation.sourceDate.toString(Qt::ISODate)));
    m_automaticLabel->setText(
        tr("Automatically included: %1 Native English Teacher(s), %2 GS Team member(s).")
            .arg(validation.previewCounts.nativeEnglishTeachers)
            .arg(validation.previewCounts.gsTeamMembers));
    rebuildOptions();
    updateImportEnabled();
}

void TeacherImportDialog::clearOptions()
{
    m_groupControls.clear();
    if (m_optionsWidget)
    {
        m_optionsHostLayout->removeWidget(m_optionsWidget);
        delete m_optionsWidget;
        m_optionsWidget = nullptr;
    }
    m_preview = {};
}

void TeacherImportDialog::rebuildOptions()
{
    m_optionsWidget = new QWidget;
    auto* layout = new QVBoxLayout(m_optionsWidget);
    layout->setContentsMargins(0, 0, 0, 0);

    auto* grid = new QGridLayout;
    grid->addWidget(new QLabel(tr("Level")), 0, 0);
    grid->addWidget(new QLabel(tr("All")), 0, 1, Qt::AlignCenter);
    grid->addWidget(new QLabel(tr("Select")), 0, 2, Qt::AlignCenter);
    grid->addWidget(new QLabel(tr("None")), 0, 3, Qt::AlignCenter);

    auto* checklists = new QWidget;
    auto* checklistLayout = new QVBoxLayout(checklists);
    checklistLayout->setContentsMargins(0, 8, 0, 0);

    for (int row = 0; row < m_preview.koreanGroups.size(); ++row)
    {
        const KoreanTeacherImportGroup& group = m_preview.koreanGroups.at(row);
        GroupControls controls;
        controls.level = group.level;
        controls.all = new QRadioButton;
        controls.selected = new QRadioButton;
        controls.none = new QRadioButton;
        controls.all->setObjectName(QStringLiteral("teacherImportAll_%1").arg(group.level));
        controls.selected->setObjectName(QStringLiteral("teacherImportSelect_%1").arg(group.level));
        controls.none->setObjectName(QStringLiteral("teacherImportNone_%1").arg(group.level));
        auto* buttonGroup = new QButtonGroup(m_optionsWidget);
        buttonGroup->addButton(controls.all);
        buttonGroup->addButton(controls.selected);
        buttonGroup->addButton(controls.none);
        controls.all->setChecked(true);

        grid->addWidget(new QLabel(group.level), row + 1, 0);
        grid->addWidget(controls.all, row + 1, 1, Qt::AlignCenter);
        grid->addWidget(controls.selected, row + 1, 2, Qt::AlignCenter);
        grid->addWidget(controls.none, row + 1, 3, Qt::AlignCenter);

        auto* box = new QGroupBox(tr("Select %1 Teachers").arg(group.level));
        auto* boxLayout = new QVBoxLayout(box);
        for (int candidateIndex = 0; candidateIndex < group.candidates.size(); ++candidateIndex)
        {
            const KoreanTeacherImportCandidate& candidate = group.candidates.at(candidateIndex);
            QString text = candidate.teacher.teacherKr;
            if (!candidate.teacher.roomNumber.isEmpty())
            {
                text += tr(" — Room %1").arg(candidate.teacher.roomNumber);
            }
            auto* check = new QCheckBox(text, box);
            check->setObjectName(
                QStringLiteral("teacherImportCandidate_%1_%2")
                    .arg(row).arg(candidateIndex));
            check->setChecked(candidate.selectedByDefault);
            controls.candidateChecks.append(check);
            boxLayout->addWidget(check);
            connect(check, &QCheckBox::toggled, this, &TeacherImportDialog::updateImportEnabled);
        }
        box->setVisible(false);
        controls.checklistWidget = box;
        checklistLayout->addWidget(box);

        connect(controls.selected, &QRadioButton::toggled, this, [this, box](bool checked) {
            box->setVisible(checked);
            updateImportEnabled();
        });
        connect(controls.all, &QRadioButton::toggled, this, &TeacherImportDialog::updateImportEnabled);
        connect(controls.none, &QRadioButton::toggled, this, &TeacherImportDialog::updateImportEnabled);
        m_groupControls.append(controls);
    }

    layout->addLayout(grid);
    layout->addWidget(checklists);
    layout->addStretch();
    m_optionsHostLayout->insertWidget(0, m_optionsWidget);
}

TeacherImportPlan TeacherImportDialog::importPlan() const
{
    TeacherImportPlan plan;
    plan.templateId = m_preview.templateId;
    plan.sourceDate = m_preview.sourceDate;
    plan.nativeEnglishTeachers = m_preview.nativeEnglishTeachers;
    plan.gsTeamMembers = m_preview.gsTeamMembers;

    for (int groupIndex = 0; groupIndex < m_preview.koreanGroups.size(); ++groupIndex)
    {
        const KoreanTeacherImportGroup& group = m_preview.koreanGroups.at(groupIndex);
        const GroupControls& controls = m_groupControls.at(groupIndex);
        if (controls.none->isChecked())
        {
            continue;
        }
        for (int candidateIndex = 0; candidateIndex < group.candidates.size(); ++candidateIndex)
        {
            if (controls.all->isChecked()
                || (controls.selected->isChecked()
                    && controls.candidateChecks.at(candidateIndex)->isChecked()))
            {
                plan.koreanTeachers.append(group.candidates.at(candidateIndex).teacher);
            }
        }
    }
    return plan;
}

void TeacherImportDialog::updateImportEnabled()
{
    if (!m_valid)
    {
        m_importButton->setEnabled(false);
        return;
    }
    const TeacherImportPlan plan = importPlan();
    m_importButton->setEnabled(
        !plan.koreanTeachers.isEmpty()
        || !plan.nativeEnglishTeachers.isEmpty()
        || !plan.gsTeamMembers.isEmpty());
}
