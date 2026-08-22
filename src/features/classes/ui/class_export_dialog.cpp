#include "class_export_dialog.h"
#include "ui/shared/widgets/text_fit_dialog_button_box.h"

#include "app/services/feature_services.h"
#include "core/utils/sidebar_node_naming.h"
#include "domain/models/class_info.h"
#include "domain/models/classroom.h"
#include "domain/models/teacher.h"
#include "ui/shared/dialogs/user_prompt_service.h"
#include "ui/shared/widgets/text_fit_push_button.h"

#include <QDialogButtonBox>
#include <QCollator>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>

#include <algorithm>
#include <utility>

namespace
{
QString classDisplayName(
    ClassService* classService,
    TeacherService* teacherService,
    const Classroom& classroom
    )
{
    const ClassInfo info = classService->classInfo(classroom.id).value_or(ClassInfo{});
    Teacher teacher;

    if (info.teacherId > 0)
    {
        teacher = teacherService->teacher(info.teacherId)
            .value_or(Teacher{});
    }

    const QString formatted =
        SidebarNodeNaming::formatClassDisplayName(info, teacher).trimmed();

    if (!formatted.isEmpty())
    {
        return formatted;
    }

    if (!classroom.name.trimmed().isEmpty())
    {
        return classroom.name.trimmed();
    }

    return QObject::tr("Class %1").arg(classroom.id);
}
}

ClassExportDialog::ClassExportDialog(
    ClassService* classService,
    TeacherService* teacherService,
    QWidget* parent
    )
    : DialogShell(QStringLiteral("classExport"), parent)
{
    setWindowTitle(tr("Export Classes"));
    setModal(true);
    resize(620, 480);

    auto* layout = contentLayout();
    auto* description = new QLabel(
        tr("Select the classes to include in the package."), this);
    description->setWordWrap(true);
    layout->addWidget(description);

    m_classList = new QListWidget(this);
    m_classList->setObjectName(QStringLiteral("classExportList"));
    layout->addWidget(m_classList, 1);

    if (classService && teacherService)
    {
        QList<QPair<QString, int>> classes;
        const Result<QList<Classroom>> loadedClasses =
            classService->classes();
        if (!loadedClasses)
        {
            DialogServices::showWarning(
                this,
                tr("Export Classes"),
                tr("Classes could not be loaded."),
                loadedClasses.error()
                );
        }

        for (const Classroom& classroom : loadedClasses.value_or(
                 QList<Classroom>{}))
        {
            classes.append({
                classDisplayName(classService, teacherService, classroom),
                classroom.id
            });
        }

        QCollator collator;
        collator.setCaseSensitivity(Qt::CaseInsensitive);
        collator.setNumericMode(true);

        std::sort(
            classes.begin(),
            classes.end(),
            [&collator](const auto& left, const auto& right)
            {
                const int comparison = collator.compare(
                    left.first, right.first);

                return comparison == 0
                    ? left.second < right.second
                    : comparison < 0;
            });

        for (const auto& [displayName, classId] : std::as_const(classes))
        {
            auto* item = new QListWidgetItem(displayName, m_classList);
            item->setData(Qt::UserRole, classId);
            item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
            item->setCheckState(Qt::Unchecked);
        }
    }

    auto* selectionLayout = new QHBoxLayout;
    auto* selectAllButton = new TextFitPushButton(tr("Select All"), this);
    auto* clearButton = new TextFitPushButton(tr("Clear"), this);
    selectionLayout->addWidget(selectAllButton);
    selectionLayout->addWidget(clearButton);
    selectionLayout->addStretch(1);
    layout->addLayout(selectionLayout);

    auto* buttons = addButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    m_exportButton = buttons->button(QDialogButtonBox::Ok);
    m_exportButton->setObjectName(QStringLiteral("exportClassesButton"));
    m_exportButton->setText(tr("Export"));

    connect(selectAllButton, &QPushButton::clicked, this, [this]()
    {
        setAllChecked(true);
    });
    connect(clearButton, &QPushButton::clicked, this, [this]()
    {
        setAllChecked(false);
    });
    connect(m_classList, &QListWidget::itemChanged,
            this, &ClassExportDialog::updateExportEnabled);
    updateExportEnabled();
}

QList<int> ClassExportDialog::selectedClassIds() const
{
    QList<int> result;

    for (int index = 0; index < m_classList->count(); ++index)
    {
        const QListWidgetItem* item = m_classList->item(index);

        if (item->checkState() == Qt::Checked)
        {
            result.append(item->data(Qt::UserRole).toInt());
        }
    }

    return result;
}

void ClassExportDialog::setAllChecked(
    bool checked
    )
{
    for (int index = 0; index < m_classList->count(); ++index)
    {
        m_classList->item(index)->setCheckState(
            checked ? Qt::Checked : Qt::Unchecked);
    }

    updateExportEnabled();
}

void ClassExportDialog::updateExportEnabled()
{
    if (m_exportButton)
    {
        m_exportButton->setEnabled(!selectedClassIds().isEmpty());
    }
}
