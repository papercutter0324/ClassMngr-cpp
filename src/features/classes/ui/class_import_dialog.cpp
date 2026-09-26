#include "class_import_dialog.h"
#include "ui/shared/widgets/text_fit_dialog_button_box.h"

#include "app/services/feature_services.h"
#include "core/result.h"
#include "core/utils/sidebar_node_naming.h"
#include "domain/models/classroom.h"
#include "next/application/class_transfer_projection.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QFrame>
#include <QLabel>
#include <QObject>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

#include <algorithm>
#include <optional>
#include <string>

namespace
{
constexpr int ActionRole = Qt::UserRole;
constexpr int TargetRole = Qt::UserRole + 1;

const ClassTransferTeacher* packageTeacher(
    const ClassTransferPackage& package,
    const QString& key
    )
{
    for (const ClassTransferTeacher& teacher : package.teachers)
    {
        if (teacher.key == key)
        {
            return &teacher;
        }
    }

    return nullptr;
}

QString packageClassDisplayName(
    const ClassTransferPackage& package,
    const ClassTransferClass& transferClass
    )
{
    Teacher teacher;
    const ClassTransferTeacher* transferTeacher =
        packageTeacher(package, transferClass.teacherKey);

    if (transferTeacher)
    {
        teacher = transferTeacher->teacher;
    }

    const QString display = SidebarNodeNaming::formatClassDisplayName(
        transferClass.info, teacher).trimmed();

    if (!display.isEmpty())
    {
        return display;
    }

    if (!transferClass.name.trimmed().isEmpty())
    {
        return transferClass.name.trimmed();
    }

    return transferClass.key;
}

QString destinationClassDisplayName(
    ClassService* classService,
    TeacherService* teacherService,
    int classId
    )
{
    const Classroom classroom = classService->classroom(classId)
        .value_or(Classroom{});
    const ClassInfo info = classService->classInfo(classId).value_or(ClassInfo{});
    Teacher teacher;

    if (info.teacherId > 0)
    {
        teacher = teacherService->teacher(info.teacherId)
            .value_or(Teacher{});
    }

    const QString display = SidebarNodeNaming::formatClassDisplayName(
        info, teacher).trimmed();

    if (!display.isEmpty())
    {
        return display;
    }

    if (!classroom.name.trimmed().isEmpty())
    {
        return classroom.name.trimmed();
    }

    return QObject::tr("Class %1").arg(classId);
}

QString destinationTeacherDisplayName(
    TeacherService* teacherService,
    int teacherId
    )
{
    const Teacher teacher = teacherService->teacher(teacherId)
        .value_or(Teacher{});
    const QString display =
        SidebarNodeNaming::formatTeacherDisplayName(teacher).trimmed();

    return display.isEmpty()
        ? QObject::tr("Teacher %1").arg(teacherId)
        : display;
}

void addChoice(
    QComboBox* combo,
    const QString& label,
    int action,
    int targetId = -1
    )
{
    combo->addItem(label);
    const int index = combo->count() - 1;
    combo->setItemData(index, action, ActionRole);
    combo->setItemData(index, targetId, TargetRole);
}

QFrame* separator(
    QWidget* parent
    )
{
    auto* line = new QFrame(parent);
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Sunken);
    return line;
}

ClassMngr::Next::Application::ClassTransferReviewClassAction reviewAction(
    const ClassImportAction action
    )
{
    using ReviewAction =
        ClassMngr::Next::Application::ClassTransferReviewClassAction;
    switch (action)
    {
    case ClassImportAction::Create:
        return ReviewAction::Create;
    case ClassImportAction::Replace:
        return ReviewAction::Replace;
    case ClassImportAction::Skip:
        return ReviewAction::Skip;
    }
    return ReviewAction::Invalid;
}

ClassMngr::Next::Application::ClassTransferReviewTeacherAction reviewAction(
    const TeacherImportAction action
    )
{
    using ReviewAction =
        ClassMngr::Next::Application::ClassTransferReviewTeacherAction;
    switch (action)
    {
    case TeacherImportAction::Create:
        return ReviewAction::Create;
    case TeacherImportAction::KeepExisting:
        return ReviewAction::KeepExisting;
    case TeacherImportAction::ReplaceExisting:
        return ReviewAction::ReplaceExisting;
    }
    return ReviewAction::Invalid;
}

template <typename TypedId>
Result<TypedId> typedReviewId(
    const int legacyId,
    const QString& description
    )
{
    if (legacyId <= 0)
    {
        return std::unexpected(
            QObject::tr("The %1 contains an invalid destination ID.")
                .arg(description));
    }
    return *TypedId::fromString(std::to_string(legacyId));
}

QString reviewIssueMessage(
    const ClassMngr::Next::Application::ClassTransferReviewDecisionIssueCode code
    );

template <typename TypedId>
Result<std::optional<TypedId>> typedReviewTarget(
    const int legacyId,
    const QString& description,
    const ClassMngr::Next::Application::ClassTransferReviewClassAction action
    )
{
    if (legacyId == -1)
    {
        return std::optional<TypedId>{};
    }
    using Action =
        ClassMngr::Next::Application::ClassTransferReviewClassAction;
    using IssueCode =
        ClassMngr::Next::Application::ClassTransferReviewDecisionIssueCode;
    if (legacyId <= 0)
    {
        if (action == Action::Invalid || action == Action::Unselected)
        {
            return std::optional<TypedId>{};
        }
        return std::unexpected(reviewIssueMessage(
            action == Action::Replace
                ? IssueCode::ReplaceClassMissingTarget
                : IssueCode::NonReplaceClassHasTarget));
    }
    const Result<TypedId> typedId = typedReviewId<TypedId>(
        legacyId, description);
    if (!typedId)
    {
        return std::unexpected(typedId.error());
    }
    return std::optional<TypedId>{*typedId};
}

template <typename TypedId>
Result<std::optional<TypedId>> typedReviewTarget(
    const int legacyId,
    const QString& description,
    const ClassMngr::Next::Application::ClassTransferReviewTeacherAction action
    )
{
    if (legacyId == -1)
    {
        return std::optional<TypedId>{};
    }
    using Action =
        ClassMngr::Next::Application::ClassTransferReviewTeacherAction;
    using IssueCode =
        ClassMngr::Next::Application::ClassTransferReviewDecisionIssueCode;
    if (legacyId <= 0)
    {
        if (action == Action::Invalid || action == Action::Unselected)
        {
            return std::optional<TypedId>{};
        }
        return std::unexpected(reviewIssueMessage(
            action == Action::Create
                ? IssueCode::CreateTeacherHasTarget
                : IssueCode::TeacherActionMissingTarget));
    }
    const Result<TypedId> typedId = typedReviewId<TypedId>(
        legacyId, description);
    if (!typedId)
    {
        return std::unexpected(typedId.error());
    }
    return std::optional<TypedId>{*typedId};
}

Result<ClassMngr::Next::Application::ClassTransferReviewDecisionRequest>
reviewDecisionRequest(
    const ClassTransferPackage& package,
    const ClassImportPreview& preview,
    const ClassImportPlan& plan
    )
{
    using namespace ClassMngr::Next::Application;
    using ClassId = ClassMngr::Next::Domain::ClassId;
    using TeacherId = ClassMngr::Next::Domain::TeacherId;
    ClassTransferReviewDecisionRequest request;

    for (int index = 0; index < package.classes.size(); ++index)
    {
        ClassTransferReviewClassCandidate candidate;
        candidate.packageClassIndex = index;
        const auto previewEntry = std::find_if(
            preview.classes.cbegin(),
            preview.classes.cend(),
            [index](const ClassImportClassPreview& item)
            {
                return item.packageClassIndex == index;
            }
            );
        if (previewEntry != preview.classes.cend())
        {
            candidate.matchingClassIds.reserve(
                static_cast<std::size_t>(previewEntry->matchingClassIds.size())
            );
            for (const int classId : previewEntry->matchingClassIds)
            {
                const Result<ClassId> typedId = typedReviewId<ClassId>(
                    classId, QObject::tr("class import preview"));
                if (!typedId)
                {
                    return std::unexpected(typedId.error());
                }
                candidate.matchingClassIds.push_back(*typedId);
            }
        }
        request.classes.push_back(std::move(candidate));
    }

    for (const ClassTransferTeacher& transferTeacher : package.teachers)
    {
        ClassTransferReviewTeacherCandidate candidate;
        candidate.teacherKey = transferTeacher.key.toStdString();
        const auto previewEntry = std::find_if(
            preview.teachers.cbegin(),
            preview.teachers.cend(),
            [&transferTeacher](const ClassImportTeacherPreview& item)
            {
                return item.teacherKey == transferTeacher.key;
            }
            );
        if (previewEntry != preview.teachers.cend())
        {
            candidate.matchingTeacherIds.reserve(
                static_cast<std::size_t>(previewEntry->matchingTeacherIds.size())
            );
            for (const int teacherId : previewEntry->matchingTeacherIds)
            {
                const Result<TeacherId> typedId = typedReviewId<TeacherId>(
                    teacherId, QObject::tr("teacher import preview"));
                if (!typedId)
                {
                    return std::unexpected(typedId.error());
                }
                candidate.matchingTeacherIds.push_back(*typedId);
            }
        }
        request.teachers.push_back(std::move(candidate));
    }

    request.classResolutions.reserve(
        static_cast<std::size_t>(plan.classes.size())
        );
    for (const ClassImportResolution& resolution : plan.classes)
    {
        const auto action = reviewAction(resolution.action);
        const Result<std::optional<ClassId>> targetId =
            typedReviewTarget<ClassId>(
                resolution.targetClassId,
                QObject::tr("class import plan"),
                action);
        if (!targetId)
        {
            return std::unexpected(targetId.error());
        }
        request.classResolutions.push_back({
            resolution.packageClassIndex,
            action,
            *targetId
        });
    }

    request.teacherResolutions.reserve(
        static_cast<std::size_t>(plan.teachers.size())
        );
    for (const TeacherImportResolution& resolution : plan.teachers)
    {
        const auto action = reviewAction(resolution.action);
        const Result<std::optional<TeacherId>> targetId =
            typedReviewTarget<TeacherId>(
                resolution.targetTeacherId,
                QObject::tr("teacher import plan"),
                action);
        if (!targetId)
        {
            return std::unexpected(targetId.error());
        }
        request.teacherResolutions.push_back({
            resolution.teacherKey.toStdString(),
            action,
            *targetId
        });
    }

    return request;
}

QString reviewIssueMessage(
    const ClassMngr::Next::Application::ClassTransferReviewDecisionIssueCode code
    )
{
    using IssueCode =
        ClassMngr::Next::Application::ClassTransferReviewDecisionIssueCode;
    switch (code)
    {
    case IssueCode::DuplicateClassReplacementTarget:
        return QObject::tr(
            "Two package classes cannot replace the same destination class.");
    case IssueCode::DuplicateTeacherReplacementTarget:
        return QObject::tr(
            "Two package teachers cannot replace the same local teacher.");
    case IssueCode::InvalidTeacherAction:
    case IssueCode::MissingTeacherResolution:
        return QObject::tr(
            "Choose a resolution for every ambiguous teacher match.");
    case IssueCode::UniqueTeacherCannotBeCreated:
    case IssueCode::CreateTeacherHasTarget:
        return QObject::tr(
            "An unambiguous teacher match must reuse the local teacher.");
    case IssueCode::ReplaceClassMissingTarget:
    case IssueCode::ClassTargetNotInMatchSet:
        return QObject::tr(
            "A replacement class is not one of the inferred matches.");
    case IssueCode::NonReplaceClassHasTarget:
        return QObject::tr(
            "Only replacement actions may specify a destination class.");
    case IssueCode::TeacherActionMissingTarget:
    case IssueCode::TeacherTargetNotInMatchSet:
        return QObject::tr(
            "A selected teacher is not one of the inferred matches.");
    case IssueCode::MissingClassResolution:
        return QObject::tr("Every package class must have an import action.");
    case IssueCode::InvalidClassAction:
    case IssueCode::InvalidClassIndex:
    case IssueCode::DuplicateClassResolution:
    case IssueCode::UnknownClassResolution:
        return QObject::tr(
            "The class import plan contains an invalid or duplicate class entry.");
    case IssueCode::EmptyTeacherKey:
    case IssueCode::DuplicateTeacherResolution:
    case IssueCode::UnknownTeacherResolution:
        return QObject::tr(
            "The teacher import plan contains an invalid or duplicate teacher entry.");
    }
    return QObject::tr("The class import choices are invalid.");
}
}

ClassImportDialog::ClassImportDialog(
    ClassService* classService,
    TeacherService* teacherService,
    const ClassTransferPackage& package,
    const ClassImportPreview& preview,
    QWidget* parent
    )
    : DialogShell(QStringLiteral("classImport"), parent)
    , m_package(package)
    , m_preview(preview)
{
    setWindowTitle(tr("Import Classes"));
    setModal(true);
    resize(820, 640);

    auto* mainLayout = contentLayout();
    auto* description = new QLabel(
        tr("Review how package classes and teachers will be imported."), this);
    description->setWordWrap(true);
    mainLayout->addWidget(description);

    auto* scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    auto* content = new QWidget(scrollArea);
    auto* contentLayout = new QVBoxLayout(content);

    auto* classesHeading = new QLabel(tr("Classes"), content);
    classesHeading->setObjectName(QStringLiteral("sectionHeading"));
    contentLayout->addWidget(classesHeading);

    auto* classForm = new QFormLayout;
    classForm->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);

    for (const ClassImportClassPreview& classPreview : preview.classes)
    {
        if (classPreview.packageClassIndex < 0
            || classPreview.packageClassIndex >= package.classes.size())
        {
            continue;
        }

        const ClassTransferClass& transferClass =
            package.classes[classPreview.packageClassIndex];
        auto* combo = new QComboBox(content);
        combo->setObjectName(
            QStringLiteral("classImportChoice_%1")
                .arg(classPreview.packageClassIndex));
        addChoice(
            combo,
            classPreview.matchingClassIds.isEmpty()
                ? tr("Create new class")
                : tr("Create another class"),
            static_cast<int>(ClassImportAction::Create)
            );

        for (int classId : classPreview.matchingClassIds)
        {
            addChoice(
                combo,
                tr("Replace: %1").arg(
                    destinationClassDisplayName(
                        classService, teacherService, classId)),
                static_cast<int>(ClassImportAction::Replace),
                classId
                );
        }

        addChoice(
            combo,
            tr("Skip"),
            static_cast<int>(ClassImportAction::Skip)
            );
        classForm->addRow(
            packageClassDisplayName(package, transferClass), combo);
        m_classRows.append({classPreview.packageClassIndex, combo});
        connect(combo, &QComboBox::currentIndexChanged,
                this, &ClassImportDialog::updateImportEnabled);
    }

    contentLayout->addLayout(classForm);
    contentLayout->addWidget(separator(content));

    auto* teachersHeading = new QLabel(tr("Assigned Teachers"), content);
    teachersHeading->setObjectName(QStringLiteral("sectionHeading"));
    contentLayout->addWidget(teachersHeading);

    auto* teacherForm = new QFormLayout;
    teacherForm->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);

    for (const ClassImportTeacherPreview& teacherPreview : preview.teachers)
    {
        const ClassTransferTeacher* transferTeacher =
            packageTeacher(package, teacherPreview.teacherKey);

        if (!transferTeacher)
        {
            continue;
        }

        auto* combo = new QComboBox(content);
        combo->setObjectName(
            QStringLiteral("teacherImportChoice_%1")
                .arg(teacherPreview.teacherKey));

        if (teacherPreview.matchingTeacherIds.isEmpty())
        {
            addChoice(
                combo,
                tr("Create new teacher"),
                static_cast<int>(TeacherImportAction::Create)
                );
        }
        else if (teacherPreview.matchingTeacherIds.size() == 1)
        {
            const int teacherId = teacherPreview.matchingTeacherIds.first();
            const QString localName = destinationTeacherDisplayName(
                teacherService, teacherId);
            addChoice(
                combo,
                tr("Keep local: %1").arg(localName),
                static_cast<int>(TeacherImportAction::KeepExisting),
                teacherId
                );
            addChoice(
                combo,
                tr("Replace local: %1").arg(localName),
                static_cast<int>(TeacherImportAction::ReplaceExisting),
                teacherId
                );
        }
        else
        {
            addChoice(combo, tr("Choose a teacher resolution…"), -1);
            addChoice(
                combo,
                tr("Create new teacher"),
                static_cast<int>(TeacherImportAction::Create)
                );

            for (int teacherId : teacherPreview.matchingTeacherIds)
            {
                const QString localName = destinationTeacherDisplayName(
                    teacherService, teacherId);
                addChoice(
                    combo,
                    tr("Keep local: %1").arg(localName),
                    static_cast<int>(TeacherImportAction::KeepExisting),
                    teacherId
                    );
                addChoice(
                    combo,
                    tr("Replace local: %1").arg(localName),
                    static_cast<int>(TeacherImportAction::ReplaceExisting),
                    teacherId
                    );
            }
        }

        const QString teacherName =
            SidebarNodeNaming::formatTeacherDisplayName(
                transferTeacher->teacher).trimmed();
        teacherForm->addRow(
            teacherName.isEmpty() ? teacherPreview.teacherKey : teacherName,
            combo
            );
        m_teacherRows.append({teacherPreview.teacherKey, combo});
        connect(combo, &QComboBox::currentIndexChanged,
                this, &ClassImportDialog::updateImportEnabled);
    }

    if (m_teacherRows.isEmpty())
    {
        teacherForm->addRow(new QLabel(tr("No assigned teachers"), content));
    }

    contentLayout->addLayout(teacherForm);
    contentLayout->addStretch(1);
    scrollArea->setWidget(content);
    mainLayout->addWidget(scrollArea, 1);

    m_validationLabel = new QLabel(this);
    m_validationLabel->setObjectName(QStringLiteral("importValidationLabel"));
    m_validationLabel->setWordWrap(true);
    mainLayout->addWidget(m_validationLabel);

    auto* buttons = addButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    m_importButton = buttons->button(QDialogButtonBox::Ok);
    m_importButton->setObjectName(QStringLiteral("importClassesButton"));
    m_importButton->setText(tr("Import"));
    updateImportEnabled();
}

ClassImportPlan ClassImportDialog::importPlan() const
{
    ClassImportPlan plan;

    for (const ClassRow& row : m_classRows)
    {
        const int choiceIndex = row.choice->currentIndex();
        plan.classes.append({
            row.packageClassIndex,
            static_cast<ClassImportAction>(
                row.choice->itemData(choiceIndex, ActionRole).toInt()),
            row.choice->itemData(choiceIndex, TargetRole).toInt()
        });
    }

    for (const TeacherRow& row : m_teacherRows)
    {
        const int choiceIndex = row.choice->currentIndex();
        plan.teachers.append({
            row.teacherKey,
            static_cast<TeacherImportAction>(
                row.choice->itemData(choiceIndex, ActionRole).toInt()),
            row.choice->itemData(choiceIndex, TargetRole).toInt()
        });
    }

    return plan;
}

void ClassImportDialog::updateImportEnabled()
{
    const auto request =
        reviewDecisionRequest(m_package, m_preview, importPlan());
    QString validationMessage;
    if (!request)
    {
        validationMessage = request.error();
    }
    else
    {
        const auto decision =
            ClassMngr::Next::Application::validateClassTransferReviewDecisions(
                *request);
        validationMessage = decision.accepted()
            ? QString()
            : reviewIssueMessage(decision.issues.front().code);
    }

    if (m_validationLabel)
    {
        m_validationLabel->setText(validationMessage);
        m_validationLabel->setVisible(!validationMessage.isEmpty());
    }

    if (m_importButton)
    {
        m_importButton->setEnabled(validationMessage.isEmpty());
    }
}
