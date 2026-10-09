#include "my_classes_page.h"

#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "core/fontmanager.h"
#include "core/utils/sidebar_node_naming.h"
#include "domain/models/class_info.h"
#include "domain/models/classroom.h"
#include "domain/models/teacher.h"
#include "features/classes/models/class_tab_navigation_model.h"
#include "next/application/classes_list_read_query.h"
#include "next/application/my_classes_class_information_batch_read_query.h"
#include "next/application/my_classes_student_count_batch_read_query.h"
#include "next/application/my_classes_teacher_profile_batch_read_query.h"
#include "next/platform/application_services_classes_list_read_port.h"
#include "next/platform/application_services_my_classes_class_information_batch_read_port.h"
#include "next/platform/application_services_my_classes_student_count_batch_read_port.h"
#include "next/platform/application_services_my_classes_teacher_profile_batch_read_port.h"
#include "ui/shared/constants/gui_constants.h"
#include "ui/shared/styles/roles.h"
#include "ui/shared/utils/widget_sizing.h"
#include "ui/shared/widgets/sectioncards/class_info_section_card.h"
#include "ui/shared/widgets/navigation_tab_widget.h"
#include "ui/shared/dialogs/user_prompt_service.h"

#include <algorithm>
#include <charconv>
#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <system_error>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include <QFrame>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QLayout>
#include <QSizePolicy>
#include <QTextEdit>
#include <QVBoxLayout>

namespace
{
constexpr int CompactFieldWidth = 170;
constexpr int ClassTabContentTopMargin = 16;
const QString NotAvailableText =
    QStringLiteral("N/A");

QList<Classroom> classroomsFromListSnapshot(
    const ClassMngr::Next::Application::ClassesListSnapshot& snapshot
    )
{
    QList<Classroom> classrooms;
    classrooms.reserve(static_cast<qsizetype>(snapshot.classes.size()));
    for (const auto& entry : snapshot.classes)
    {
        int classId = 0;
        const std::string& classIdValue = entry.classId.value();
        const auto [end, error] = std::from_chars(
            classIdValue.data(),
            classIdValue.data() + classIdValue.size(),
            classId
            );
        Q_ASSERT(
            error == std::errc{}
            && end == classIdValue.data() + classIdValue.size()
            && classId > 0
            );
        if (error != std::errc{}
            || end != classIdValue.data() + classIdValue.size()
            || classId <= 0)
        {
            continue;
        }

        Classroom classroom;
        classroom.id = classId;
        classroom.name = QString::fromStdU16String(entry.className);
        classrooms.append(std::move(classroom));
    }
    return classrooms;
}

QString classesListErrorMessage(const std::string& message)
{
    return QString::fromUtf8(
        message.data(),
        static_cast<qsizetype>(message.size())
        );
}

struct ClassSummary
{
    Classroom classroom;
    ClassInfo info;
    std::optional<ClassMngr::Next::Domain::TeacherId> teacherId;
    Teacher teacher;
    bool teacherProfileLoaded = false;
    QString displayName;
    int studentCount = 0;
};

ClassInfo classInfoFromMyClassesSnapshot(
    const ClassMngr::Next::Application::MyClassesClassInformationFields& fields,
    const int classId
    )
{
    ClassInfo info;
    info.classId = classId;
    info.classGrade = QString::fromStdU16String(fields.classGrade);
    info.classLevel = QString::fromStdU16String(fields.classLevel);
    info.notes = QString::fromStdU16String(fields.notes);
    info.timeFillerActivities = QString::fromStdU16String(
        fields.timeFillerActivities
        );

    for (const auto& schedule : fields.regularSchedule)
    {
        info.classTimes.append({
            .day = QString::fromStdU16String(schedule.day),
            .startTime = QString::fromStdU16String(schedule.startTime),
            .endTime = QString::fromStdU16String(schedule.endTime)
        });
    }
    for (const auto& schedule : fields.intensiveSchedule)
    {
        info.intensiveTimes.append({
            .day = QString::fromStdU16String(schedule.day),
            .startTime = QString::fromStdU16String(schedule.startTime),
            .endTime = QString::fromStdU16String(schedule.endTime)
        });
    }

    return info;
}

Teacher teacherFromMyClassesProfile(
    const ClassMngr::Next::Application::MyClassesTeacherProfileFields& fields
    )
{
    Teacher teacher;
    teacher.teacherKr = QString::fromStdU16String(fields.teacherKr);
    teacher.teacherEn = QString::fromStdU16String(fields.teacherEn);
    teacher.preferredRomanization = QString::fromStdU16String(
        fields.preferredRomanization
        );
    teacher.preferredName = QString::fromStdU16String(fields.preferredName);
    teacher.roomNumber = QString::fromStdU16String(fields.roomNumber);
    teacher.internetType = QString::fromStdU16String(fields.internetType);
    teacher.wifiName = QString::fromStdU16String(fields.wifiName);
    teacher.wifiPassword = QString::fromStdU16String(fields.wifiPassword);
    teacher.projectionType = QString::fromStdU16String(fields.projectionType);
    teacher.zoomId = QString::fromStdU16String(fields.zoomId);
    teacher.zoomPassword = QString::fromStdU16String(fields.zoomPassword);
    teacher.notes = QString::fromStdU16String(fields.notes);
    return teacher;
}

QString valueOrNa(
    const QString& value
    )
{
    const QString trimmed =
        value.trimmed();

    return trimmed.isEmpty()
        ? NotAvailableText
        : trimmed;
}

QString gradeLevelText(
    const ClassInfo& info
    )
{
    const QString grade =
        info.classGrade.trimmed();
    const QString level =
        info.classLevel.trimmed();

    if (!grade.isEmpty() && !level.isEmpty())
    {
        return QStringLiteral("%1 - %2")
            .arg(grade, level);
    }

    if (!grade.isEmpty())
    {
        return grade;
    }

    if (!level.isEmpty())
    {
        return level;
    }

    return NotAvailableText;
}

QString classTitleText(
    const Classroom& classroom,
    const ClassInfo& info
    )
{
    const QString gradeLevel =
        gradeLevelText(info);

    if (gradeLevel != NotAvailableText)
    {
        return gradeLevel;
    }

    if (!classroom.name.trimmed().isEmpty())
    {
        return classroom.name.trimmed();
    }

    return QStringLiteral("Class %1")
        .arg(classroom.id);
}

QString dayAbbreviation(
    const QString& day
    )
{
    if (day == QStringLiteral("Monday"))
    {
        return MyClassesPage::tr("Mon");
    }
    if (day == QStringLiteral("Tuesday"))
    {
        return MyClassesPage::tr("Tues");
    }
    if (day == QStringLiteral("Wednesday"))
    {
        return MyClassesPage::tr("Wed");
    }
    if (day == QStringLiteral("Thursday"))
    {
        return MyClassesPage::tr("Thurs");
    }
    if (day == QStringLiteral("Friday"))
    {
        return MyClassesPage::tr("Fri");
    }
    if (day == QStringLiteral("Saturday"))
    {
        return MyClassesPage::tr("Sat");
    }
    if (day == QStringLiteral("Sunday"))
    {
        return MyClassesPage::tr("Sun");
    }

    return day.trimmed();
}

QString formatSchedule(
    const QList<ClassTime>& times
    )
{
    if (times.isEmpty())
    {
        return NotAvailableText;
    }

    struct TimeGroup
    {
        QString startTime;
        QString endTime;
        QStringList days;
    };

    QList<TimeGroup> groups;

    for (const ClassTime& time : times)
    {
        const QString start =
            time.startTime.trimmed();
        const QString end =
            time.endTime.trimmed();

        auto group =
            std::find_if(
                groups.begin(),
                groups.end(),
                [&start, &end](const TimeGroup& candidate)
                {
                    return candidate.startTime == start
                        && candidate.endTime == end;
                }
                );

        if (group == groups.end())
        {
            TimeGroup newGroup;
            newGroup.startTime = start;
            newGroup.endTime = end;
            newGroup.days.append(
                dayAbbreviation(time.day)
                );

            groups.append(newGroup);
        }
        else
        {
            group->days.append(
                dayAbbreviation(time.day)
                );
        }
    }

    QStringList labels;

    for (const TimeGroup& group : groups)
    {
        const QString days =
            group.days.join(QStringLiteral("/"));

        if (group.startTime.isEmpty() && group.endTime.isEmpty())
        {
            labels.append(
                days.isEmpty()
                    ? NotAvailableText
                    : days
                );
            continue;
        }

        labels.append(
            QStringLiteral("%1 %2-%3")
                .arg(
                    days.isEmpty()
                        ? NotAvailableText
                        : days,
                    valueOrNa(group.startTime),
                    valueOrNa(group.endTime)
                    )
            );
    }

    return labels.isEmpty()
        ? NotAvailableText
        : labels.join(QStringLiteral("; "));
}

QString teacherDisplayName(
    const Teacher& teacher
    )
{
    const QString displayName =
        SidebarNodeNaming::formatTeacherDisplayName(
            teacher
            )
            .trimmed();

    return displayName.isEmpty()
        ? QObject::tr("Unassigned")
        : displayName;
}

QString teacherHeadingText(
    const Teacher& teacher,
    bool unassigned
    )
{
    if (unassigned)
    {
        return QObject::tr("Unassigned");
    }

    const QString name =
        teacherDisplayName(teacher);

    const QString room =
        teacher.roomNumber.trimmed();

    if (room.isEmpty())
    {
        return name;
    }

    return QStringLiteral("%1 - Room %2")
        .arg(name, room);
}

void clearLayout(
    QLayout* layout
    )
{
    if (!layout)
    {
        return;
    }

    while (QLayoutItem* item = layout->takeAt(0))
    {
        if (auto* childLayout = item->layout())
        {
            clearLayout(childLayout);
            delete childLayout;
            continue;
        }

        if (auto* widget = item->widget())
        {
            widget->deleteLater();
        }

        delete item;
    }
}

void clearLayoutImmediately(
    QLayout* layout
    )
{
    if (!layout)
    {
        return;
    }

    while (QLayoutItem* item = layout->takeAt(0))
    {
        if (auto* childLayout = item->layout())
        {
            clearLayoutImmediately(childLayout);
            delete childLayout;
            continue;
        }

        if (auto* widget = item->widget())
        {
            delete widget;
        }

        delete item;
    }
}

QLabel* createValueLabel(
    const QString& value,
    QWidget* parent
    )
{
    auto* label =
        new QLabel(
            valueOrNa(value),
            parent
            );

    label->setTextInteractionFlags(
        Qt::TextSelectableByMouse
        | Qt::TextSelectableByKeyboard
        );
    label->setWordWrap(true);
    label->setSizePolicy(
        QSizePolicy::Expanding,
        QSizePolicy::Preferred
        );

    return label;
}

QLineEdit* createReadOnlyValueEdit(
    const QString& value,
    QWidget* parent
    )
{
    auto* edit =
        new QLineEdit(
            valueOrNa(value),
            parent
            );

    edit->setReadOnly(true);
    edit->setCursorPosition(0);
    WidgetSizing::installTextAwareFieldWidth(
        edit,
        CompactFieldWidth
        );

    return edit;
}

void addInfoRow(
    QGridLayout* grid,
    int row,
    const QString& labelText,
    const QString& value,
    QWidget* parent
    )
{
    auto* label =
        new QLabel(labelText, parent);

    label->setContentsMargins(
        UiConstants::ClassInfo::Form::LabelIndent,
        0,
        0,
        0
        );

    auto* valueLabel =
        createValueLabel(
            value,
            parent
            );

    grid->addWidget(
        label,
        row,
        0,
        Qt::AlignLeft | Qt::AlignTop
        );
    grid->addWidget(
        valueLabel,
        row,
        1,
        Qt::AlignLeft | Qt::AlignTop
        );
}

void addHorizontalInfoField(
    QGridLayout* grid,
    int labelRow,
    int valueRow,
    int column,
    const QString& labelText,
    const QString& value,
    QWidget* parent
    )
{
    auto* label =
        new QLabel(labelText, parent);

    label->setContentsMargins(
        0,
        0,
        0,
        0
        );

    grid->addWidget(
        label,
        labelRow,
        column,
        Qt::AlignLeft
        );
    auto* valueEdit =
        createReadOnlyValueEdit(value, parent);

    grid->addWidget(
        valueEdit,
        valueRow,
        column
        );
}
}

void MyClassesPage::refreshGeneratedContent()
{
    rebuildClassInformation();
}
void MyClassesPage::rebuildClassInformation()
{
    if (
        !m_services
        || !m_services->hasOpenDatabase()
        || !m_classInformationLayout
        )
    {
        return;
    }

    clearClassInformation();
    m_classInformationTabs = nullptr;

    QList<ClassSummary> summaries;

    ClassMngr::Next::Platform::
        ApplicationServicesClassesListReadPort readPort(m_services);
    const ClassMngr::Next::Application::ClassesListReadQuery query(readPort);
    ++m_runtimeMetrics.classSummaryListQueryCount;
    const auto classes = query.execute();
    if (!classes)
    {
        DialogServices::showWarning(
            this,
            tr("Load Classes"),
            tr("Class information could not be loaded."),
            classesListErrorMessage(classes.error().message)
            );
        return;
    }

    const QList<Classroom> classrooms =
        classroomsFromListSnapshot(classes.value());
    summaries.reserve(classrooms.size());
    std::vector<ClassMngr::Next::Domain::ClassId> classInformationIds;
    classInformationIds.reserve(static_cast<std::size_t>(classrooms.size()));
    std::vector<std::size_t> summaryIndexes;
    summaryIndexes.reserve(static_cast<std::size_t>(classrooms.size()));
    for (const Classroom& classroom : classrooms)
    {
        ClassSummary summary;
        summary.classroom = classroom;
        const auto typedClassId =
            ClassMngr::Next::Domain::ClassId::fromString(
                std::to_string(classroom.id)
                );
        if (typedClassId)
        {
            classInformationIds.push_back(*typedClassId);
            summaryIndexes.push_back(
                static_cast<std::size_t>(summaries.size())
                );
        }
        summaries.append(std::move(summary));
    }

    if (!classInformationIds.empty())
    {
        ClassMngr::Next::Platform::
            ApplicationServicesMyClassesClassInformationBatchReadPort
                classInformationReadPort(m_services);
        const ClassMngr::Next::Application::
            MyClassesClassInformationBatchReadQuery classInformationQuery(
                classInformationReadPort
                );
        const auto loadedClassInformation =
            classInformationQuery.execute(classInformationIds);
        if (loadedClassInformation)
        {
            const auto& entries = loadedClassInformation.value();
            for (std::size_t index = 0; index < entries.size(); ++index)
            {
                const auto& entry = entries[index];
                if (!entry.information)
                {
                    continue;
                }

                ClassSummary& summary = summaries[
                    static_cast<qsizetype>(summaryIndexes[index])
                    ];
                const auto& fields = entry.information.value();
                summary.info = classInfoFromMyClassesSnapshot(
                    fields,
                    summary.classroom.id
                    );
                summary.teacherId = fields.teacherId;
            }
        }
    }

    if (!classInformationIds.empty())
    {
        ClassMngr::Next::Platform::
            ApplicationServicesMyClassesStudentCountBatchReadPort
                studentCountReadPort(m_services);
        const ClassMngr::Next::Application::
            MyClassesStudentCountBatchReadQuery studentCountQuery(
                studentCountReadPort
                );
        const auto loadedStudentCounts =
            studentCountQuery.execute(classInformationIds);
        if (loadedStudentCounts)
        {
            const auto& entries = loadedStudentCounts.value();
            for (std::size_t index = 0; index < entries.size(); ++index)
            {
                const auto& entry = entries[index];
                if (!entry.studentCount)
                {
                    continue;
                }

                summaries[
                    static_cast<qsizetype>(summaryIndexes[index])
                    ].studentCount = entry.studentCount.value();
            }
        }
    }

    std::vector<ClassMngr::Next::Domain::TeacherId> teacherProfileIds;
    std::unordered_set<std::string> seenTeacherProfileIds;
    teacherProfileIds.reserve(static_cast<std::size_t>(summaries.size()));
    seenTeacherProfileIds.reserve(static_cast<std::size_t>(summaries.size()));
    for (const ClassSummary& summary : summaries)
    {
        if (summary.teacherId
            && seenTeacherProfileIds.insert(
                summary.teacherId->value()
                ).second)
        {
            teacherProfileIds.push_back(*summary.teacherId);
        }
    }

    if (!teacherProfileIds.empty())
    {
        ClassMngr::Next::Platform::
            ApplicationServicesMyClassesTeacherProfileBatchReadPort
                teacherProfileReadPort(m_services);
        const ClassMngr::Next::Application::
            MyClassesTeacherProfileBatchReadQuery teacherProfileQuery(
                teacherProfileReadPort
                );
        const auto loadedProfiles = teacherProfileQuery.execute(
            teacherProfileIds
            );
        if (loadedProfiles)
        {
            std::unordered_map<std::string, std::size_t> profileIndexes;
            profileIndexes.reserve(loadedProfiles.value().size());
            for (std::size_t index = 0;
                 index < loadedProfiles.value().size();
                 ++index)
            {
                profileIndexes.emplace(
                    loadedProfiles.value()[index].teacherId.value(),
                    index
                    );
            }

            for (ClassSummary& summary : summaries)
            {
                if (!summary.teacherId)
                {
                    continue;
                }

                const auto profileIndex = profileIndexes.find(
                    summary.teacherId->value()
                    );
                if (profileIndex == profileIndexes.end())
                {
                    continue;
                }

                const auto& profile = loadedProfiles.value()[
                    profileIndex->second
                    ].profile;
                if (profile)
                {
                    summary.teacher = teacherFromMyClassesProfile(
                        profile.value()
                        );
                    summary.teacherProfileLoaded = true;
                }
            }
        }
    }

    for (ClassSummary& summary : summaries)
    {
        summary.displayName =
            classTitleText(
                summary.classroom,
                summary.info
                );
    }

    if (summaries.isEmpty())
    {
        m_selectedClassId = -1;

        auto* emptyLabel =
            new QLabel(
                tr("No classes available."),
                m_classInformationContent
                );
        emptyLabel->setObjectName("pageSubtitle");
        m_classInformationLayout->addWidget(
            emptyLabel
            );
        return;
    }

    QList<ClassTabNavigation::ClassEntry> navigationEntries;

    for (const ClassSummary& summary : std::as_const(summaries))
    {
        ClassTabNavigation::ClassEntry entry;
        entry.classId =
            summary.classroom.id;
        entry.classroomName =
            summary.classroom.name;
        entry.grade =
            summary.info.classGrade;
        entry.level =
            summary.info.classLevel;
        entry.regularTimes =
            summary.info.classTimes;
        entry.intensiveTimes =
            summary.info.intensiveTimes;
        entry.teacherEn =
            summary.teacher.teacherEn;
        entry.teacherKr =
            summary.teacher.teacherKr;

        navigationEntries.append(entry);
    }

    const ClassTabNavigation::Model navigation =
        ClassTabNavigation::build(
            navigationEntries
            );

    const int previousClassId =
        m_selectedClassId;

    const auto summaryData =
        std::make_shared<QList<ClassSummary>>(std::move(summaries));

    auto findSummary =
        [summaryData](int classId) -> const ClassSummary*
        {
            for (const ClassSummary& summary : *summaryData)
            {
                if (summary.classroom.id == classId)
                {
                    return &summary;
                }
            }

            return nullptr;
        };

    auto tabIndexForClass =
        [](NavigationTabWidget* tabs, int classId)
        {
            if (!tabs)
            {
                return -1;
            }

            for (int index = 0; index < tabs->count(); ++index)
            {
                QWidget* page =
                    tabs->widget(index);

                if (
                    page
                    && page->property("class_id").toInt() == classId
                    )
                {
                    return index;
                }
            }

            return -1;
        };

    auto createClassPage =
        [](int classId, QWidget* parent)
        {
            auto* page =
                new QWidget(parent);
            page->setProperty(
                "class_id",
                classId
                );

            auto* pageLayout =
                new QVBoxLayout(page);
            pageLayout->setContentsMargins(
                0,
                ClassTabContentTopMargin,
                0,
                0
                );
            pageLayout->setSpacing(
                UiConstants::ClassInfo::Page::ContentSpacing
                );
            pageLayout->setAlignment(Qt::AlignTop);

            return page;
        };

    auto populateClassPage =
        [this](QWidget* page, const ClassSummary& summary)
        {
            if (!page)
            {
                return;
            }

            const bool unassigned =
                !summary.teacherProfileLoaded;

            auto* pageLayout =
                qobject_cast<QVBoxLayout*>(page->layout());

            if (!pageLayout)
            {
                return;
            }

            auto* teacherHeading =
                new QLabel(
                    teacherHeadingText(
                        summary.teacher,
                        unassigned
                        ),
                    page
                    );
            teacherHeading->setObjectName("sectionTitle");
            teacherHeading->setFont(
                FontManager::getUiFont(
                    16,
                    QFont::DemiBold
                    )
                );
            pageLayout->addWidget(
                teacherHeading
                );

            auto* teacherCard =
                new QFrame(page);
            teacherCard->setProperty(
                "role",
                UiRoles::Card
                );
            teacherCard->setObjectName(
                "sectionCard"
                );

            auto* teacherLayout =
                new QVBoxLayout(teacherCard);
            teacherLayout->setAlignment(Qt::AlignTop);
            teacherLayout->setContentsMargins(
                UiConstants::ClassInfo::SectionCard::Margin,
                UiConstants::ClassInfo::SectionCard::Margin,
                UiConstants::ClassInfo::SectionCard::Margin,
                UiConstants::ClassInfo::SectionCard::Margin
                );
            teacherLayout->setSpacing(
                UiConstants::ClassInfo::SectionCard::Spacing
                );

            auto* teacherGrid =
                new QGridLayout;
            teacherGrid->setHorizontalSpacing(
                UiConstants::ClassInfo::Form::HorizontalSpacing
                );
            teacherGrid->setVerticalSpacing(
                UiConstants::ClassInfo::Form::VerticalSpacing
                );
            teacherGrid->setColumnStretch(0, 1);
            teacherGrid->setColumnStretch(1, 1);
            teacherGrid->setColumnStretch(2, 1);
            teacherGrid->setColumnStretch(3, 0);

            addHorizontalInfoField(
                teacherGrid,
                0,
                1,
                0,
                tr("Internet Type"),
                unassigned ? QString() : summary.teacher.internetType,
                teacherCard
                );
            addHorizontalInfoField(
                teacherGrid,
                0,
                1,
                1,
                tr("WiFi Name"),
                unassigned ? QString() : summary.teacher.wifiName,
                teacherCard
                );
            addHorizontalInfoField(
                teacherGrid,
                0,
                1,
                2,
                tr("WiFi Password"),
                unassigned ? QString() : summary.teacher.wifiPassword,
                teacherCard
                );
            teacherGrid->addItem(
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
            addHorizontalInfoField(
                teacherGrid,
                3,
                4,
                0,
                tr("Projection Type"),
                unassigned ? QString() : summary.teacher.projectionType,
                teacherCard
                );
            addHorizontalInfoField(
                teacherGrid,
                3,
                4,
                1,
                tr("Zoom ID"),
                unassigned ? QString() : summary.teacher.zoomId,
                teacherCard
                );
            addHorizontalInfoField(
                teacherGrid,
                3,
                4,
                2,
                tr("Zoom Password"),
                unassigned ? QString() : summary.teacher.zoomPassword,
                teacherCard
                );

            teacherLayout->addLayout(
                teacherGrid
                );

            teacherLayout->addWidget(
                createFieldLabel(
                    tr("Notes"),
                    teacherCard
                    )
                );

            auto* teacherNotes =
                createTextEdit(
                    6,
                    true,
                    teacherCard
                    );
            teacherNotes->setPlainText(
                unassigned ? QString() : summary.teacher.notes
                );
            teacherLayout->addWidget(
                teacherNotes
                );

            pageLayout->addWidget(
                teacherCard
                );

            auto* classCard =
                new SectionCard(
                    summary.displayName,
                    page
                    );

            auto* classGrid =
                new QGridLayout;
            classGrid->setHorizontalSpacing(
                UiConstants::ClassInfo::Form::HorizontalSpacing
                );
            classGrid->setVerticalSpacing(
                UiConstants::ClassInfo::Form::VerticalSpacing
                );
            classGrid->setColumnStretch(1, 1);

            int row = 0;
            addInfoRow(
                classGrid,
                row++,
                tr("# of Students"),
                QString::number(summary.studentCount),
                classCard
                );

            if (!summary.info.classTimes.isEmpty())
            {
                addInfoRow(
                    classGrid,
                    row++,
                    tr("Regular"),
                    formatSchedule(summary.info.classTimes),
                    classCard
                    );
            }

            if (!summary.info.intensiveTimes.isEmpty())
            {
                addInfoRow(
                    classGrid,
                    row++,
                    tr("Intensive"),
                    formatSchedule(summary.info.intensiveTimes),
                    classCard
                    );
            }

            if (
                summary.info.classTimes.isEmpty()
                && summary.info.intensiveTimes.isEmpty()
                )
            {
                addInfoRow(
                    classGrid,
                    row++,
                    tr("Schedule"),
                    NotAvailableText,
                    classCard
                    );
            }

            classCard->contentLayout()->addLayout(
                classGrid
                );

            classCard->contentLayout()->addWidget(
                createFieldLabel(
                    tr("Class Notes"),
                    classCard
                    )
                );

            auto* classNotes =
                createTextEdit(
                    6,
                    true,
                    classCard
                    );
            classNotes->setPlainText(
                summary.info.notes
                );
            classCard->contentLayout()->addWidget(
                classNotes
                );

            classCard->contentLayout()->addWidget(
                createFieldLabel(
                    tr("Time Filler Activities"),
                    classCard
                    )
                );

            auto* timeFillerActivities =
                createTextEdit(
                    4,
                    true,
                    classCard
                    );
            timeFillerActivities->setPlainText(
                valueOrNa(
                    summary.info.timeFillerActivities
                    )
                );
            classCard->contentLayout()->addWidget(
                timeFillerActivities
                );

            pageLayout->addWidget(
                classCard
                );
        };

    auto updateSelectedFromTabs =
        [this, findSummary, populateClassPage](NavigationTabWidget* tabs)
        {
            if (!tabs || tabs->currentIndex() < 0)
            {
                return;
            }

            QWidget* page =
                tabs->currentWidget();

            const int classId =
                page
                    ? page->property("class_id").toInt()
                    : -1;
            const ClassSummary* summary =
                classId > 0
                    ? findSummary(classId)
                    : nullptr;

            if (!summary)
            {
                return;
            }

            if (m_activeClassPage != page)
            {
                if (m_activeClassPage)
                {
                    clearLayoutImmediately(
                        m_activeClassPage->layout()
                        );
                }

                m_activeClassPage = page;
                populateClassPage(page, *summary);
            }

            m_selectedClassId = classId;
        };

    if (navigation.mode == ClassTabNavigation::Mode::Flat)
    {
        auto* tabs =
            new NavigationTabWidget(
                NavigationTabKind::Class,
                QStringLiteral("myInfoClassTabBar"),
                m_classInformationContent
                );
        tabs->setObjectName("myInfoClassTabs");

        for (const ClassTabNavigation::ClassTab& tab
             : navigation.flatClasses)
        {
            const ClassSummary* summary =
                findSummary(tab.classId);

            if (!summary)
            {
                continue;
            }

            tabs->addTab(
                createClassPage(
                    tab.classId,
                    tabs
                    ),
                tab.label
                );
        }

        connect(
            tabs,
            &NavigationTabWidget::currentChanged,
            this,
            [tabs, updateSelectedFromTabs](int)
            {
                updateSelectedFromTabs(tabs);
            }
            );

        m_materializeSelectedDetails =
            [tabs, updateSelectedFromTabs]()
            {
                updateSelectedFromTabs(tabs);
            };

        int selectedIndex =
            tabIndexForClass(
                tabs,
                previousClassId
                );

        if (selectedIndex < 0 && tabs->count() > 0)
        {
            selectedIndex = 0;
        }

        if (selectedIndex >= 0)
        {
            tabs->setCurrentIndex(selectedIndex);
        }

        updateSelectedFromTabs(tabs);

        m_classInformationTabs = tabs;
        m_classInformationLayout->addWidget(
            tabs
            );
        return;
    }

    auto* gradeTabs =
        new NavigationTabWidget(
            NavigationTabKind::Grade,
            QStringLiteral("myInfoGradeTabBar"),
            m_classInformationContent
            );
    gradeTabs->setObjectName("myInfoGradeTabs");

    int selectedGradeIndex = -1;
    int selectedClassIndex = -1;

    for (const ClassTabNavigation::GradeGroup& group
         : navigation.gradeGroups)
    {
        auto* gradePage =
            new QWidget(gradeTabs);

        auto* gradeLayout =
            new QVBoxLayout(gradePage);
        gradeLayout->setContentsMargins(0, 0, 0, 0);
        gradeLayout->setSpacing(
            UiConstants::ClassInfo::Page::ContentSpacing
            );
        gradeLayout->setAlignment(Qt::AlignTop);

        auto* classTabs =
            new NavigationTabWidget(
                NavigationTabKind::Class,
                QStringLiteral("myInfoClassTabBar"),
                gradePage
                );
        classTabs->setObjectName("myInfoClassTabs");

        for (const ClassTabNavigation::ClassTab& tab
             : group.classes)
        {
            const ClassSummary* summary =
                findSummary(tab.classId);

            if (!summary)
            {
                continue;
            }

            classTabs->addTab(
                createClassPage(
                    tab.classId,
                    classTabs
                    ),
                tab.label
                );

            if (tab.classId == previousClassId)
            {
                selectedGradeIndex =
                    gradeTabs->count();
                selectedClassIndex =
                    classTabs->count() - 1;
            }
        }

        connect(
            classTabs,
            &NavigationTabWidget::currentChanged,
            this,
            [classTabs, updateSelectedFromTabs](int)
            {
                updateSelectedFromTabs(classTabs);
            }
            );

        gradeLayout->addWidget(
            classTabs
            );

        gradeTabs->addTab(
            gradePage,
            group.label
            );
    }

    connect(
        gradeTabs,
        &NavigationTabWidget::currentChanged,
        this,
        [gradeTabs, updateSelectedFromTabs](int index)
        {
            QWidget* gradePage =
                gradeTabs->widget(index);

            auto* classTabs =
                gradePage
                    ? gradePage->findChild<NavigationTabWidget*>(
                        QStringLiteral("myInfoClassTabs"),
                        Qt::FindDirectChildrenOnly
                        )
                    : nullptr;

            updateSelectedFromTabs(classTabs);
        }
        );

    if (gradeTabs->count() > 0)
    {
        if (selectedGradeIndex < 0)
        {
            selectedGradeIndex = 0;
        }

        gradeTabs->setCurrentIndex(
            selectedGradeIndex
            );

        QWidget* gradePage =
            gradeTabs->widget(
                selectedGradeIndex
                );

        auto* selectedClassTabs =
            gradePage
                ? gradePage->findChild<NavigationTabWidget*>(
                    QStringLiteral("myInfoClassTabs"),
                    Qt::FindDirectChildrenOnly
                    )
                : nullptr;

        if (selectedClassTabs)
        {
            if (
                selectedClassIndex < 0
                || selectedClassIndex >= selectedClassTabs->count()
                )
            {
                selectedClassIndex = 0;
            }

            if (selectedClassTabs->count() > 0)
            {
                selectedClassTabs->setCurrentIndex(
                    selectedClassIndex
                    );
            }

            updateSelectedFromTabs(selectedClassTabs);
        }
    }

    m_materializeSelectedDetails =
        [gradeTabs, updateSelectedFromTabs]()
        {
            QWidget* const gradePage =
                gradeTabs->currentWidget();
            auto* const classTabs =
                gradePage
                    ? gradePage->findChild<NavigationTabWidget*>(
                        QStringLiteral("myInfoClassTabs"),
                        Qt::FindDirectChildrenOnly
                        )
                    : nullptr;
            updateSelectedFromTabs(classTabs);
        };

    m_classInformationTabs = gradeTabs;
    m_classInformationLayout->addWidget(
        gradeTabs
        );
}
void MyClassesPage::clearClassInformation()
{
    m_materializeSelectedDetails = {};
    m_activeClassPage = nullptr;
    clearLayout(
        m_classInformationLayout
        );
}

void MyClassesPage::releaseFeatureResources()
{
    if (m_activeClassPage)
    {
        clearLayoutImmediately(
            m_activeClassPage->layout()
            );
        m_activeClassPage = nullptr;
    }

    BasePage::releaseFeatureResources();
}
