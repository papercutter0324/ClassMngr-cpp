#include "teacher_info_page.h"

#include "core/application_services.h"
#include "core/fontmanager.h"
#include "models/teacher.h"
#include "services/dataservice.h"
#include "ui/constants/gui_constants.h"
#include "ui/widgets/sectioncards/teacher_section_card.h"
#include "utils/sidebar_node_naming.h"

#include <QFont>
#include <QFrame>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QTextEdit>
#include <QVBoxLayout>


TeacherInfoPage::TeacherInfoPage(
    ApplicationServices* services,
    QWidget* parent
)
    : BasePage(parent),
      m_services(services)
{
    buildUi();
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

    m_titleLabel = new QLabel("Teacher Information");
    m_titleLabel->setObjectName("pageTitle");

    m_titleLabel->setFont(
        FontManager::getUiFont(
            24,
            QFont::Bold
            )
        );

    m_subtitleLabel =
        new QLabel("View and manage teacher details.");

    m_subtitleLabel->setObjectName("pageSubtitle");

    m_subtitleLabel->setFont(
        FontManager::getUiFont(11)
        );

    headerLayout->addWidget(m_titleLabel);
    headerLayout->addWidget(m_subtitleLabel);

    scrollLayout->addLayout(headerLayout);

    // =====================================================
    // Teacher Details
    // =====================================================

    auto* detailsCard =
        new SectionCard("Teacher Details");

    auto* detailsGrid = new QGridLayout;

    detailsGrid->setHorizontalSpacing(16);
    detailsGrid->setVerticalSpacing(4);

    m_teacherKrEdit = new QLineEdit;
    m_teacherEnEdit = new QLineEdit;
    m_roomNumberEdit = new QLineEdit;

    detailsGrid->addWidget(
        createFieldLabel("Korean Name"), 0, 0);

    detailsGrid->addWidget(
        createFieldLabel("English Name"), 0, 1);

    detailsGrid->addWidget(
        createFieldLabel("Room Number"), 0, 2);

    detailsGrid->addWidget(m_teacherKrEdit, 1, 0);
    detailsGrid->addWidget(m_teacherEnEdit, 1, 1);
    detailsGrid->addWidget(m_roomNumberEdit, 1, 2);

    detailsCard->contentLayout()->addLayout(detailsGrid);

    scrollLayout->addWidget(detailsCard);

    // =====================================================
    // Connectivity
    // =====================================================

    auto* connectivityCard =
        new SectionCard("Connectivity");

    auto* connectivityGrid = new QGridLayout;

    connectivityGrid->setHorizontalSpacing(16);
    connectivityGrid->setVerticalSpacing(4);

    m_wifiNameEdit = new QLineEdit;
    m_wifiPasswordEdit = new QLineEdit;
    m_zoomIdEdit = new QLineEdit;
    m_zoomPasswordEdit = new QLineEdit;

    connectivityGrid->addWidget(
        createFieldLabel("WiFi Name"), 0, 0);

    connectivityGrid->addWidget(
        createFieldLabel("WiFi Password"), 0, 1);

    connectivityGrid->addWidget(
        createFieldLabel("Zoom ID"), 0, 2);

    connectivityGrid->addWidget(
        createFieldLabel("Zoom Password"), 0, 3);

    connectivityGrid->addWidget(m_wifiNameEdit, 1, 0);
    connectivityGrid->addWidget(m_wifiPasswordEdit, 1, 1);
    connectivityGrid->addWidget(m_zoomIdEdit, 1, 2);
    connectivityGrid->addWidget(m_zoomPasswordEdit, 1, 3);

    connectivityCard->contentLayout()->addLayout(
        connectivityGrid);

    scrollLayout->addWidget(connectivityCard);

    // =====================================================
    // Notes
    // =====================================================

    auto* notesCard =
        new SectionCard("Notes");

    m_notesEdit = new QTextEdit;
    m_notesEdit->setMinimumHeight(180);

    notesCard->contentLayout()->addWidget(m_notesEdit);

    scrollLayout->addWidget(notesCard);

    // =====================================================
    // Footer
    // =====================================================

    m_saveButton =
        new QPushButton("Save Changes");

    m_saveButton->setObjectName(
        "primaryButton");

    connect(
        m_saveButton,
        &QPushButton::clicked,
        this,
        &TeacherInfoPage::saveTeacher);

    bottomLayout()->addStretch();
    bottomLayout()->addWidget(m_saveButton);
}

QLabel* TeacherInfoPage::createFieldLabel(
    const QString& text)
{
    auto* label = new QLabel(text);
    label->setContentsMargins(4, 0, 0, 0);
    return label;
}

void TeacherInfoPage::loadTeacher(
    const Teacher& teacher)
{
    m_teacher = teacher;

    QString displayName =
        SidebarNodeNaming::teacherDisplayName(teacher);

    m_titleLabel->setText(
        QString("Teacher Information for %1")
            .arg(displayName));

    m_teacherKrEdit->setText(
        teacher.teacherKr);

    m_teacherEnEdit->setText(
        teacher.teacherEn);

    m_roomNumberEdit->setText(
        teacher.roomNumber);

    m_wifiNameEdit->setText(
        teacher.wifiName);

    m_wifiPasswordEdit->setText(
        teacher.wifiPassword);

    m_zoomIdEdit->setText(
        teacher.zoomId);

    m_zoomPasswordEdit->setText(
        teacher.zoomPassword);

    m_notesEdit->setPlainText(
        teacher.notes);
}

void TeacherInfoPage::saveData()
{
    if (m_teacher.id <= 0)
        return;

    saveTeacher();
}

void TeacherInfoPage::saveTeacher()
{
    if (m_teacher.id <= 0)
        return;

    auto* dataService = m_services->dataService();

    // Build updated model directly from UI
    Teacher updated = m_teacher;

    updated.teacherKr = m_teacherKrEdit->text().trimmed();
    updated.teacherEn = m_teacherEnEdit->text().trimmed();

    updated.roomNumber = m_roomNumberEdit->text().trimmed();

    updated.wifiName = m_wifiNameEdit->text().trimmed();
    updated.wifiPassword = m_wifiPasswordEdit->text().trimmed();

    updated.zoomId = m_zoomIdEdit->text().trimmed();
    updated.zoomPassword = m_zoomPasswordEdit->text().trimmed();

    updated.notes = m_notesEdit->toPlainText().trimmed();

    dataService->updateTeacher(updated);

    m_teacher = dataService->getTeacher(m_teacher.id);
    loadTeacher(m_teacher);
}

void TeacherInfoPage::refresh()
{
    BasePage::refresh();
}