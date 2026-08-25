#include "class_co_teacher_page.h"

#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "core/fontmanager.h"
#include "core/utils/sidebar_node_naming.h"
#include "domain/models/class_info.h"
#include "domain/models/teacher.h"
#include "ui/shared/constants/gui_constants.h"
#include "ui/shared/dialogs/user_prompt_service.h"
#include "ui/shared/pages/autosave_coordinator.h"
#include "ui/shared/pages/page_header.h"
#include "ui/shared/pages/scrollable_page_body.h"
#include "ui/shared/styles/roles.h"
#include "ui/shared/widgets/sections/teacher_info_section.h"
#include "ui/shared/widgets/sectioncards/class_info_section_card.h"
#include "ui/shared/widgets/text_fit_push_button.h"

#include <QFont>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QtAssert>

ClassCoTeacherPage::ClassCoTeacherPage(
    ApplicationServices* services,
    bool embedded,
    QWidget* parent
    )
    : BasePage(parent)
    , m_services(services)
    , m_embedded(embedded)
    , m_autosave(new AutosaveCoordinator(this))
{
    Q_ASSERT(m_services);

    setProperty("role", UiRoles::ClassInfo);

    if (m_embedded)
    {
        setPageLayoutMargins({});
    }

    buildUi();
    m_autosave->bindSaveButton(m_saveButton);
    connect(
        m_autosave,
        &AutosaveCoordinator::saveRequested,
        this,
        [this](bool interactive) {
            saveCoTeacherInternal(interactive);
        }
        );
    connect(
        m_teacherSection,
        &TeacherInfoSection::dataChanged,
        this,
        &ClassCoTeacherPage::markDirty
        );
}

void ClassCoTeacherPage::buildUi()
{
    contentLayout()->setContentsMargins(
        m_embedded ? 0 : UiConstants::Pages::Margin,
        m_embedded ? 0 : UiConstants::Pages::Margin,
        m_embedded ? 0 : UiConstants::Pages::Margin,
        0
        );
    contentLayout()->setSpacing(
        m_embedded ? 12 : UiConstants::Pages::Spacing
        );

    if (m_embedded)
    {
        m_embeddedHeading = new QLabel(tr("Co-Teacher"), this);
        m_embeddedHeading->setObjectName(
            QStringLiteral("classCoTeacherHeading")
            );
        m_embeddedHeading->setFont(
            FontManager::getUiFont(18, QFont::DemiBold)
            );
        contentLayout()->addWidget(m_embeddedHeading);
    }

    m_pageHeader = new PageHeader(
        tr("Co-Teacher"),
        tr("No class selected"),
        this
        );
    if (m_embedded)
    {
        m_pageHeader->hide();
    }
    else
    {
        contentLayout()->addWidget(m_pageHeader);
        contentLayout()->addSpacing(
            UiConstants::Pages::HeaderContentSpacing
            );
    }

    m_pageBody = new ScrollablePageBody(
        this,
        QMargins(0, 0, 0, 0),
        UiConstants::ClassInfo::Page::ContentSpacing
        );
    contentLayout()->addWidget(m_pageBody);

    m_teacherCard = new SectionCard(
        tr("Korean Teacher"),
        m_pageBody->contentWidget()
        );
    m_teacherCard->setObjectName(
        QStringLiteral("classKoreanTeacherCard")
        );
    m_pageBody->contentLayout()->addWidget(
        m_teacherCard,
        0,
        Qt::AlignTop
        );

    m_teacherSection = new TeacherInfoSection(m_teacherCard);
    m_teacherCard->contentLayout()->addWidget(m_teacherSection);

    m_saveButton = new TextFitPushButton(tr("Save Changes"));
    m_saveButton->setEnabled(false);
    m_saveButton->setObjectName(
        QStringLiteral("classCoTeacherSaveButton")
        );
    bottomLayout()->addStretch();
    bottomLayout()->addWidget(m_saveButton);

    updateActions();
}

void ClassCoTeacherPage::loadClass(
    const Classroom& classroom
    )
{
    m_autosave->setLoading(true);
    refresh();
    m_classroom = classroom;

    const Result<QList<Teacher>> teachers =
        m_services->teacherService()->teachers();
    if (!teachers)
    {
        DialogServices::showWarning(
            this,
            tr("Load Co-Teacher"),
            tr("Teachers could not be loaded."),
            teachers.error()
            );
        m_autosave->setLoading(false);
        clearDatabaseState();
        return;
    }

    m_teacherSection->setTeachers(*teachers);
    const ClassInfo info =
        m_services->classService()
            ->classInfo(classroom.id)
            .value_or(ClassInfo{});
    m_teacherSection->selectTeacher(info.teacherId);
    updateTitle();

    m_autosave->setLoading(false);
    clearDirty();
}

void ClassCoTeacherPage::clearDatabaseState()
{
    m_autosave->setLoading(true);
    m_classroom = {};
    m_teacherSection->setTeachers({});
    m_teacherSection->selectTeacher(-1);
    updateTitle();
    m_autosave->setLoading(false);
    clearDirty();
}

void ClassCoTeacherPage::refresh()
{
    BasePage::refresh();
}

void ClassCoTeacherPage::saveData()
{
    saveCoTeacherInternal(true);
}

bool ClassCoTeacherPage::saveChanges()
{
    m_autosave->cancelPendingSave();

    return !hasUnsavedChanges() || saveCoTeacherInternal(true);
}

bool ClassCoTeacherPage::hasUnsavedChanges() const
{
    return m_autosave->isDirty();
}

void ClassCoTeacherPage::discardChanges()
{
    m_autosave->cancelPendingSave();
    loadClass(m_classroom);
}

void ClassCoTeacherPage::setSaveMode(
    SaveMode mode
    )
{
    m_autosave->setSaveMode(mode);
}

void ClassCoTeacherPage::retranslateUi()
{
    if (m_embeddedHeading)
    {
        m_embeddedHeading->setText(tr("Co-Teacher"));
    }

    m_pageHeader->setTitle(tr("Co-Teacher"));
    updateTitle();

    if (m_teacherCard)
    {
        m_teacherCard->setTitle(tr("Korean Teacher"));
    }

    if (m_teacherSection)
    {
        m_teacherSection->retranslateUi();
    }

    updateActions();
}

void ClassCoTeacherPage::updateTitle()
{
    if (m_classroom.id <= 0)
    {
        m_pageHeader->setSubtitle(tr("No class selected"));
        return;
    }

    const ClassInfo info =
        m_services->classService()
            ->classInfo(m_classroom.id)
            .value_or(ClassInfo{});
    Teacher teacher;
    if (info.teacherId > 0)
    {
        teacher = m_services->teacherService()
            ->teacher(info.teacherId)
            .value_or(Teacher{});
    }

    const QString displayName =
        SidebarNodeNaming::formatClassDisplayName(info, teacher).trimmed();
    m_pageHeader->setSubtitle(
        !displayName.isEmpty()
            ? displayName
            : !m_classroom.name.trimmed().isEmpty()
                ? m_classroom.name.trimmed()
                : tr("Class %1").arg(m_classroom.id)
        );
}

void ClassCoTeacherPage::markDirty()
{
    if (!m_autosave->isLoading())
    {
        m_autosave->markDirty();
    }
}

void ClassCoTeacherPage::clearDirty()
{
    m_autosave->markClean();
    updateActions();
}

void ClassCoTeacherPage::updateActions()
{
    m_autosave->setSaveAvailable(m_classroom.id >= 0);
    m_autosave->setSaveMode(m_autosave->saveMode());
}

bool ClassCoTeacherPage::saveCoTeacherInternal(
    bool showMessages
    )
{
    if (m_classroom.id < 0)
    {
        return true;
    }

    ClassInfo info =
        m_services->classService()
            ->classInfo(m_classroom.id)
            .value_or(ClassInfo{});
    info.classId = m_classroom.id;
    info.teacherId = m_teacherSection->teacherId();

    const Status saved =
        m_services->classService()->saveClassInfo(info);
    if (!saved)
    {
        m_autosave->markDirty(false);

        if (showMessages)
        {
            DialogServices::showWarning(
                this,
                tr("Save Co-Teacher"),
                saved.error()
                );
        }

        return false;
    }

    clearDirty();
    updateTitle();
    emit classInfoSaved(m_classroom.id);
    return true;
}
