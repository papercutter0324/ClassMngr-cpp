#include "class_import_dialog.h"
#include "ui/shared/widgets/text_fit_dialog_button_box.h"

#include "app/services/feature_services.h"
#include "core/utils/sidebar_node_naming.h"
#include "domain/models/classroom.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QFrame>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QSet>
#include <QVBoxLayout>

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
    const ClassInfo info = classService->classInfo(classId);
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
    QString validationMessage;
    QSet<int> replacedClasses;
    QSet<int> replacedTeachers;

    for (const ClassRow& row : m_classRows)
    {
        const int index = row.choice->currentIndex();
        const int action = row.choice->itemData(index, ActionRole).toInt();

        if (action == static_cast<int>(ClassImportAction::Replace))
        {
            const int target = row.choice->itemData(index, TargetRole).toInt();

            if (replacedClasses.contains(target))
            {
                validationMessage = tr(
                    "Two package classes cannot replace the same destination class.");
                break;
            }

            replacedClasses.insert(target);
        }
    }

    if (validationMessage.isEmpty())
    {
        for (const TeacherRow& row : m_teacherRows)
        {
            const int index = row.choice->currentIndex();
            const int action = row.choice->itemData(index, ActionRole).toInt();

            if (action < 0)
            {
                validationMessage = tr(
                    "Choose a resolution for every ambiguous teacher match.");
                break;
            }

            if (action == static_cast<int>(
                    TeacherImportAction::ReplaceExisting))
            {
                const int target =
                    row.choice->itemData(index, TargetRole).toInt();

                if (replacedTeachers.contains(target))
                {
                    validationMessage = tr(
                        "Two package teachers cannot replace the same local teacher.");
                    break;
                }

                replacedTeachers.insert(target);
            }
        }
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
