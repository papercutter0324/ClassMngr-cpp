#include "class_export_dialog.h"
#include "ui/shared/widgets/text_fit_dialog_button_box.h"

#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "core/utils/sidebar_node_naming.h"
#include "domain/models/class_info.h"
#include "domain/models/classroom.h"
#include "domain/models/teacher.h"
#include "next/application/classes_list_read_query.h"
#include "next/application/selected_class_subtitle_read_query.h"
#include "next/domain/domain_types.h"
#include "next/platform/application_services_classes_list_read_port.h"
#include "next/platform/application_services_selected_class_subtitle_read_port.h"
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
#include <charconv>
#include <string>
#include <system_error>
#include <utility>

namespace
{
QString classDisplayName(
    ApplicationServices* applicationServices,
    const Classroom& classroom
    )
{
    ClassInfo info;
    Teacher teacher;

    const auto selectedClassId =
        ClassMngr::Next::Domain::ClassId::fromString(
            std::to_string(classroom.id)
            );
    if (applicationServices && selectedClassId)
    {
        ClassMngr::Next::Platform::
            ApplicationServicesSelectedClassSubtitleReadPort readPort(
                applicationServices
                );
        const ClassMngr::Next::Application::SelectedClassSubtitleReadQuery query(
            readPort
            );
        const auto loadedSubtitle = query.execute(*selectedClassId);
        if (loadedSubtitle && loadedSubtitle.value().classFields)
        {
            const auto& subtitle = loadedSubtitle.value();
            const auto& fields = subtitle.classFields.value();
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

            if (subtitle.assignedTeacher && subtitle.assignedTeacher.value())
            {
                const auto& teacherFields =
                    subtitle.assignedTeacher.value().value();
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

QString classesListErrorMessage(const std::string& message)
{
    return QString::fromUtf8(
        message.data(),
        static_cast<qsizetype>(message.size())
        );
}
}

ClassExportDialog::ClassExportDialog(
    ApplicationServices* applicationServices,
    QWidget* parent
    )
    : DialogShell(QStringLiteral("classExport"), parent)
{
    auto* classService = applicationServices
        ? applicationServices->classService()
        : nullptr;
    auto* teacherService = applicationServices
        ? applicationServices->teacherService()
        : nullptr;

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
        ClassMngr::Next::Platform::
            ApplicationServicesClassesListReadPort readPort(
                applicationServices
                );
        const ClassMngr::Next::Application::ClassesListReadQuery query(
            readPort
            );
        const auto loadedClasses = query.execute();
        if (!loadedClasses)
        {
            DialogServices::showWarning(
                this,
                tr("Export Classes"),
                tr("Classes could not be loaded."),
                classesListErrorMessage(loadedClasses.error().message)
                );
        }
        else
        {
            for (const auto& entry : loadedClasses.value().classes)
            {
                int classId = 0;
                const std::string& classIdValue = entry.classId.value();
                const auto [end, conversionError] = std::from_chars(
                    classIdValue.data(),
                    classIdValue.data() + classIdValue.size(),
                    classId
                    );
                Q_ASSERT(
                    conversionError == std::errc{}
                    && end == classIdValue.data() + classIdValue.size()
                    && classId > 0
                    );
                if (conversionError != std::errc{}
                    || end != classIdValue.data() + classIdValue.size()
                    || classId <= 0)
                {
                    continue;
                }

                Classroom classroom;
                classroom.id = classId;
                classroom.name = QString::fromStdU16String(entry.className);
                classes.append({
                    classDisplayName(applicationServices, classroom),
                    classroom.id
                });
            }
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
