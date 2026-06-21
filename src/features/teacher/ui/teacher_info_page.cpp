#include "teacher_info_page.h"

#include "core/application_services.h"
#include "core/fontmanager.h"
#include "domain/models/teacher.h"
#include "data/data_service.h"
#include "ui/shared/constants/gui_constants.h"
#include "ui/shared/styles/roles.h"
#include "ui/shared/widgets/sectioncards/teacher_section_card.h"
#include "core/utils/sidebar_node_naming.h"

#include <QComboBox>
#include "ui/shared/widgets/no_wheel_combobox.h"
#include <QFont>
#include <QFrame>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSizePolicy>
#include <QSpacerItem>
#include <QTextEdit>
#include <QTimer>
#include <QVBoxLayout>

namespace
{

constexpr int TeacherNameFieldWidth = 140;
constexpr int TeacherRoomNumberFieldWidth = 115;

void applyTeacherFieldWidth(
    QWidget* widget
    )
{
    if (!widget)
    {
        return;
    }

    widget->setMinimumWidth(
        UiConstants::ClassInfo::Teacher::FieldMinWidth
        );

    widget->setMaximumWidth(
        UiConstants::ClassInfo::Teacher::FieldMaxWidth
        );

    widget->setSizePolicy(
        QSizePolicy::Maximum,
        QSizePolicy::Preferred
        );
}

void applyFixedTeacherFieldWidth(
    QWidget* widget,
    int width
    )
{
    if (!widget)
    {
        return;
    }

    widget->setMinimumWidth(
        width
        );

    widget->setMaximumWidth(
        width
        );

    widget->setSizePolicy(
        QSizePolicy::Fixed,
        QSizePolicy::Preferred
        );
}

int findComboText(
    QComboBox* combo,
    const QString& value
    )
{
    if (!combo)
    {
        return -1;
    }

    const QString trimmed =
        value.trimmed();

    for (int index = 0; index < combo->count(); ++index)
    {
        if (combo->itemText(index).compare(trimmed, Qt::CaseInsensitive) == 0)
        {
            return index;
        }
    }

    return -1;
}

void setComboTextWithFallback(
    QComboBox* combo,
    const QString& value,
    const QString& fallback
    )
{
    int index =
        findComboText(combo, value);

    if (index < 0)
    {
        index = findComboText(combo, fallback);
    }

    if (index >= 0)
    {
        combo->setCurrentIndex(index);
    }
}

} // namespace


TeacherInfoPage::TeacherInfoPage(
    ApplicationServices* services,
    QWidget* parent
)
    : BasePage(parent),
      m_services(services)
{
    setProperty("role", UiRoles::TeacherInfo);

    buildUi();

    m_autosaveTimer =
        new QTimer(this);

    m_autosaveTimer->setSingleShot(true);
    m_autosaveTimer->setInterval(750);

    connect(
        m_autosaveTimer,
        &QTimer::timeout,
        this,
        &TeacherInfoPage::autosaveTeacher
        );
}

void TeacherInfoPage::buildUi()
{
    // =====================================================
    // Scroll Area
    // =====================================================

    m_scroll = new QScrollArea;
    m_scroll->setWidgetResizable(true);
    m_scroll->setFrameShape(QFrame::NoFrame);

    auto* scrollContainer = new QWidget;

    auto* scrollLayout = new QVBoxLayout(scrollContainer);

    scrollLayout->setContentsMargins(
        UiConstants::Pages::Margin,
        UiConstants::Pages::Margin,
        UiConstants::Pages::Margin,
        UiConstants::Pages::Margin);

    scrollLayout->setSpacing(
        UiConstants::Pages::Spacing);

    scrollLayout->setAlignment(Qt::AlignTop);

    m_scroll->setWidget(scrollContainer);

    contentLayout()->addWidget(m_scroll);

    // =====================================================
    // Header
    // =====================================================

    auto* headerLayout = new QVBoxLayout;
    headerLayout->setContentsMargins(
        UiConstants::Pages::HeaderMargin,
        UiConstants::Pages::HeaderMargin,
        UiConstants::Pages::HeaderMargin,
        UiConstants::Pages::HeaderMargin
        );
    headerLayout->setSpacing(
        UiConstants::Pages::HeaderSpacing
        );

    m_titleLabel = new QLabel(tr("Teacher Information"));
    m_titleLabel->setObjectName("pageTitle");

    m_titleLabel->setFont(
        FontManager::getUiFont(
            UiConstants::Pages::TitleFontSize,
            QFont::Bold
            )
        );

    m_subtitleLabel =
        new QLabel(tr("View and manage teacher details."));

    m_subtitleLabel->setObjectName("pageSubtitle");

    m_subtitleLabel->setFont(
        FontManager::getUiFont(
            UiConstants::Pages::SubtitleFontSize
            )
        );

    headerLayout->addWidget(m_titleLabel);
    headerLayout->addWidget(m_subtitleLabel);

    scrollLayout->addLayout(headerLayout);
    scrollLayout->addSpacing(
        UiConstants::Pages::HeaderContentSpacing
        );

    // =====================================================
    // Teacher Details
    // =====================================================

    m_detailsCard =
        new TeacherSectionCard(tr("Teacher Details"));

    auto* detailsGrid = new QGridLayout;

    detailsGrid->setHorizontalSpacing(16);
    detailsGrid->setVerticalSpacing(4);

    m_teacherKrEdit = new QLineEdit;
    m_teacherKrEdit->setFont(
        FontManager::getKoreanFont()
        );
    m_teacherEnEdit = new QLineEdit;
    m_roomNumberEdit = new QLineEdit;

    applyFixedTeacherFieldWidth(
        m_teacherKrEdit,
        TeacherNameFieldWidth
        );
    applyFixedTeacherFieldWidth(
        m_teacherEnEdit,
        TeacherNameFieldWidth
        );
    applyFixedTeacherFieldWidth(
        m_roomNumberEdit,
        TeacherRoomNumberFieldWidth
        );

    m_teacherKrLabel =
        createFieldLabel(tr("Korean Name"));

    m_teacherEnLabel =
        createFieldLabel(tr("English Name"));

    m_roomNumberLabel =
        createFieldLabel(tr("Room Number"));

    detailsGrid->addWidget(
        m_teacherKrLabel, 0, 0);

    detailsGrid->addWidget(
        m_teacherEnLabel, 0, 1);

    detailsGrid->addWidget(
        m_roomNumberLabel, 0, 2);

    detailsGrid->addWidget(m_teacherKrEdit, 1, 0);
    detailsGrid->addWidget(m_teacherEnEdit, 1, 1);
    detailsGrid->addWidget(m_roomNumberEdit, 1, 2);

    m_detailsCard->contentLayout()->addLayout(detailsGrid);

    scrollLayout->addWidget(m_detailsCard);

    // =====================================================
    // Connectivity
    // =====================================================

    m_connectivityCard =
        new TeacherSectionCard(tr("Connectivity"));

    auto* connectivityGrid = new QGridLayout;

    connectivityGrid->setHorizontalSpacing(
        UiConstants::ClassInfo::Form::HorizontalSpacing
        );

    connectivityGrid->setVerticalSpacing(
        UiConstants::ClassInfo::Form::VerticalSpacing
        );

    m_wifiNameEdit = new QLineEdit;
    m_wifiPasswordEdit = new QLineEdit;
    m_internetTypeCombo = new NoWheelComboBox;
    m_internetTypeCombo->addItems(
        {
            QStringLiteral("WiFi"),
            QStringLiteral("LAN"),
            QStringLiteral("Both"),
            QStringLiteral("N/A")
        });

    m_zoomIdEdit = new QLineEdit;
    m_zoomPasswordEdit = new QLineEdit;
    m_projectionTypeCombo = new NoWheelComboBox;
    m_projectionTypeCombo->addItems(
        {
            QStringLiteral("HDMI"),
            QStringLiteral("Zoom"),
            QStringLiteral("Any"),
            QStringLiteral("N/A")
        });

    m_internetTypeLabel =
        createFieldLabel(tr("Internet Type"));

    m_wifiNameLabel =
        createFieldLabel(tr("WiFi Name"));

    m_wifiPasswordLabel =
        createFieldLabel(tr("WiFi Password"));

    connectivityGrid->addWidget(
        m_internetTypeLabel, 0, 0, Qt::AlignLeft);

    connectivityGrid->addWidget(
        m_wifiNameLabel, 0, 1, Qt::AlignLeft);

    connectivityGrid->addWidget(
        m_wifiPasswordLabel, 0, 2, Qt::AlignLeft);

    connectivityGrid->addWidget(
        m_internetTypeCombo, 1, 0, Qt::AlignLeft);
    connectivityGrid->addWidget(
        m_wifiNameEdit, 1, 1, Qt::AlignLeft);
    connectivityGrid->addWidget(
        m_wifiPasswordEdit, 1, 2, Qt::AlignLeft);

    connectivityGrid->addItem(
        new QSpacerItem(
            0,
            UiConstants::ClassInfo::Form::GroupSpacerHeight,
            QSizePolicy::Minimum,
            QSizePolicy::Fixed
            ),
        2,
        0,
        1,
        4
        );

    m_projectionTypeLabel =
        createFieldLabel(tr("Projection Type"));

    m_zoomIdLabel =
        createFieldLabel(tr("Zoom ID"));

    m_zoomPasswordLabel =
        createFieldLabel(tr("Zoom Password"));

    connectivityGrid->addWidget(
        m_projectionTypeLabel, 3, 0, Qt::AlignLeft);

    connectivityGrid->addWidget(
        m_zoomIdLabel, 3, 1, Qt::AlignLeft);

    connectivityGrid->addWidget(
        m_zoomPasswordLabel, 3, 2, Qt::AlignLeft);

    connectivityGrid->addWidget(
        m_projectionTypeCombo, 4, 0, Qt::AlignLeft);
    connectivityGrid->addWidget(
        m_zoomIdEdit, 4, 1, Qt::AlignLeft);
    connectivityGrid->addWidget(
        m_zoomPasswordEdit, 4, 2, Qt::AlignLeft);

    for (auto* widget : {
             static_cast<QWidget*>(m_internetTypeCombo),
             static_cast<QWidget*>(m_wifiNameEdit),
             static_cast<QWidget*>(m_wifiPasswordEdit),
             static_cast<QWidget*>(m_projectionTypeCombo),
             static_cast<QWidget*>(m_zoomIdEdit),
             static_cast<QWidget*>(m_zoomPasswordEdit)
         })
    {
        applyTeacherFieldWidth(widget);
    }

    connectivityGrid->setColumnStretch(
        0,
        UiConstants::ClassInfo::Teacher::ColumnStretch
        );

    connectivityGrid->setColumnStretch(
        1,
        UiConstants::ClassInfo::Teacher::ColumnStretch
        );

    connectivityGrid->setColumnStretch(
        2,
        UiConstants::ClassInfo::Teacher::ColumnStretch
        );

    connectivityGrid->setColumnStretch(
        3,
        UiConstants::ClassInfo::Teacher::FillerColumnStretch
        );

    m_connectivityCard->contentLayout()->addLayout(
        connectivityGrid);

    scrollLayout->addWidget(m_connectivityCard);

    // =====================================================
    // Notes
    // =====================================================

    m_notesCard =
        new TeacherSectionCard(tr("Notes"));

    m_notesEdit = new QTextEdit;
    m_notesEdit->setMinimumHeight(180);

    m_notesCard->contentLayout()->addWidget(m_notesEdit);

    scrollLayout->addWidget(m_notesCard);

    // =====================================================
    // Footer
    // =====================================================

    m_saveButton =
        new QPushButton(tr("Save Changes"));

    m_saveButton->setObjectName(
        "primaryButton");

    connect(
        m_saveButton,
        &QPushButton::clicked,
        this,
        &TeacherInfoPage::saveTeacher);

    for (auto* edit : {
             m_teacherKrEdit,
             m_teacherEnEdit,
             m_roomNumberEdit,
             m_wifiNameEdit,
             m_wifiPasswordEdit,
             m_zoomIdEdit,
             m_zoomPasswordEdit
         })
    {
        connect(
            edit,
            &QLineEdit::textChanged,
            this,
            &TeacherInfoPage::handleFieldChanged
            );
    }

    connect(
        m_notesEdit,
        &QTextEdit::textChanged,
        this,
        &TeacherInfoPage::handleFieldChanged
        );

    connect(
        m_internetTypeCombo,
        &QComboBox::currentTextChanged,
        this,
        &TeacherInfoPage::handleFieldChanged
        );

    connect(
        m_projectionTypeCombo,
        &QComboBox::currentTextChanged,
        this,
        &TeacherInfoPage::handleFieldChanged
        );

    bottomLayout()->addStretch();
    bottomLayout()->addWidget(m_saveButton);

    updateActions();
}

QLabel* TeacherInfoPage::createFieldLabel(
    const QString& text)
{
    auto* label = new QLabel(text);
    label->setContentsMargins(
        UiConstants::ClassInfo::Form::LabelIndent,
        0,
        0,
        0
        );

    return label;
}

void TeacherInfoPage::loadTeacher(
    const Teacher& teacher)
{
    m_loading = true;

    if (m_autosaveTimer)
    {
        m_autosaveTimer->stop();
    }

    m_teacher = teacher;

    QString displayName =
        SidebarNodeNaming::formatTeacherDisplayName(teacher);

    m_titleLabel->setText(
        tr("Teacher Information for %1")
            .arg(displayName));

    m_teacherKrEdit->setText(
        teacher.teacherKr);

    m_teacherEnEdit->setText(
        teacher.teacherEn);

    m_roomNumberEdit->setText(
        teacher.roomNumber);

    setComboTextWithFallback(
        m_internetTypeCombo,
        teacher.internetType,
        QStringLiteral("WiFi")
        );

    m_wifiNameEdit->setText(
        teacher.wifiName);

    m_wifiPasswordEdit->setText(
        teacher.wifiPassword);

    setComboTextWithFallback(
        m_projectionTypeCombo,
        teacher.projectionType,
        QStringLiteral("HDMI")
        );

    m_zoomIdEdit->setText(
        teacher.zoomId);

    m_zoomPasswordEdit->setText(
        teacher.zoomPassword);

    m_notesEdit->setPlainText(
        teacher.notes);

    m_loading = false;
    clearDirty();
}

void TeacherInfoPage::saveData()
{
    saveTeacher();
}

bool TeacherInfoPage::saveChanges()
{
    if (!m_dirty)
    {
        return true;
    }

    return saveTeacherInternal();
}

bool TeacherInfoPage::hasUnsavedChanges() const
{
    return m_dirty;
}

void TeacherInfoPage::discardChanges()
{
    if (m_autosaveTimer)
    {
        m_autosaveTimer->stop();
    }

    loadTeacher(m_teacher);
}

QString TeacherInfoPage::unsavedChangesTitle() const
{
    return tr("Unsaved Teacher Changes");
}

QString TeacherInfoPage::unsavedChangesMessage() const
{
    return tr("This teacher has unsaved changes.");
}

void TeacherInfoPage::setSaveMode(
    SaveMode mode
    )
{
    if (m_saveMode == mode)
    {
        return;
    }

    m_saveMode = mode;

    updateActions();

    if (!m_autosaveTimer)
    {
        return;
    }

    if (
        m_saveMode == SaveMode::Automatic
        && m_dirty
        )
    {
        m_autosaveTimer->start();
    }
    else
    {
        m_autosaveTimer->stop();
    }
}

void TeacherInfoPage::saveTeacher()
{
    saveTeacherInternal();
}

void TeacherInfoPage::handleFieldChanged()
{
    if (m_loading)
    {
        return;
    }

    m_dirty =
        formDiffersFromTeacher();

    updateActions();

    if (!m_autosaveTimer)
    {
        return;
    }

    if (
        m_dirty
        && m_saveMode == SaveMode::Automatic
        )
    {
        m_autosaveTimer->start();
    }
    else
    {
        m_autosaveTimer->stop();
    }
}

void TeacherInfoPage::autosaveTeacher()
{
    if (
        !m_dirty
        || m_teacher.id <= 0
        )
    {
        return;
    }

    saveTeacherInternal();
}

Teacher TeacherInfoPage::teacherFromForm() const
{
    Teacher updated = m_teacher;

    updated.teacherKr = m_teacherKrEdit->text().trimmed();
    updated.teacherEn = m_teacherEnEdit->text().trimmed();

    updated.roomNumber = m_roomNumberEdit->text().trimmed();

    updated.internetType = m_internetTypeCombo->currentText().trimmed();
    updated.wifiName = m_wifiNameEdit->text().trimmed();
    updated.wifiPassword = m_wifiPasswordEdit->text().trimmed();

    updated.projectionType = m_projectionTypeCombo->currentText().trimmed();
    updated.zoomId = m_zoomIdEdit->text().trimmed();
    updated.zoomPassword = m_zoomPasswordEdit->text().trimmed();

    updated.notes = m_notesEdit->toPlainText().trimmed();

    return updated;
}

bool TeacherInfoPage::formDiffersFromTeacher() const
{
    const Teacher updated =
        teacherFromForm();

    return updated.teacherKr != m_teacher.teacherKr.trimmed()
        || updated.teacherEn != m_teacher.teacherEn.trimmed()
        || updated.roomNumber != m_teacher.roomNumber.trimmed()
        || updated.internetType != m_teacher.internetType.trimmed()
        || updated.wifiName != m_teacher.wifiName.trimmed()
        || updated.wifiPassword != m_teacher.wifiPassword.trimmed()
        || updated.projectionType != m_teacher.projectionType.trimmed()
        || updated.zoomId != m_teacher.zoomId.trimmed()
        || updated.zoomPassword != m_teacher.zoomPassword.trimmed()
        || updated.notes != m_teacher.notes.trimmed();
}

bool TeacherInfoPage::saveTeacherInternal()
{
    if (
        !m_services
        || !m_services->dataService()
        || m_teacher.id <= 0
        )
    {
        return false;
    }

    if (m_autosaveTimer)
    {
        m_autosaveTimer->stop();
    }

    auto* dataService = m_services->dataService();

    const Teacher updated =
        teacherFromForm();

    dataService->updateTeacher(updated);

    m_teacher =
        dataService->getTeacher(
            m_teacher.id
            );

    const QString displayName =
        SidebarNodeNaming::formatTeacherDisplayName(m_teacher);

    m_titleLabel->setText(
        tr("Teacher Information for %1")
            .arg(displayName)
        );

    clearDirty();

    emit teacherSaved(
        m_teacher.id
        );

    return !m_dirty;
}

void TeacherInfoPage::clearDirty()
{
    m_dirty = false;
    updateActions();
}

void TeacherInfoPage::updateActions()
{
    if (!m_saveButton)
    {
        return;
    }

    const bool showSaveButton =
        m_saveMode != SaveMode::Automatic;

    m_saveButton->setVisible(
        showSaveButton
        );

    m_saveButton->setEnabled(
        showSaveButton
        && m_dirty
        && m_teacher.id > 0
        );

    m_saveButton->setText(
        m_dirty
            ? tr("Save Changes *")
            : tr("Save Changes")
        );
}

Teacher TeacherInfoPage::teacher() const
{
    return m_teacher;
}

void TeacherInfoPage::refresh()
{
    BasePage::refresh();
}

void TeacherInfoPage::retranslateUi()
{
    if (m_titleLabel)
    {
        if (m_teacher.id > 0)
        {
            m_titleLabel->setText(
                tr("Teacher Information for %1")
                    .arg(
                        SidebarNodeNaming::formatTeacherDisplayName(
                            m_teacher
                            )
                        )
                );
        }
        else
        {
            m_titleLabel->setText(
                tr("Teacher Information")
                );
        }
    }

    if (m_subtitleLabel)
    {
        m_subtitleLabel->setText(
            tr("View and manage teacher details.")
            );
    }

    if (m_detailsCard)
    {
        m_detailsCard->setTitle(
            tr("Teacher Details")
            );
    }

    if (m_connectivityCard)
    {
        m_connectivityCard->setTitle(
            tr("Connectivity")
            );
    }

    if (m_notesCard)
    {
        m_notesCard->setTitle(
            tr("Notes")
            );
    }

    if (m_teacherKrLabel)
    {
        m_teacherKrLabel->setText(
            tr("Korean Name")
            );
    }

    if (m_teacherEnLabel)
    {
        m_teacherEnLabel->setText(
            tr("English Name")
            );
    }

    if (m_roomNumberLabel)
    {
        m_roomNumberLabel->setText(
            tr("Room Number")
            );
    }

    if (m_internetTypeLabel)
    {
        m_internetTypeLabel->setText(
            tr("Internet Type")
            );
    }

    if (m_wifiNameLabel)
    {
        m_wifiNameLabel->setText(
            tr("WiFi Name")
            );
    }

    if (m_wifiPasswordLabel)
    {
        m_wifiPasswordLabel->setText(
            tr("WiFi Password")
            );
    }

    if (m_projectionTypeLabel)
    {
        m_projectionTypeLabel->setText(
            tr("Projection Type")
            );
    }

    if (m_zoomIdLabel)
    {
        m_zoomIdLabel->setText(
            tr("Zoom ID")
            );
    }

    if (m_zoomPasswordLabel)
    {
        m_zoomPasswordLabel->setText(
            tr("Zoom Password")
            );
    }

    updateActions();
}
