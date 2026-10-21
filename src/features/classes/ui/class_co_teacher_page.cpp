#include "class_co_teacher_page.h"

#include "core/application_services.h"
#include "core/fontmanager.h"
#include "core/utils/sidebar_node_naming.h"
#include "domain/models/class_info.h"
#include "domain/models/teacher.h"
#include "next/application/class_co_teacher_assignment_use_case.h"
#include "next/application/class_co_teacher_page_read_query.h"
#include "next/application/class_co_teacher_teacher_choices_read_query.h"
#include "next/platform/application_services_class_co_teacher_page_read_port.h"
#include "next/platform/application_services_class_co_teacher_assignment_port.h"
#include "next/platform/application_services_class_co_teacher_teacher_choices_read_port.h"
#include "ui/shared/constants/gui_constants.h"
#include "ui/shared/dialogs/user_prompt_service.h"
#include "ui/shared/pages/autosave_coordinator.h"
#include "ui/shared/pages/page_header.h"
#include "ui/shared/pages/scrollable_page_body.h"
#include "ui/shared/styles/roles.h"
#include "ui/shared/widgets/sections/teacher_info_section.h"
#include "ui/shared/widgets/sectioncards/class_info_section_card.h"
#include "ui/shared/widgets/text_fit_push_button.h"

#include <QComboBox>
#include <QFont>
#include <QHash>
#include <QLabel>
#include <QPushButton>
#include <QSet>
#include <QSizePolicy>
#include <QVector>
#include <QVBoxLayout>
#include <QtAssert>

#include <charconv>
#include <limits>
#include <string>
#include <system_error>
#include <utility>

namespace
{

std::optional<qint64> stableTeacherIdValue(
    const ClassMngr::Next::Domain::TeacherId& teacherId
    )
{
    const std::string& value = teacherId.value();
    qint64 parsed = 0;
    const auto [end, error] = std::from_chars(
        value.data(),
        value.data() + value.size(),
        parsed
        );
    if (error != std::errc{}
        || end != value.data() + value.size()
        || parsed <= 0
        || std::to_string(parsed) != value)
    {
        return std::nullopt;
    }
    return parsed;
}

Teacher uiTeacher(
    const ClassMngr::Next::Application::ClassCoTeacherTeacherChoice& source,
    const int uiTeacherId
    )
{
    Teacher teacher;
    teacher.id = uiTeacherId;
    teacher.teacherKr = QString::fromStdU16String(source.teacherKr);
    teacher.teacherEn = QString::fromStdU16String(source.teacherEn);
    teacher.roomNumber = QString::fromStdU16String(source.roomNumber);
    teacher.internetType = QString::fromStdU16String(source.internetType);
    teacher.wifiName = QString::fromStdU16String(source.wifiName);
    teacher.wifiPassword = QString::fromStdU16String(source.wifiPassword);
    teacher.projectionType = QString::fromStdU16String(source.projectionType);
    teacher.zoomId = QString::fromStdU16String(source.zoomId);
    teacher.zoomPassword = QString::fromStdU16String(source.zoomPassword);
    return teacher;
}

QString operationErrorCodeName(
    const ClassMngr::Next::Domain::ErrorCode code
    )
{
    using ClassMngr::Next::Domain::ErrorCode;
    switch (code)
    {
    case ErrorCode::InvalidInput:
        return QStringLiteral("InvalidInput");
    case ErrorCode::NotFound:
        return QStringLiteral("NotFound");
    case ErrorCode::Conflict:
        return QStringLiteral("Conflict");
    case ErrorCode::Validation:
        return QStringLiteral("Validation");
    case ErrorCode::Canceled:
        return QStringLiteral("Canceled");
    case ErrorCode::Technical:
        return QStringLiteral("Technical");
    }

    return QStringLiteral("Unknown");
}

} // namespace

ClassCoTeacherPage::ClassCoTeacherPage(
    ApplicationServices* services,
    bool embedded,
    QWidget* parent,
    ClassMngr::Next::Application::ClassCoTeacherAssignmentPort*
        assignmentPort,
    ClassMngr::Next::Application::ClassCoTeacherPageReadPort*
        readPort,
    ClassMngr::Next::Application::ClassCoTeacherTeacherChoicesReadPort*
        teacherChoicesReadPort
    )
    : BasePage(parent)
    , m_services(services)
    , m_assignmentPort(assignmentPort)
    , m_readPort(readPort)
    , m_teacherChoicesReadPort(teacherChoicesReadPort)
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

    m_assignmentReadStatusLabel = new QLabel(
        m_pageBody->contentWidget()
        );
    m_assignmentReadStatusLabel->setObjectName(
        QStringLiteral("classCoTeacherAssignmentReadStatus")
        );
    m_assignmentReadStatusLabel->setAccessibleName(
        tr("Co-Teacher assignment status")
        );
    m_assignmentReadStatusLabel->setAlignment(
        Qt::AlignLeft | Qt::AlignVCenter
        );
    m_assignmentReadStatusLabel->setWordWrap(true);
    m_assignmentReadStatusLabel->setSizePolicy(
        QSizePolicy::Expanding,
        QSizePolicy::Preferred
        );
    m_assignmentReadStatusLabel->hide();
    m_pageBody->contentLayout()->addWidget(
        m_assignmentReadStatusLabel,
        0,
        Qt::AlignTop
        );

    m_automaticSaveFailureStatusLabel = new QLabel(
        m_pageBody->contentWidget()
        );
    m_automaticSaveFailureStatusLabel->setObjectName(
        QStringLiteral("classCoTeacherAutosaveFailureStatus")
        );
    m_automaticSaveFailureStatusLabel->setAccessibleName(tr("Save status"));
    m_automaticSaveFailureStatusLabel->setAlignment(
        Qt::AlignLeft | Qt::AlignVCenter
        );
    m_automaticSaveFailureStatusLabel->setWordWrap(true);
    m_automaticSaveFailureStatusLabel->setSizePolicy(
        QSizePolicy::Expanding,
        QSizePolicy::Preferred
        );
    m_automaticSaveFailureStatusLabel->hide();
    m_pageBody->contentLayout()->addWidget(
        m_automaticSaveFailureStatusLabel,
        0,
        Qt::AlignTop
        );

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
    m_titleTeacherNameFallback.clear();
    refresh();
    m_classroom = classroom;

    ClassMngr::Next::Platform::
        ApplicationServicesClassCoTeacherTeacherChoicesReadPort
            defaultTeacherChoicesReadPort(m_services);
    const ClassMngr::Next::Application::
        ClassCoTeacherTeacherChoicesReadPort& teacherChoicesReadPort =
            m_teacherChoicesReadPort
                ? *m_teacherChoicesReadPort
                : defaultTeacherChoicesReadPort;
    const ClassMngr::Next::Application::
        ClassCoTeacherTeacherChoicesReadQuery teacherChoicesQuery(
            teacherChoicesReadPort
            );
    const auto teacherChoices = teacherChoicesQuery.execute();
    if (!teacherChoices)
    {
        const std::string& message = teacherChoices.error().message;
        DialogServices::showWarning(
            this,
            tr("Load Co-Teacher"),
            tr("Teachers could not be loaded."),
            QString::fromUtf8(
                message.data(),
                static_cast<qsizetype>(message.size())
                )
            );
        m_autosave->setLoading(false);
        clearDatabaseState();
        return;
    }

    QVector<qint64> stableTeacherIds;
    stableTeacherIds.reserve(static_cast<qsizetype>(
        teacherChoices.value().teachers.size()
        ));
    QSet<int> reservedUiIds;
    for (const auto& teacher : teacherChoices.value().teachers)
    {
        const std::optional<qint64> teacherId =
            stableTeacherIdValue(teacher.teacherId);
        if (!teacherId)
        {
            DialogServices::showWarning(
                this,
                tr("Load Co-Teacher"),
                tr("Teachers could not be loaded."),
                tr("A teacher has an invalid identifier.")
                );
            m_autosave->setLoading(false);
            clearDatabaseState();
            return;
        }
        stableTeacherIds.append(*teacherId);
        if (*teacherId <= std::numeric_limits<int>::max())
        {
            reservedUiIds.insert(static_cast<int>(*teacherId));
        }
    }

    m_stableTeacherIdsByUiId.clear();
    m_uiIdsByStableTeacherId.clear();
    QList<Teacher> teachers;
    teachers.reserve(static_cast<qsizetype>(
        teacherChoices.value().teachers.size()
        ));
    qint64 nextUiId = 1;
    for (std::size_t index = 0;
         index < teacherChoices.value().teachers.size();
         ++index)
    {
        const qint64 stableTeacherId = stableTeacherIds[static_cast<qsizetype>(
            index
            )];
        int uiTeacherId = -1;
        if (stableTeacherId <= std::numeric_limits<int>::max())
        {
            uiTeacherId = static_cast<int>(stableTeacherId);
        }
        else
        {
            while (nextUiId <= std::numeric_limits<int>::max()
                && reservedUiIds.contains(static_cast<int>(nextUiId)))
            {
                ++nextUiId;
            }
            if (nextUiId > std::numeric_limits<int>::max())
            {
                DialogServices::showWarning(
                    this,
                    tr("Load Co-Teacher"),
                    tr("Teachers could not be loaded."),
                    tr("Teacher selections could not be mapped safely.")
                    );
                m_autosave->setLoading(false);
                clearDatabaseState();
                return;
            }
            uiTeacherId = static_cast<int>(nextUiId++);
            reservedUiIds.insert(uiTeacherId);
        }

        m_stableTeacherIdsByUiId.insert(uiTeacherId, stableTeacherId);
        m_uiIdsByStableTeacherId.insert(stableTeacherId, uiTeacherId);
        teachers.append(uiTeacher(
            teacherChoices.value().teachers[index],
            uiTeacherId
            ));
    }
    m_teacherSection->setTeachers(teachers);
    (void)readSelectedClassSnapshot();
    selectCachedTeacher();
    updateTitle();

    m_autosave->setLoading(false);
    clearDirty();
}

void ClassCoTeacherPage::clearDatabaseState()
{
    m_autosave->setLoading(true);
    clearAssignmentReadFailure();
    m_classroom = {};
    m_readSnapshot.reset();
    m_titleTeacherNameFallback.clear();
    m_stableTeacherIdsByUiId.clear();
    m_uiIdsByStableTeacherId.clear();
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
    if (m_assignmentReadError)
    {
        return;
    }

    saveCoTeacherInternal(true);
}

bool ClassCoTeacherPage::saveChanges()
{
    m_autosave->cancelPendingSave();

    if (m_assignmentReadError && hasUnsavedChanges())
    {
        return false;
    }

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

    if (m_assignmentReadStatusLabel)
    {
        m_assignmentReadStatusLabel->setAccessibleName(
            tr("Co-Teacher assignment status")
            );
        updateAssignmentReadStatus();
    }

    if (m_automaticSaveFailureStatusLabel)
    {
        m_automaticSaveFailureStatusLabel->setAccessibleName(tr("Save status"));
        if (m_automaticSaveFailureStatusActive)
        {
            m_automaticSaveFailureStatusLabel->setText(
                tr(
                    "Your co-teacher assignment could not be saved. "
                    "Your changes are still here."
                    )
                );
        }
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

    ClassInfo info;
    Teacher teacher;
    bool hasClassFields = false;
    if (m_readSnapshot)
    {
        const auto& classFields = m_readSnapshot->classFields;
        if (classFields)
        {
            hasClassFields = true;
            info.classId = m_classroom.id;
            info.teacherId = selectedTeacherIdFromSnapshot() > 0 ? 1 : -1;
            info.classGrade = QString::fromStdU16String(
                classFields.value().classGrade
                );
            info.classLevel = QString::fromStdU16String(
                classFields.value().classLevel
                );
            info.classTimes.reserve(static_cast<qsizetype>(
                classFields.value().regularSchedule.size()
                ));
            for (const auto& row : classFields.value().regularSchedule)
            {
                ClassTime time;
                time.day = QString::fromStdU16String(row.day());
                time.startTime = QString::fromStdU16String(row.startTime());
                info.classTimes.append(std::move(time));
            }

            const auto& teacherDisplayName = m_readSnapshot->teacherDisplayName;
            if (info.teacherId > 0 && teacherDisplayName)
            {
                teacher.preferredName = QString::fromStdU16String(
                    teacherDisplayName.value()
                    );
            }
        }
    }

    if (!m_titleTeacherNameFallback.isEmpty())
    {
        info.teacherId = 1;
        teacher.preferredName = m_titleTeacherNameFallback;
        if (!hasClassFields)
        {
            const QString className = !m_classroom.name.trimmed().isEmpty()
                ? m_classroom.name.trimmed()
                : tr("Class %1").arg(m_classroom.id);
            m_pageHeader->setSubtitle(
                QStringLiteral("%1 • %2").arg(
                    className,
                    m_titleTeacherNameFallback
                    )
                );
            return;
        }
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
    clearAutomaticSaveFailureStatus();
    m_autosave->markClean();
    updateActions();
}

void ClassCoTeacherPage::showAutomaticSaveFailureStatus()
{
    m_automaticSaveFailureStatusActive = true;
    if (!m_automaticSaveFailureStatusLabel)
    {
        return;
    }

    m_automaticSaveFailureStatusLabel->setText(
        tr(
            "Your co-teacher assignment could not be saved. "
            "Your changes are still here."
            )
        );
    m_automaticSaveFailureStatusLabel->show();
}

void ClassCoTeacherPage::clearAutomaticSaveFailureStatus()
{
    m_automaticSaveFailureStatusActive = false;
    if (!m_automaticSaveFailureStatusLabel)
    {
        return;
    }

    m_automaticSaveFailureStatusLabel->clear();
    m_automaticSaveFailureStatusLabel->hide();
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
    if (m_assignmentReadError && !showMessages)
    {
        return false;
    }

    if (m_classroom.id < 0)
    {
        return true;
    }

    ClassMngr::Next::Platform::
        ApplicationServicesClassCoTeacherAssignmentPort defaultAssignmentPort(
            m_services
            );
    const ClassMngr::Next::Application::
        ClassCoTeacherAssignmentPort& assignmentPort =
            m_assignmentPort ? *m_assignmentPort : defaultAssignmentPort;
    const auto typedClassId =
        ClassMngr::Next::Domain::ClassId::fromString(
            std::to_string(m_classroom.id)
            );
    const auto typedTeacherId =
        ClassMngr::Next::Domain::TeacherId::fromString(
            std::to_string(selectedTeacherIdFromUi())
            );
    Q_ASSERT(typedClassId && typedTeacherId);
    const ClassMngr::Next::Domain::Result<void> saved =
        ClassMngr::Next::Application::ClassCoTeacherAssignmentUseCase::execute(
            *typedClassId,
            *typedTeacherId,
            assignmentPort
            );
    if (!saved)
    {
        m_autosave->markDirty(false);

        if (showMessages)
        {
            DialogServices::showWarning(
                this,
                tr("Save Co-Teacher"),
                QString::fromUtf8(
                    saved.error().message.data(),
                    static_cast<qsizetype>(saved.error().message.size())
                    )
                );
        }
        else if (
            saved.error().code
                == ClassMngr::Next::Domain::ErrorCode::Technical
            )
        {
            showAutomaticSaveFailureStatus();
        }

        return false;
    }

    const auto previousSnapshot = m_readSnapshot;
    const qint64 committedTeacherId = selectedTeacherIdFromUi();
    const QString committedTeacherName = selectedTeacherDisplayNameFromUi();
    clearDirty();
    const bool requiredReadSucceeded = readSelectedClassSnapshot();
    if (!requiredReadSucceeded)
    {
        m_readSnapshot = previousSnapshot;
        if (m_readSnapshot && m_readSnapshot->classFields)
        {
            auto& selectedTeacherId = m_readSnapshot->classFields.value()
                .selectedTeacherId;
            if (committedTeacherId > 0)
            {
                selectedTeacherId =
                    ClassMngr::Next::Domain::TeacherId::fromString(
                        std::to_string(committedTeacherId)
                        );
            }
            else
            {
                selectedTeacherId.reset();
            }
        }

        clearAssignmentReadFailure();
        m_titleTeacherNameFallback = committedTeacherId > 0
            ? committedTeacherName
            : QString{};
    }
    else if (selectedUiTeacherIdFromSnapshot() > 0)
    {
        selectCachedTeacher();
        const bool hasTeacherDisplayName = m_readSnapshot
            && m_readSnapshot->teacherDisplayName
            && !QString::fromStdU16String(
                m_readSnapshot->teacherDisplayName.value()
                ).trimmed().isEmpty();
        m_titleTeacherNameFallback = hasTeacherDisplayName
            ? QString{}
            : selectedTeacherDisplayNameFromUi();
    }
    else
    {
        m_titleTeacherNameFallback = committedTeacherName;
    }
    updateTitle();
    emit classInfoSaved(m_classroom.id);
    return true;
}

bool ClassCoTeacherPage::readSelectedClassSnapshot()
{
    m_readSnapshot.reset();
    if (m_classroom.id <= 0)
    {
        clearAssignmentReadFailure();
        return true;
    }

    const auto classId = ClassMngr::Next::Domain::ClassId::fromString(
        std::to_string(m_classroom.id)
        );
    if (!classId)
    {
        setAssignmentReadFailure({
            .code = ClassMngr::Next::Domain::ErrorCode::InvalidInput,
            .message = "The selected class identifier is invalid.",
            .recoverable = false
        });
        return false;
    }

    ClassMngr::Next::Platform::
        ApplicationServicesClassCoTeacherPageReadPort defaultReadPort(
            m_services
            );
    const ClassMngr::Next::Application::ClassCoTeacherPageReadPort& readPort =
        m_readPort ? *m_readPort : defaultReadPort;
    const ClassMngr::Next::Application::ClassCoTeacherPageReadQuery query(
        readPort
        );
    const auto snapshot = query.execute(*classId);
    if (!snapshot)
    {
        setAssignmentReadFailure(snapshot.error());
        return false;
    }

    m_readSnapshot = snapshot.value();
    if (!m_readSnapshot->classFields)
    {
        setAssignmentReadFailure(m_readSnapshot->classFields.error());
        return false;
    }

    clearAssignmentReadFailure();
    return true;
}

void ClassCoTeacherPage::setAssignmentReadFailure(
    const ClassMngr::Next::Domain::OperationError& error
    )
{
    m_assignmentReadError = error;
    m_autosave->cancelPendingSave();
    m_autosave->setManualSaveRequired(true);
    updateAssignmentReadStatus();
}

void ClassCoTeacherPage::clearAssignmentReadFailure()
{
    m_assignmentReadError.reset();
    m_autosave->setManualSaveRequired(false);
    updateAssignmentReadStatus();
}

void ClassCoTeacherPage::updateAssignmentReadStatus()
{
    if (!m_assignmentReadStatusLabel)
    {
        return;
    }

    if (!m_assignmentReadError)
    {
        m_assignmentReadStatusLabel->clear();
        m_assignmentReadStatusLabel->hide();
        return;
    }

    const auto& error = *m_assignmentReadError;
    const QString errorDetails = QString::fromUtf8(
        error.message.data(),
        static_cast<qsizetype>(error.message.size())
        );
    m_assignmentReadStatusLabel->setText(
        tr(
            "The saved Co-Teacher assignment could not be confirmed, so "
            "the current selection may not match the saved value. Use the "
            "Save Changes button on this page to explicitly assign or "
            "unassign a teacher. Read error: code %1; recoverable: %2; "
            "details: %3"
            ).arg(
                operationErrorCodeName(error.code),
                error.recoverable
                    ? QStringLiteral("true")
                    : QStringLiteral("false"),
                errorDetails
                )
        );
    m_assignmentReadStatusLabel->show();
}

void ClassCoTeacherPage::selectCachedTeacher()
{
    m_teacherSection->selectTeacher(selectedUiTeacherIdFromSnapshot());
}

qint64 ClassCoTeacherPage::selectedTeacherIdFromSnapshot() const
{
    if (!m_readSnapshot || !m_readSnapshot->classFields)
    {
        return -1;
    }

    const auto& teacherId =
        m_readSnapshot->classFields.value().selectedTeacherId;
    if (!teacherId)
    {
        return -1;
    }

    return stableTeacherIdValue(*teacherId).value_or(-1);
}

int ClassCoTeacherPage::selectedUiTeacherIdFromSnapshot() const
{
    const qint64 stableTeacherId = selectedTeacherIdFromSnapshot();
    return stableTeacherId > 0
        ? m_uiIdsByStableTeacherId.value(stableTeacherId, -1)
        : -1;
}

qint64 ClassCoTeacherPage::selectedTeacherIdFromUi() const
{
    const int uiTeacherId = m_teacherSection->teacherId();
    if (uiTeacherId <= 0)
    {
        return -1;
    }
    return m_stableTeacherIdsByUiId.value(uiTeacherId, 0);
}

QString ClassCoTeacherPage::selectedTeacherDisplayNameFromUi() const
{
    if (!m_teacherSection)
    {
        return {};
    }

    const auto* englishSelector = m_teacherSection->findChild<QComboBox*>(
        QStringLiteral("teacherEnCombo")
        );
    QString displayName = englishSelector
        ? englishSelector->currentText().trimmed()
        : QString{};
    if (displayName.isEmpty())
    {
        displayName = m_teacherSection->teacherSelector()
            ? m_teacherSection->teacherSelector()->currentText().trimmed()
            : QString{};
    }
    if (displayName.isEmpty() && m_teacherSection->teacherId() > 0)
    {
        const qint64 stableTeacherId = m_stableTeacherIdsByUiId.value(
            m_teacherSection->teacherId(),
            m_teacherSection->teacherId()
            );
        displayName = tr("Teacher %1").arg(stableTeacherId);
    }
    return displayName;
}
