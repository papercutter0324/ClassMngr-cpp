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
    setFixedWidth(460);
    resize(460, 680);

    auto* mainLayout = new QVBoxLayout(this);
    auto* fileLabel = new QLabel(tr("File to import from:"), this);
    fileLabel->setObjectName(QStringLiteral("teacherImportFilePathLabel"));
    fileLabel->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    mainLayout->addWidget(fileLabel, 0, Qt::AlignTop);

    auto* fileLayout = new QHBoxLayout;
    m_fileEdit = new QLineEdit(this);
    m_fileEdit->setObjectName(QStringLiteral("teacherImportFilePath"));
    m_fileEdit->setReadOnly(true);
    m_fileEdit->setPlaceholderText(tr("Select an XLSX teacher list..."));
    fileLabel->setBuddy(m_fileEdit);
    m_browseButton = new QPushButton(tr("Browse..."), this);
    m_browseButton->setObjectName(QStringLiteral("teacherImportBrowseButton"));
    fileLayout->addWidget(m_fileEdit, 1);
    fileLayout->addWidget(m_browseButton);
    mainLayout->addLayout(fileLayout);
    mainLayout->setAlignment(fileLayout, Qt::AlignTop);

    m_statusLabel = new QLabel(tr("Choose a file to import from."), this);
    m_statusLabel->setObjectName(QStringLiteral("teacherImportValidationStatus"));
    m_statusLabel->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    m_statusLabel->setWordWrap(true);
    QFont statusFont = m_statusLabel->font();
    statusFont.setBold(true);
    m_statusLabel->setFont(statusFont);
    m_templateLabel = new QLabel(this);
    m_templateLabel->setObjectName(QStringLiteral("teacherImportTemplateName"));
    m_templateLabel->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    m_templateLabel->setWordWrap(true);
    m_templateLabel->setTextFormat(Qt::PlainText);
    m_templateLabel->setVisible(false);
    m_automaticLabel = new QLabel(this);
    m_automaticLabel->setObjectName(QStringLiteral("teacherImportAutomaticCounts"));
    m_automaticLabel->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    m_automaticLabel->setWordWrap(true);
    m_automaticLabel->setTextFormat(Qt::PlainText);
    m_automaticLabel->setVisible(false);
    m_koreanHeadingLabel = new QLabel(tr("Korean Teachers to Import:"), this);
    m_koreanHeadingLabel->setObjectName(QStringLiteral("teacherImportKoreanHeading"));
    m_koreanHeadingLabel->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    m_koreanHeadingLabel->setVisible(false);
    mainLayout->addWidget(m_statusLabel, 0, Qt::AlignTop);
    mainLayout->addSpacing(8);
    mainLayout->addWidget(m_templateLabel, 0, Qt::AlignTop);
    mainLayout->addSpacing(8);
    mainLayout->addWidget(m_automaticLabel, 0, Qt::AlignTop);
    mainLayout->addSpacing(8);
    mainLayout->addWidget(m_koreanHeadingLabel, 0, Qt::AlignTop);
    mainLayout->addSpacing(8);

    auto* optionsHost = new QWidget(this);
    optionsHost->setObjectName(QStringLiteral("teacherImportLevelOptionsHost"));
    m_optionsHostLayout = new QVBoxLayout(optionsHost);
    m_optionsHostLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->addWidget(optionsHost, 0, Qt::AlignTop);
    mainLayout->addItem(
        new QSpacerItem(0, 0, QSizePolicy::Minimum, QSizePolicy::Expanding));

    m_candidateScrollArea = new QScrollArea(this);
    m_candidateScrollArea->setObjectName(QStringLiteral("teacherImportCandidateScrollArea"));
    m_candidateScrollArea->setWidgetResizable(true);
    m_candidateScrollArea->setFrameShape(QFrame::NoFrame);
    m_candidateScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_candidateScrollArea->setVisible(false);
    auto* checklistsHost = new QWidget(m_candidateScrollArea);
    m_checklistsHostLayout = new QVBoxLayout(checklistsHost);
    m_checklistsHostLayout->setContentsMargins(0, 0, 0, 0);
    m_candidateScrollArea->setWidget(checklistsHost);
    mainLayout->addWidget(m_candidateScrollArea, 1);

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
        m_statusLabel->setText(tr("Status: Invalid File"));
        m_statusLabel->setToolTip(validation.diagnostics.join(QLatin1Char('\n')));
        m_templateLabel->clear();
        m_templateLabel->setVisible(false);
        m_automaticLabel->clear();
        m_automaticLabel->setVisible(false);
        m_koreanHeadingLabel->setVisible(false);
        m_candidateScrollArea->setVisible(false);
        updateImportEnabled();
        return;
    }

    m_valid = true;
    m_preview = validation.preview;
    m_statusLabel->setText(tr("Status: Valid File"));
    m_statusLabel->setToolTip(QString());
    m_templateLabel->setText(
        tr("Template: %1\nVersion: %2")
            .arg(validation.templateName,
                 validation.sourceDate.toString(Qt::ISODate)));
    m_automaticLabel->setText(
        tr("Automatically Importing\n"
           "    GS Team Member(s): %1\n"
           "    Native English Teacher(s): %2")
            .arg(validation.previewCounts.gsTeamMembers)
            .arg(validation.previewCounts.nativeEnglishTeachers));
    m_templateLabel->setVisible(true);
    m_automaticLabel->setVisible(true);
    m_koreanHeadingLabel->setVisible(true);
    m_candidateScrollArea->setVisible(true);
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
    if (m_checklistsWidget)
    {
        m_checklistsHostLayout->removeWidget(m_checklistsWidget);
        delete m_checklistsWidget;
        m_checklistsWidget = nullptr;
    }
    m_preview = {};
}

void TeacherImportDialog::rebuildOptions()
{
    m_optionsWidget = new QWidget;
    auto* layout = new QVBoxLayout(m_optionsWidget);
    layout->setContentsMargins(0, 0, 0, 0);

    auto* grid = new QGridLayout;
    auto* levelHeader = new QLabel(tr("Level"), m_optionsWidget);
    levelHeader->setObjectName(QStringLiteral("teacherImportLevelHeader"));
    levelHeader->setIndent(levelHeader->fontMetrics().horizontalAdvance(QStringLiteral("    ")));
    auto* allHeader = new QLabel(tr("All"), m_optionsWidget);
    allHeader->setObjectName(QStringLiteral("teacherImportAllHeader"));
    auto* selectHeader = new QLabel(tr("Select"), m_optionsWidget);
    selectHeader->setObjectName(QStringLiteral("teacherImportSelectHeader"));
    auto* noneHeader = new QLabel(tr("None"), m_optionsWidget);
    noneHeader->setObjectName(QStringLiteral("teacherImportNoneHeader"));
    grid->addWidget(levelHeader, 0, 0);
    grid->addWidget(allHeader, 0, 1, Qt::AlignCenter);
    grid->addWidget(selectHeader, 0, 2, Qt::AlignCenter);
    grid->addWidget(noneHeader, 0, 3, Qt::AlignCenter);
    grid->setColumnStretch(0, 1);
    grid->setColumnMinimumWidth(1, 38);
    grid->setColumnMinimumWidth(2, 38);
    grid->setColumnMinimumWidth(3, 38);
    grid->setHorizontalSpacing(15);

    m_checklistsWidget = new QWidget;
    auto* checklistLayout = new QVBoxLayout(m_checklistsWidget);
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

        auto* levelLabel = new QLabel(group.level, m_optionsWidget);
        levelLabel->setObjectName(
            QStringLiteral("teacherImportLevel_%1").arg(group.level));
        levelLabel->setIndent(levelLabel->fontMetrics().horizontalAdvance(QStringLiteral("    ")));
        grid->addWidget(levelLabel, row + 1, 0);
        grid->addWidget(controls.all, row + 1, 1, Qt::AlignCenter);
        grid->addWidget(controls.selected, row + 1, 2, Qt::AlignCenter);
        grid->addWidget(controls.none, row + 1, 3, Qt::AlignCenter);

        auto* box = new QGroupBox(tr("Select %1 Teachers").arg(group.level), m_checklistsWidget);
        auto* boxLayout = new QGridLayout(box);
        auto* nameHeader = new QLabel(tr("Name"), box);
        nameHeader->setObjectName(
            QStringLiteral("teacherImportCandidateNameHeader_%1").arg(group.level));
        auto* roomHeader = new QLabel(tr("Room"), box);
        roomHeader->setObjectName(
            QStringLiteral("teacherImportCandidateRoomHeader_%1").arg(group.level));
        boxLayout->setHorizontalSpacing(0);
        boxLayout->setVerticalSpacing(12);
        boxLayout->setColumnMinimumWidth(1, 16);
        boxLayout->setColumnMinimumWidth(3, 32);
        boxLayout->addWidget(nameHeader, 0, 2);
        boxLayout->addWidget(roomHeader, 0, 4);
        boxLayout->setColumnStretch(5, 1);
        for (int candidateIndex = 0; candidateIndex < group.candidates.size(); ++candidateIndex)
        {
            const KoreanTeacherImportCandidate& candidate = group.candidates.at(candidateIndex);
            auto* check = new QCheckBox(box);
            check->setObjectName(
                QStringLiteral("teacherImportCandidate_%1_%2")
                    .arg(row).arg(candidateIndex));
            check->setChecked(candidate.selectedByDefault);
            controls.candidateChecks.append(check);
            auto* name = new QLabel(candidate.teacher.teacherKr, box);
            name->setObjectName(
                QStringLiteral("teacherImportCandidateName_%1_%2")
                    .arg(row).arg(candidateIndex));
            auto* room = new QLabel(candidate.teacher.roomNumber, box);
            room->setObjectName(
                QStringLiteral("teacherImportCandidateRoom_%1_%2")
                    .arg(row).arg(candidateIndex));
            boxLayout->addWidget(check, candidateIndex + 1, 0, Qt::AlignVCenter);
            boxLayout->addWidget(name, candidateIndex + 1, 2, Qt::AlignCenter);
            boxLayout->addWidget(room, candidateIndex + 1, 4, Qt::AlignCenter);
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
    m_optionsHostLayout->insertWidget(0, m_optionsWidget);
    checklistLayout->addStretch();
    m_checklistsHostLayout->addWidget(m_checklistsWidget);
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
