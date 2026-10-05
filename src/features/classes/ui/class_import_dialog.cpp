#include "class_import_dialog.h"
#include "next/platform/class_transfer_apply_legacy_adapter.h"
#include "ui/shared/widgets/text_fit_dialog_button_box.h"

#include "core/application_services.h"
#include "core/result.h"
#include "core/utils/sidebar_node_naming.h"
#include "next/application/class_transfer_projection.h"
#include "next/application/selected_class_subtitle_batch_read_query.h"
#include "next/application/teacher_display_name_batch_read_query.h"
#include "next/application/teacher_profile_read_query.h"
#include "next/domain/domain_types.h"
#include "next/platform/application_services_selected_class_subtitle_batch_read_port.h"
#include "next/platform/application_services_teacher_display_name_batch_read_port.h"
#include "next/platform/application_services_teacher_profile_read_port.h"

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
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

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
    const int classId,
    const ClassMngr::Next::Application::SelectedClassSubtitleReadSnapshot*
        subtitle
    )
{
    ClassInfo info;
    Teacher teacher;

    if (subtitle && subtitle->classFields)
    {
        const auto& fields = subtitle->classFields.value();
        info.classGrade = QString::fromStdU16String(fields.classGrade);
        info.classLevel = QString::fromStdU16String(fields.classLevel);
        info.classTimes.reserve(
            static_cast<qsizetype>(fields.regularSchedule.size())
            );
        for (const auto& row : fields.regularSchedule)
        {
            ClassTime time;
            time.day = QString::fromStdU16String(row.day);
            time.startTime = QString::fromStdU16String(row.startTime);
            time.endTime.clear();
            info.classTimes.append(std::move(time));
        }

        if (subtitle->assignedTeacher && subtitle->assignedTeacher.value())
        {
            const auto& teacherFields = subtitle->assignedTeacher.value().value();
            teacher.teacherKr = QString::fromStdU16String(
                teacherFields.teacherKr
                );
            teacher.teacherEn = QString::fromStdU16String(
                teacherFields.teacherEn
                );
            teacher.preferredRomanization = QString::fromStdU16String(
                teacherFields.preferredRomanization
                );
            teacher.preferredName = QString::fromStdU16String(
                teacherFields.preferredName
                );
        }
    }

    const QString display = SidebarNodeNaming::formatClassDisplayName(
        info, teacher).trimmed();

    if (!display.isEmpty())
    {
        return display;
    }

    return QObject::tr("Class %1").arg(classId);
}

QString destinationTeacherDisplayName(
    ApplicationServices* applicationServices,
    int teacherId
    )
{
    Teacher teacher;
    if (applicationServices && teacherId > 0)
    {
        const auto typedTeacherId =
            ClassMngr::Next::Domain::TeacherId::fromString(
                std::to_string(teacherId)
                );
        if (typedTeacherId)
        {
            ClassMngr::Next::Platform::
                ApplicationServicesTeacherProfileReadPort readPort(
                    applicationServices
                    );
            const ClassMngr::Next::Application::TeacherProfileReadQuery query(
                readPort
                );
            const auto loadedProfile = query.execute(*typedTeacherId);
            if (loadedProfile)
            {
                const auto& fields = loadedProfile.value().fields;
                teacher.teacherKr = QString::fromStdU16String(fields.teacherKr);
                teacher.teacherEn = QString::fromStdU16String(fields.teacherEn);
                teacher.preferredRomanization = QString::fromStdU16String(
                    fields.preferredRomanization
                    );
                teacher.preferredName = QString::fromStdU16String(
                    fields.preferredName
                    );
            }
        }
    }

    const QString display =
        SidebarNodeNaming::formatTeacherDisplayName(teacher).trimmed();

    return display.isEmpty()
        ? QObject::tr("Teacher %1").arg(teacherId)
        : display;
}

QString destinationTeacherDisplayName(
    const ClassMngr::Next::Application::TeacherDisplayNameFields* fields,
    const int teacherId
    )
{
    Teacher teacher;
    if (fields)
    {
        teacher.teacherKr = QString::fromStdU16String(fields->teacherKr);
        teacher.teacherEn = QString::fromStdU16String(fields->teacherEn);
        teacher.preferredRomanization = QString::fromStdU16String(
            fields->preferredRomanization
            );
        teacher.preferredName = QString::fromStdU16String(
            fields->preferredName
            );
    }

    const QString display =
        SidebarNodeNaming::formatTeacherDisplayName(teacher).trimmed();

    return display.isEmpty()
        ? QObject::tr("Teacher %1").arg(teacherId)
        : display;
}

using DestinationTeacherDisplayNames = std::unordered_map<int, QString>;

DestinationTeacherDisplayNames destinationTeacherDisplayNames(
    ApplicationServices* applicationServices,
    const ClassTransferPackage& package,
    const ClassImportPreview& preview
    )
{
    std::vector<ClassMngr::Next::Domain::TeacherId> teacherIds;
    std::unordered_set<int> seenTeacherIds;
    for (const ClassImportTeacherPreview& teacherPreview : preview.teachers)
    {
        if (!packageTeacher(package, teacherPreview.teacherKey))
        {
            continue;
        }

        for (const int teacherId : teacherPreview.matchingTeacherIds)
        {
            if (teacherId > 0 && seenTeacherIds.insert(teacherId).second)
            {
                teacherIds.push_back(*
                    ClassMngr::Next::Domain::TeacherId::fromString(
                        std::to_string(teacherId)));
            }
        }
    }

    DestinationTeacherDisplayNames displayNames;
    if (teacherIds.empty())
    {
        return displayNames;
    }

    std::optional<std::vector<
        ClassMngr::Next::Application::TeacherDisplayNameBatchReadSnapshot
        >> batchRecords;
    try
    {
        ClassMngr::Next::Platform::
            ApplicationServicesTeacherDisplayNameBatchReadPort readPort(
                applicationServices
                );
        const ClassMngr::Next::Application::
            TeacherDisplayNameBatchReadQuery query(readPort);
        const auto loaded = query.execute(teacherIds);
        if (loaded)
        {
            batchRecords = loaded.value();
        }
    }
    catch (...)
    {
        // Preserve the per-teacher fallback if the batch boundary throws.
    }

    if (batchRecords)
    {
        displayNames.reserve(
            static_cast<std::size_t>(batchRecords->size())
            );
        for (const auto& record : *batchRecords)
        {
            const int teacherId = std::stoi(record.teacherId.value());
            displayNames.emplace(
                teacherId,
                destinationTeacherDisplayName(&record.fields, teacherId)
                );
        }
        return displayNames;
    }

    // A failed batch must not hide readable siblings. Retry each requested
    // teacher through the existing single-profile path independently.
    displayNames.reserve(static_cast<std::size_t>(teacherIds.size()));
    for (const auto& teacherId : teacherIds)
    {
        const int legacyTeacherId = std::stoi(teacherId.value());
        try
        {
            displayNames.emplace(
                legacyTeacherId,
                destinationTeacherDisplayName(
                    applicationServices,
                    legacyTeacherId
                    )
                );
        }
        catch (...)
        {
            displayNames.emplace(
                legacyTeacherId,
                destinationTeacherDisplayName(
                    static_cast<ApplicationServices*>(nullptr),
                    legacyTeacherId
                    )
                );
        }
    }
    return displayNames;
}

QString destinationTeacherDisplayName(
    const DestinationTeacherDisplayNames& displayNames,
    const int teacherId
    )
{
    const auto displayName = displayNames.find(teacherId);
    if (displayName != displayNames.end())
    {
        return displayName->second;
    }

    return destinationTeacherDisplayName(
        static_cast<
            const ClassMngr::Next::Application::TeacherDisplayNameFields*>(
                nullptr),
        teacherId
        );
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
    ApplicationServices* applicationServices,
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

    std::vector<ClassMngr::Next::Domain::ClassId> destinationClassIds;
    std::unordered_set<std::string> seenDestinationClassIds;
    for (const ClassImportClassPreview& classPreview : preview.classes)
    {
        if (classPreview.packageClassIndex < 0
            || classPreview.packageClassIndex >= package.classes.size())
        {
            continue;
        }

        for (const int classId : classPreview.matchingClassIds)
        {
            if (classId <= 0)
            {
                continue;
            }

            const std::string value = std::to_string(classId);
            const auto typedId =
                ClassMngr::Next::Domain::ClassId::fromString(value);
            if (typedId
                && seenDestinationClassIds.insert(value).second)
            {
                destinationClassIds.push_back(*typedId);
            }
        }
    }

    std::unordered_map<
        std::string,
        ClassMngr::Next::Application::SelectedClassSubtitleReadSnapshot
        > destinationClassSubtitles;
    ClassMngr::Next::Platform::
        ApplicationServicesSelectedClassSubtitleBatchReadPort readPort(
            applicationServices
            );
    const ClassMngr::Next::Application::SelectedClassSubtitleBatchReadQuery
        query(readPort);
    const auto loadedSubtitles = query.execute(destinationClassIds);
    if (loadedSubtitles)
    {
        for (auto& subtitle : loadedSubtitles.value())
        {
            const std::string classId = subtitle.classId.value();
            destinationClassSubtitles.emplace(
                classId,
                std::move(subtitle)
                );
        }
    }

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
            const auto subtitle =
                destinationClassSubtitles.find(std::to_string(classId));
            addChoice(
                combo,
                tr("Replace: %1").arg(
                    destinationClassDisplayName(
                        classId,
                        subtitle == destinationClassSubtitles.end()
                            ? nullptr
                            : &subtitle->second)),
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
    const DestinationTeacherDisplayNames destinationTeacherNames =
        destinationTeacherDisplayNames(
            applicationServices,
            package,
            preview
            );

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
                destinationTeacherNames,
                teacherId
                );
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
                    destinationTeacherNames,
                    teacherId
                    );
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

ClassMngr::Next::Application::ClassTransferApplyRequest ClassImportDialog::applyRequest() const
{
    return ClassMngr::Next::Platform::classTransferApplyRequest(importPlan());
}

void ClassImportDialog::updateImportEnabled()
{
    const auto candidates = ClassMngr::Next::Platform::classTransferApplyCandidates(
        m_package, m_preview);
    QString validationMessage;
    if (!candidates)
    {
        validationMessage = candidates.error();
    }
    else
    {
        const auto decision =
            ClassMngr::Next::Application::validateClassTransferApplyRequest(
                applyRequest(), *candidates);
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
