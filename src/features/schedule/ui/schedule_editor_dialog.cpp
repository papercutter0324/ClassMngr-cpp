#include "schedule_editor_dialog.h"

#include "features/classes/config/class_info_config.h"
#include "core/application_services.h"
#include "core/fontmanager.h"
#include "data/data_service.h"
#include "ui/shared/widgets/clickable_color_preview.h"
#include "core/utils/colorutils.h"

#include <QComboBox>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSignalBlocker>
#include <QVBoxLayout>

ScheduleEditorDialog::ScheduleEditorDialog(
    ApplicationServices* services,
    int classId,
    QWidget* parent
    )
    : QDialog(parent)
    , m_services(services)
    , m_classId(classId)
{
    setWindowTitle(
        tr("Edit Schedule Cell")
        );

    setModal(true);
    resize(420, 320);

    buildUi();
    loadData();
}

void ScheduleEditorDialog::updateLevelOptions()
{
    if (m_loadingData)
    {
        return;
    }

    rebuildLevelOptions(QString());
}

void ScheduleEditorDialog::rebuildLevelOptions(
    const QString& preferredLevel
    )
{
    if (!m_levelCombo || !m_gradeCombo)
    {
        return;
    }

    const QSignalBlocker blocker(m_levelCombo);

    m_levelCombo->clear();
    m_levelCombo->addItem(QString());
    m_levelCombo->addItems(
        ClassInfoConfig::levelsForGrade(
            m_gradeCombo->currentText()
            )
        );

    if (!preferredLevel.isEmpty())
    {
        setComboText(
            m_levelCombo,
            preferredLevel
            );
    }
}

void ScheduleEditorDialog::chooseClassColor()
{
    QColor color =
        ColorUtils::getColor(
            QColor(m_classColor),
            this,
            tr("Choose Class Color"),
            m_services->dataService()
            );

    if (!color.isValid())
    {
        return;
    }

    m_classColor = color.name();
    updateColorPreviews();
}

void ScheduleEditorDialog::chooseFontColor()
{
    QColor color =
        ColorUtils::getColor(
            QColor(m_fontColor),
            this,
            tr("Choose Font Color"),
            m_services->dataService()
            );

    if (!color.isValid())
    {
        return;
    }

    m_fontColor = color.name();
    updateColorPreviews();
}

void ScheduleEditorDialog::saveChanges()
{
    if (!m_services || !m_services->dataService())
    {
        return;
    }

    ClassInfo info =
        m_cachedInfo;

    const QString newGrade =
        m_gradeCombo->currentText();

    const QString newLevel =
        m_levelCombo->currentText();

    const bool clearBooks =
        newGrade != m_originalGrade
        || newLevel != m_originalLevel;

    info.classGrade = newGrade;
    info.classLevel = newLevel;
    info.classColor = m_classColor;
    info.fontColor = m_fontColor;

    if (clearBooks)
    {
        info.readingBook.clear();
        info.essayBook.clear();
    }

    if (!m_services->dataService()->saveClassInfo(info))
    {
        QMessageBox::warning(
            this,
            tr("Could Not Save"),
            tr("The class information could not be saved.")
            );

        return;
    }

    emit saved(m_classId);
    accept();
}

void ScheduleEditorDialog::buildUi()
{
    auto* mainLayout =
        new QVBoxLayout(this);

    mainLayout->setContentsMargins(
        20,
        20,
        20,
        20
        );

    mainLayout->setSpacing(16);

    auto* title =
        new QLabel(
            tr("Edit Class Information"),
            this
            );

    title->setObjectName("pageTitle");
    title->setFont(
        FontManager::getUiFont(
            16,
            QFont::DemiBold
            )
        );

    mainLayout->addWidget(title);

    auto* form =
        new QGridLayout;

    form->setHorizontalSpacing(12);
    form->setVerticalSpacing(10);

    m_teacherKrEdit =
        new QLineEdit(this);

    m_teacherKrEdit->setReadOnly(true);

    m_roomNumberEdit =
        new QLineEdit(this);

    m_roomNumberEdit->setReadOnly(true);

    m_gradeCombo =
        new QComboBox(this);

    m_gradeCombo->addItem(QString());
    m_gradeCombo->addItems(ClassInfoConfig::Grades);

    m_levelCombo =
        new QComboBox(this);

    m_classColorPreview =
        new ClickableColorPreview(this);

    m_classColorPreview->setFixedSize(28, 28);

    auto* classColorButton =
        new QPushButton(
            tr("Choose Color"),
            this
            );

    auto* classColorLayout =
        new QHBoxLayout;

    classColorLayout->setContentsMargins(0, 0, 0, 0);
    classColorLayout->setSpacing(8);
    classColorLayout->addWidget(m_classColorPreview);
    classColorLayout->addWidget(classColorButton);
    classColorLayout->addStretch();

    m_fontColorPreview =
        new ClickableColorPreview(this);

    m_fontColorPreview->setFixedSize(28, 28);

    auto* fontColorButton =
        new QPushButton(
            tr("Choose Color"),
            this
            );

    auto* fontColorLayout =
        new QHBoxLayout;

    fontColorLayout->setContentsMargins(0, 0, 0, 0);
    fontColorLayout->setSpacing(8);
    fontColorLayout->addWidget(m_fontColorPreview);
    fontColorLayout->addWidget(fontColorButton);
    fontColorLayout->addStretch();

    form->addWidget(new QLabel(tr("Korean Teacher"), this), 0, 0);
    form->addWidget(m_teacherKrEdit, 0, 1);
    form->addWidget(new QLabel(tr("Room Number"), this), 1, 0);
    form->addWidget(m_roomNumberEdit, 1, 1);
    form->addWidget(new QLabel(tr("Class Grade"), this), 2, 0);
    form->addWidget(m_gradeCombo, 2, 1);
    form->addWidget(new QLabel(tr("Class Level"), this), 3, 0);
    form->addWidget(m_levelCombo, 3, 1);
    form->addWidget(new QLabel(tr("Class Color"), this), 4, 0);
    form->addLayout(classColorLayout, 4, 1);
    form->addWidget(new QLabel(tr("Font Color"), this), 5, 0);
    form->addLayout(fontColorLayout, 5, 1);

    mainLayout->addLayout(form);
    mainLayout->addStretch();

    auto* footerLayout =
        new QHBoxLayout;

    footerLayout->addStretch();

    auto* cancelButton =
        new QPushButton(
            tr("Cancel"),
            this
            );

    auto* saveButton =
        new QPushButton(
            tr("Save"),
            this
            );

    saveButton->setObjectName("primaryButton");

    footerLayout->addWidget(cancelButton);
    footerLayout->addWidget(saveButton);

    mainLayout->addLayout(footerLayout);

    connect(
        m_gradeCombo,
        &QComboBox::currentTextChanged,
        this,
        &ScheduleEditorDialog::updateLevelOptions
        );

    connect(
        classColorButton,
        &QPushButton::clicked,
        this,
        &ScheduleEditorDialog::chooseClassColor
        );

    connect(
        fontColorButton,
        &QPushButton::clicked,
        this,
        &ScheduleEditorDialog::chooseFontColor
        );

    connect(
        m_classColorPreview,
        &ClickableColorPreview::clicked,
        this,
        &ScheduleEditorDialog::chooseClassColor
        );

    connect(
        m_fontColorPreview,
        &ClickableColorPreview::clicked,
        this,
        &ScheduleEditorDialog::chooseFontColor
        );

    connect(
        cancelButton,
        &QPushButton::clicked,
        this,
        &QDialog::reject
        );

    connect(
        saveButton,
        &QPushButton::clicked,
        this,
        &ScheduleEditorDialog::saveChanges
        );
}

void ScheduleEditorDialog::loadData()
{
    if (!m_services || !m_services->dataService())
    {
        return;
    }

    m_loadingData = true;

    m_cachedInfo =
        m_services
            ->dataService()
            ->loadClassInfo(m_classId);

    m_originalGrade =
        m_cachedInfo.classGrade;

    m_originalLevel =
        m_cachedInfo.classLevel;

    m_teacherKrEdit->setText(
        m_cachedInfo.teacherKr
        );

    m_roomNumberEdit->setText(
        m_cachedInfo.roomNumber
        );

    setComboText(
        m_gradeCombo,
        m_originalGrade
        );

    rebuildLevelOptions(m_originalLevel);

    m_classColor =
        m_cachedInfo.classColor.isEmpty()
            ? QStringLiteral("#FFFFFF")
            : m_cachedInfo.classColor;

    m_fontColor =
        m_cachedInfo.fontColor.isEmpty()
            ? QStringLiteral("#000000")
            : m_cachedInfo.fontColor;

    updateColorPreviews();

    m_loadingData = false;
}

void ScheduleEditorDialog::updateColorPreviews()
{
    if (m_classColorPreview)
    {
        m_classColorPreview->setStyleSheet(
            QStringLiteral(
                "background:%1;"
                "border:1px solid #888;"
                "border-radius:6px;"
                ).arg(m_classColor)
            );
    }

    if (m_fontColorPreview)
    {
        m_fontColorPreview->setStyleSheet(
            QStringLiteral(
                "background:%1;"
                "border:1px solid #888;"
                "border-radius:6px;"
                ).arg(m_fontColor)
            );
    }
}

void ScheduleEditorDialog::setComboText(
    QComboBox* combo,
    const QString& text
    )
{
    if (!combo)
    {
        return;
    }

    const int index =
        combo->findText(text);

    if (index >= 0)
    {
        combo->setCurrentIndex(index);
        return;
    }

    if (!text.isEmpty())
    {
        combo->addItem(text);
        combo->setCurrentText(text);
    }
}
