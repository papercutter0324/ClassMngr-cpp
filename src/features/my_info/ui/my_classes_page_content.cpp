#include "my_classes_page.h"

#include "core/application_services.h"
#include "core/fontmanager.h"
#include "core/utils/sidebar_node_naming.h"
#include "data/data_service.h"
#include "domain/models/class_info.h"
#include "domain/models/classroom.h"
#include "domain/models/teacher.h"
#include "features/classes/models/class_tab_navigation_model.h"
#include "ui/shared/constants/gui_constants.h"
#include "ui/shared/styles/roles.h"
#include "ui/shared/utils/widget_sizing.h"
#include "ui/shared/widgets/sectioncards/class_info_section_card.h"
#include "ui/shared/widgets/uniform_width_tab_bar.h"

#include <algorithm>

#include <QFrame>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QLayout>
#include <QSizePolicy>
#include <QTabWidget>
#include <QTextEdit>
#include <QVBoxLayout>

namespace
{
constexpr int CompactFieldWidth = 170;
constexpr int ClassTabContentTopMargin = 16;
const QString NotAvailableText =
    QStringLiteral("N/A");

struct ClassSummary
{
    Classroom classroom;
    ClassInfo info;
    Teacher teacher;
    QString displayName;
    int studentCount = 0;
};

DataService* openDataService(
    ApplicationServices* services
    )
{
    auto* dataService =
        services
            ? services->dataService()
            : nullptr;

    return dataService && dataService->isOpen()
        ? dataService
        : nullptr;
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
        return QStringLiteral("Mon");
    }
    if (day == QStringLiteral("Tuesday"))
    {
        return QStringLiteral("Tues");
    }
    if (day == QStringLiteral("Wednesday"))
    {
        return QStringLiteral("Wed");
    }
    if (day == QStringLiteral("Thursday"))
    {
        return QStringLiteral("Thurs");
    }
    if (day == QStringLiteral("Friday"))
    {
        return QStringLiteral("Fri");
    }
    if (day == QStringLiteral("Saturday"))
    {
        return QStringLiteral("Sat");
    }
    if (day == QStringLiteral("Sunday"))
    {
        return QStringLiteral("Sun");
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
        }

        if (auto* widget = item->widget())
        {
            widget->deleteLater();
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
    auto* dataService =
        openDataService(m_services);

    if (!dataService || !m_classInformationLayout)
    {
        return;
    }

    clearClassInformation();
    m_classInformationTabs = nullptr;

    QList<ClassSummary> summaries;

    const QList<Classroom> classes =
        dataService->getClasses();

    for (const Classroom& classroom : classes)
    {
        ClassSummary summary;
        summary.classroom = classroom;
        summary.info =
            dataService->loadClassInfo(
                classroom.id
                );
        summary.studentCount =
            dataService->getRosterStudentCount(
                classroom.id
                );

        if (summary.info.teacherId > 0)
        {
            summary.teacher =
                dataService->getTeacher(
                    summary.info.teacherId
                    );
        }

        summary.displayName =
            classTitleText(
                classroom,
                summary.info
                );

        summaries.append(summary);
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

    auto findSummary =
        [&summaries](int classId) -> const ClassSummary*
        {
            for (const ClassSummary& summary : summaries)
            {
                if (summary.classroom.id == classId)
                {
                    return &summary;
                }
            }

            return nullptr;
        };

    auto tabIndexForClass =
        [](QTabWidget* tabs, int classId)
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

    auto updateSelectedFromTabs =
        [this](QTabWidget* tabs)
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

            if (classId > 0)
            {
                m_selectedClassId = classId;
            }
        };

    auto createClassPage =
        [this](const ClassSummary& summary, QWidget* parent)
        {
            const bool unassigned =
                summary.teacher.id <= 0;

            auto* page =
                new QWidget(parent);
            page->setProperty(
                "class_id",
                summary.classroom.id
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

            return page;
        };

    if (navigation.mode == ClassTabNavigation::Mode::Flat)
    {
        auto* tabs =
            new UniformWidthTabWidget(
                UniformWidthTabKind::Class,
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
                    *summary,
                    tabs
                    ),
                tab.label
                );
        }

        connect(
            tabs,
            &QTabWidget::currentChanged,
            this,
            [tabs, updateSelectedFromTabs](int)
            {
                updateSelectedFromTabs(tabs);
            }
            );

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
        new UniformWidthTabWidget(
            UniformWidthTabKind::Grade,
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
            new UniformWidthTabWidget(
                UniformWidthTabKind::Class,
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
                    *summary,
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
            &QTabWidget::currentChanged,
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
        &QTabWidget::currentChanged,
        this,
        [gradeTabs, updateSelectedFromTabs](int index)
        {
            QWidget* gradePage =
                gradeTabs->widget(index);

            auto* classTabs =
                gradePage
                    ? gradePage->findChild<QTabWidget*>(
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
                ? gradePage->findChild<QTabWidget*>(
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

    m_classInformationTabs = gradeTabs;
    m_classInformationLayout->addWidget(
        gradeTabs
        );
}
void MyClassesPage::clearClassInformation()
{
    clearLayout(
        m_classInformationLayout
        );
}
