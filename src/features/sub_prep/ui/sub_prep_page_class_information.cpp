#include "sub_prep_page_p.h"
#include "core/startup_profiler.h"
#include "ui/shared/dialogs/user_prompt_service.h"

#include <QTextEdit>

namespace
{
QString runtimeMetricsDetail(
    const SubPrepPageRuntimeMetrics& metrics
    )
{
    return QStringLiteral(
        "widgets=%1; textEdits=%2; navigationRows=%3; sourceClasses=%4; "
        "visibleClasses=%5; groups=%6; classInfoLookups=%7; "
        "teacherLookups=%8; rosterLookups=%9; "
        "queryCounts=classes:%10,classInfo:%11,teachers:%12,rosterCounts:%13; "
        "resultRows=classes:%14,classInfo:%15,classInfoSchedule:%16,teachers:%17,"
        "rosterCounts:%18; returnedStudents=%19; rebuilds=%20; selectedClassId=%21"
        )
        .arg(metrics.classInformationWidgetCount)
        .arg(metrics.classInformationTextEditCount)
        .arg(metrics.classInformationNavigationRowCount)
        .arg(metrics.classInformationSourceClassCount)
        .arg(metrics.classInformationVisibleClassCount)
        .arg(metrics.classInformationGroupCount)
        .arg(metrics.classInformationClassInfoLookupCount)
        .arg(metrics.classInformationTeacherLookupCount)
        .arg(metrics.classInformationRosterLookupCount)
        .arg(metrics.classInformationClassQueryCount)
        .arg(metrics.classInformationClassInfoQueryCount)
        .arg(metrics.classInformationTeacherQueryCount)
        .arg(metrics.classInformationRosterQueryCount)
        .arg(metrics.classInformationClassResultRowCount)
        .arg(metrics.classInformationClassInfoResultRowCount)
        .arg(metrics.classInformationClassInfoScheduleRowCount)
        .arg(metrics.classInformationTeacherResultRowCount)
        .arg(metrics.classInformationRosterResultRowCount)
        .arg(metrics.classInformationRosterStudentResultCount)
        .arg(metrics.classInformationRebuildCount)
        .arg(metrics.selectedClassId);
}
}

void SubPrepPage::refreshGeneratedContent()
{
    if (m_scheduleWidget)
    {
        m_scheduleWidget->refreshSchedule();
    }

    rebuildClassInformation();
}

void SubPrepPage::rebuildClassInformation()
{
    ++m_classInformationRebuildCount;
    m_classInformationSourceClassCount = 0;
    m_classInformationVisibleClassCount = 0;
    m_classInformationGroupCount = 0;
    m_classInformationNavigationRowCount = 0;
    m_classInformationClassInfoLookupCount = 0;
    m_classInformationTeacherLookupCount = 0;
    m_classInformationRosterLookupCount = 0;
    m_classInformationClassQueryCount = 0;
    m_classInformationClassResultRowCount = 0;
    m_classInformationClassInfoQueryCount = 0;
    m_classInformationClassInfoResultRowCount = 0;
    m_classInformationClassInfoScheduleRowCount = 0;
    m_classInformationTeacherQueryCount = 0;
    m_classInformationTeacherResultRowCount = 0;
    m_classInformationRosterQueryCount = 0;
    m_classInformationRosterResultRowCount = 0;
    m_classInformationRosterStudentResultCount = 0;

    StartupProfiler::recordSubPrepClassInformationLifecycle(
        QStringLiteral("rebuild-start"),
        runtimeMetricsDetail(runtimeMetrics())
        );

    if (
        !openClassService(m_services)
        || !openTeacherService(m_services)
        || !openRosterService(m_services)
        || !m_scheduleWidget
        || !m_classInformationLayout
        )
    {
        return;
    }

    const int currentClassId =
        currentClassInformationId();

    if (currentClassId > 0)
    {
        m_selectedClassId =
            currentClassId;
    }

    clearClassInformation();

    const auto groups =
        buildClassInformation();

    m_classInformationGroupCount = groups.size();
    for (const auto& group : groups)
    {
        m_classInformationVisibleClassCount += group.classes.size();
    }

    StartupProfiler::recordSubPrepClassInformationLifecycle(
        groups.isEmpty()
            ? QStringLiteral("data-empty")
            : QStringLiteral("data-loaded"),
        runtimeMetricsDetail(runtimeMetrics())
        );

    if (groups.isEmpty())
    {
        m_selectedClassId = -1;

        auto* emptyLabel =
            new QLabel(
                tr("No scheduled class information available."),
                m_classInformationContent
                );
        emptyLabel->setObjectName("pageSubtitle");
        emptyLabel->setWordWrap(true);
        m_classInformationLayout->addWidget(emptyLabel);
        return;
    }

    QList<ClassTabNavigation::ClassEntry> navigationEntries;

    for (const auto& group : groups)
    {
        for (const auto& details : group.classes)
        {
            ClassTabNavigation::ClassEntry entry;
            entry.classId = details.classId;
            entry.classroomName = details.classLabel;
            entry.grade = details.info.classGrade;
            entry.level = details.info.classLevel;
            entry.regularTimes = details.info.classTimes;
            entry.intensiveTimes = details.info.intensiveTimes;
            entry.teacherEn = group.teacher.teacherEn;
            entry.teacherKr = group.teacher.teacherKr;
            navigationEntries.append(entry);
        }
    }

    const ClassTabNavigation::Model navigation =
        ClassTabNavigation::build(
            navigationEntries,
            ClassTabNavigation::GroupingPolicy::AlwaysGradeGrouped
            );
    m_classInformationNavigationRowCount = navigationEntries.size();

    StartupProfiler::recordSubPrepClassInformationLifecycle(
        QStringLiteral("view-build-start"),
        runtimeMetricsDetail(runtimeMetrics())
        );

    auto* gradeTabs =
        new NavigationTabWidget(
            NavigationTabKind::Grade,
            QStringLiteral("subPrepGradeTabBar"),
            m_classInformationContent
            );
    gradeTabs->setObjectName(
        QStringLiteral("subPrepGradeTabs")
        );
    m_classInformationTabs = gradeTabs;

    QHash<int, QWidget*> classPages;

    int classPageCount = 0;
    for (int groupIndex = 0; groupIndex < groups.size(); ++groupIndex)
    {
        const auto& group =
            groups.at(groupIndex);

        for (int classIndex = 0; classIndex < group.classes.size(); ++classIndex)
        {
            const auto& details =
                group.classes.at(classIndex);

            auto* classPage =
                new QWidget(gradeTabs);
            classPage->setObjectName(
                QStringLiteral("subPrepClassPage")
                );
            classPage->setProperty(
                "classId",
                details.classId
                );

            auto* classPageLayout =
                new QVBoxLayout(classPage);
            classPageLayout->setContentsMargins(0, 0, 0, 0);
            classPageLayout->setSpacing(
                UiConstants::ClassInfo::Page::ContentSpacing
                );
            classPageLayout->setAlignment(Qt::AlignTop);

            auto* teacherCard =
                new SectionCard(
                    QStringLiteral("%1: %2")
                        .arg(
                            group.displayName,
                            details.classLabel
                            ),
                    classPage
                    );
            teacherCard->setObjectName(
                QStringLiteral("subPrepTeacherSectionCard")
                );
            teacherCard->setProperty(
                "teacherId",
                group.teacher.id
                );

            auto* detailsWidget =
                new QWidget(teacherCard);
            detailsWidget->setObjectName(
                QStringLiteral("subPrepClassDetails")
                );
            detailsWidget->setProperty(
                "classId",
                details.classId
                );
            auto* detailsLayout =
                new QVBoxLayout(detailsWidget);
            detailsLayout->setContentsMargins(0, 0, 0, 0);
            detailsLayout->setSpacing(
                UiConstants::ClassInfo::Form::VerticalSpacing
                );

            auto* fields =
                new QGridLayout;
            fields->setHorizontalSpacing(
                UiConstants::ClassInfo::Form::HorizontalSpacing
                );
            fields->setVerticalSpacing(
                UiConstants::ClassInfo::Form::VerticalSpacing
                );

            fields->addWidget(
                createInlineValue(
                    tr("Level"),
                    details.info.classLevel,
                    detailsWidget
                    ),
                0,
                0
                );
            fields->addWidget(
                createInlineValue(
                    tr("Time"),
                    details.timeText,
                    detailsWidget
                    ),
                0,
                1
                );
            fields->addWidget(
                createInlineValue(
                    tr("# of Students"),
                    QString::number(details.studentCount),
                    detailsWidget
                    ),
                0,
                2
                );
            fields->addWidget(
                createInlineValue(
                    tr("Room"),
                    group.teacher.roomNumber,
                    detailsWidget
                    ),
                0,
                3
                );

            fields->addWidget(
                createInlineValue(
                    tr("WiFi Name"),
                    group.teacher.wifiName,
                    detailsWidget
                    ),
                1,
                0
                );
            fields->addWidget(
                createInlineValue(
                    tr("WiFi Password"),
                    group.teacher.wifiPassword,
                    detailsWidget
                    ),
                1,
                1
                );
            fields->addWidget(
                createInlineValue(
                    tr("Zoom ID"),
                    group.teacher.zoomId,
                    detailsWidget
                    ),
                1,
                2
                );
            fields->addWidget(
                createInlineValue(
                    tr("Zoom Password"),
                    group.teacher.zoomPassword,
                    detailsWidget
                    ),
                1,
                3
                );

            fields->addWidget(
                createInlineValue(
                    tr("Internet"),
                    group.teacher.internetType,
                    detailsWidget
                    ),
                2,
                0
                );
            fields->addWidget(
                createInlineValue(
                    tr("Projection"),
                    group.teacher.projectionType,
                    detailsWidget
                    ),
                2,
                1
                );

            for (int column = 0; column < 4; ++column)
            {
                fields->setColumnStretch(column, 1);
            }

            detailsLayout->addLayout(fields);
            detailsLayout->addWidget(
                createFieldLabel(
                    tr("Class Notes"),
                    detailsWidget
                    )
                );

            auto* classNotes =
                createTextEdit(
                    ClassNotesLines,
                    true,
                    detailsWidget
                    );
            classNotes->setProperty(
                "classId",
                details.classId
                );
            classNotes->setPlainText(
                valueOrNa(details.info.notes)
                );
            detailsLayout->addWidget(classNotes);

            teacherCard->contentLayout()->addWidget(detailsWidget);

            auto* teacherNotesWidget =
                new QWidget(teacherCard);
            auto* teacherNotesLayout =
                new QVBoxLayout(teacherNotesWidget);
            teacherNotesLayout->setContentsMargins(0, 0, 0, 0);
            teacherNotesLayout->setSpacing(
                UiConstants::ClassInfo::Form::VerticalSpacing
                );
            teacherNotesLayout->addWidget(
                createFieldLabel(
                    tr("Co-Teacher Notes"),
                    teacherNotesWidget
                    )
                );

            auto* teacherNotes =
                createTextEdit(
                    TeacherNotesLines,
                    true,
                    teacherNotesWidget
                    );
            teacherNotes->setProperty(
                "teacherId",
                group.teacher.id
                );
            teacherNotes->setPlainText(
                valueOrNa(group.teacher.notes)
                );
            teacherNotesLayout->addWidget(teacherNotes);
            teacherCard->contentLayout()->addWidget(teacherNotesWidget);
            classPageLayout->addWidget(teacherCard);

            classPages.insert(
                details.classId,
                classPage
                );
            ++classPageCount;

            if (
                classPageCount % 8 == 0
                || classPageCount == navigationEntries.size()
                )
            {
                StartupProfiler::recordSubPrepClassInformationLifecycle(
                    QStringLiteral("view-build-progress"),
                    runtimeMetricsDetail(runtimeMetrics())
                    );
            }
        }
    }

    int selectedGradeIndex = -1;
    int selectedLevelIndex = -1;

    for (const ClassTabNavigation::GradeGroup& gradeGroup
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

        auto* levelTabs =
            new NavigationTabWidget(
                NavigationTabKind::Class,
                QStringLiteral("subPrepLevelTabBar"),
                gradePage
                );
        levelTabs->setObjectName(
            QStringLiteral("subPrepLevelTabs")
            );

        for (const ClassTabNavigation::ClassTab& classTab
             : gradeGroup.classes)
        {
            QWidget* classPage =
                classPages.take(classTab.classId);

            if (!classPage)
            {
                continue;
            }

            const int levelIndex =
                levelTabs->addTab(
                    classPage,
                    classTab.label
                    );

            if (classTab.classId == m_selectedClassId)
            {
                selectedGradeIndex =
                    gradeTabs->count();
                selectedLevelIndex =
                    levelIndex;
            }
        }

        connect(
            levelTabs,
            &NavigationTabWidget::currentChanged,
            this,
            [this](int)
            {
                m_selectedClassId =
                    currentClassInformationId();
            }
            );

        gradeLayout->addWidget(levelTabs);
        gradeTabs->addTab(
            gradePage,
            gradeGroup.label
            );
    }

    connect(
        gradeTabs,
        &NavigationTabWidget::currentChanged,
        this,
        [this](int)
        {
            m_selectedClassId =
                currentClassInformationId();
        }
        );

    if (selectedGradeIndex < 0)
    {
        selectedGradeIndex = 0;
    }

    gradeTabs->setCurrentIndex(
        selectedGradeIndex
        );

    auto* selectedLevelTabs =
        gradeTabs->currentWidget()
            ? gradeTabs
                ->currentWidget()
                ->findChild<NavigationTabWidget*>(
                    QStringLiteral("subPrepLevelTabs"),
                    Qt::FindDirectChildrenOnly
                    )
            : nullptr;

    if (selectedLevelTabs)
    {
        selectedLevelTabs->setCurrentIndex(
            selectedLevelIndex >= 0
                ? selectedLevelIndex
                : 0
            );
    }

    m_selectedClassId =
        currentClassInformationId();
    m_classInformationLayout->addWidget(gradeTabs);

    StartupProfiler::recordSubPrepClassInformationLifecycle(
        QStringLiteral("view-ready"),
        runtimeMetricsDetail(runtimeMetrics())
        );
}

int SubPrepPage::currentClassInformationId() const
{
    if (
        !m_classInformationTabs
        || !m_classInformationTabs->currentWidget()
        )
    {
        return -1;
    }

    auto* levelTabs =
        m_classInformationTabs
            ->currentWidget()
            ->findChild<NavigationTabWidget*>(
                QStringLiteral("subPrepLevelTabs"),
                Qt::FindDirectChildrenOnly
                );

    if (!levelTabs || !levelTabs->currentWidget())
    {
        return -1;
    }

    return levelTabs
        ->currentWidget()
        ->property("classId")
        .toInt();
}

bool SubPrepPage::selectClassForStartupDiagnostics(int classId)
{
    if (classId <= 0 || !m_classInformationTabs)
    {
        return false;
    }

    for (int gradeIndex = 0;
         gradeIndex < m_classInformationTabs->count();
         ++gradeIndex)
    {
        QWidget* gradePage = m_classInformationTabs->widget(gradeIndex);
        auto* levelTabs =
            gradePage
                ? gradePage->findChild<NavigationTabWidget*>(
                    QStringLiteral("subPrepLevelTabs"),
                    Qt::FindDirectChildrenOnly
                    )
                : nullptr;
        if (!levelTabs)
        {
            continue;
        }

        for (int levelIndex = 0;
             levelIndex < levelTabs->count();
             ++levelIndex)
        {
            QWidget* classPage = levelTabs->widget(levelIndex);
            if (
                !classPage
                || classPage->property("classId").toInt() != classId
                )
            {
                continue;
            }

            m_classInformationTabs->setCurrentIndex(gradeIndex);
            levelTabs->setCurrentIndex(levelIndex);
            m_selectedClassId = currentClassInformationId();
            return m_selectedClassId == classId;
        }
    }

    return false;
}

SubPrepPageRuntimeMetrics SubPrepPage::runtimeMetrics() const
{
    SubPrepPageRuntimeMetrics metrics;
    metrics.classInformationNavigationRowCount =
        m_classInformationNavigationRowCount;
    metrics.classInformationSourceClassCount =
        m_classInformationSourceClassCount;
    metrics.classInformationVisibleClassCount =
        m_classInformationVisibleClassCount;
    metrics.classInformationGroupCount =
        m_classInformationGroupCount;
    metrics.classInformationClassInfoLookupCount =
        m_classInformationClassInfoLookupCount;
    metrics.classInformationTeacherLookupCount =
        m_classInformationTeacherLookupCount;
    metrics.classInformationRosterLookupCount =
        m_classInformationRosterLookupCount;
    metrics.classInformationClassQueryCount =
        m_classInformationClassQueryCount;
    metrics.classInformationClassResultRowCount =
        m_classInformationClassResultRowCount;
    metrics.classInformationClassInfoQueryCount =
        m_classInformationClassInfoQueryCount;
    metrics.classInformationClassInfoResultRowCount =
        m_classInformationClassInfoResultRowCount;
    metrics.classInformationClassInfoScheduleRowCount =
        m_classInformationClassInfoScheduleRowCount;
    metrics.classInformationTeacherQueryCount =
        m_classInformationTeacherQueryCount;
    metrics.classInformationTeacherResultRowCount =
        m_classInformationTeacherResultRowCount;
    metrics.classInformationRosterQueryCount =
        m_classInformationRosterQueryCount;
    metrics.classInformationRosterResultRowCount =
        m_classInformationRosterResultRowCount;
    metrics.classInformationRosterStudentResultCount =
        m_classInformationRosterStudentResultCount;
    metrics.classInformationRebuildCount =
        m_classInformationRebuildCount;
    metrics.selectedClassId = m_selectedClassId;

    if (m_classInformationContent)
    {
        metrics.classInformationWidgetCount =
            m_classInformationContent
                ->findChildren<QWidget*>()
                .size();
        metrics.classInformationTextEditCount =
            m_classInformationContent
                ->findChildren<QTextEdit*>()
                .size();
    }

    return metrics;
}

QList<SubPrepClassInformation::TeacherGroup>
SubPrepPage::buildClassInformation()
{
    if (!m_scheduleWidget)
    {
        return {};
    }

    return buildClassInformation(
        m_scheduleWidget->scheduleModel()
        );
}

QList<SubPrepClassInformation::TeacherGroup>
SubPrepPage::buildClassInformation(
    const ScheduleViewModel& schedule
    )
{
    auto* classService = openClassService(m_services);
    auto* teacherService = openTeacherService(m_services);
    auto* rosterService = openRosterService(m_services);

    if (
        !classService
        || !teacherService
        || !rosterService
        || !m_scheduleWidget
        )
    {
        return {};
    }

    QList<SubPrepClassInformation::SourceClass> sources;
    ++m_classInformationClassQueryCount;
    const Result<QList<Classroom>> classes = classService->classes();
    if (!classes)
    {
        DialogServices::showWarning(
            const_cast<SubPrepPage*>(this),
            tr("Load Class Information"),
            tr("Classes could not be loaded."),
            classes.error()
            );
        return {};
    }

    m_classInformationSourceClassCount = classes->size();
    m_classInformationClassResultRowCount = classes->size();

    for (const Classroom& classroom : *classes)
    {
        SubPrepClassInformation::SourceClass source;
        source.classroom = classroom;
        ++m_classInformationClassInfoLookupCount;
        ++m_classInformationClassInfoQueryCount;
        const Result<ClassInfo> classInfo =
            classService->classInfo(classroom.id);
        if (classInfo)
        {
            ++m_classInformationClassInfoResultRowCount;
            source.info = *classInfo;
            m_classInformationClassInfoScheduleRowCount +=
                classInfo->classTimes.size()
                + classInfo->intensiveTimes.size();
        }
        ++m_classInformationRosterLookupCount;
        ++m_classInformationRosterQueryCount;
        const Result<int> studentCount =
            rosterService->studentCount(classroom.id);
        if (studentCount)
        {
            ++m_classInformationRosterResultRowCount;
            source.studentCount = qMax(0, *studentCount);
            m_classInformationRosterStudentResultCount +=
                source.studentCount;
        }

        if (source.info.teacherId > 0)
        {
            ++m_classInformationTeacherLookupCount;
            ++m_classInformationTeacherQueryCount;
            const Result<Teacher> teacher =
                teacherService->teacher(source.info.teacherId);
            if (teacher)
            {
                ++m_classInformationTeacherResultRowCount;
                source.teacher = *teacher;
            }
        }

        sources.append(source);
    }

    const ScheduleDisplayState state =
        m_scheduleWidget->displayState();

    SubPrepClassInformation::BuildOptions options;
    options.visibleClassIds =
        visibleClassIds(schedule);
    options.visibleDays = schedule.days;
    options.useIntensive =
        state.displayMode
            == ScheduleDisplayMode::Intensive;

    return SubPrepClassInformation::build(
        sources,
        options
        );
}

bool SubPrepPage::restoreGradingDefaultIfNeeded()
{
    if (
        !m_gradingInstructionsEdit
        || !m_gradingInstructionsEdit
                ->toPlainText()
                .trimmed()
                .isEmpty()
        )
    {
        return false;
    }

    const QSignalBlocker blocker(m_gradingInstructionsEdit);
    m_gradingInstructionsEdit->setPlainText(
        defaultGradingInstructions()
        );

    return true;
}

QString SubPrepPage::defaultGradingInstructions() const
{
    return tr(
        "Scoring: 0 / 20 / 40 / 60 / 80 / 100\n"
        "Comments: Please leave a comment about what the student did well "
        "and what they need to work on."
        );
}

QString SubPrepPage::defaultSpecialInstructions() const
{
    return tr("N/A");
}

QLabel* SubPrepPage::createTopLevelHeading(
    const QString& text,
    QWidget* parent
    ) const
{
    auto* label =
        new QLabel(text, parent);

    label->setObjectName("sectionTitle");
    label->setAlignment(Qt::AlignCenter);
    label->setFont(
        FontManager::getUiFont(
            UiConstants::Pages::SectionTitleFontSize,
            QFont::DemiBold
            )
        );

    return label;
}

QLabel* SubPrepPage::createFieldLabel(
    const QString& text,
    QWidget* parent
    ) const
{
    auto* label =
        new QLabel(text, parent);

    label->setContentsMargins(
        UiConstants::ClassInfo::Form::LabelIndent,
        0,
        0,
        0
        );

    return label;
}

QTextEdit* SubPrepPage::createTextEdit(
    int minimumLines,
    bool readOnly,
    QWidget* parent
    ) const
{
    auto* edit =
        new QTextEdit(parent);

    edit->setReadOnly(readOnly);
    edit->setMinimumHeight(
        textEditHeightForLines(
            edit,
            minimumLines
            )
        );
    edit->setVerticalScrollBarPolicy(
        Qt::ScrollBarAsNeeded
        );
    edit->setHorizontalScrollBarPolicy(
        Qt::ScrollBarAsNeeded
        );
    edit->setSizePolicy(
        QSizePolicy::Expanding,
        QSizePolicy::Preferred
        );

    return edit;
}

void SubPrepPage::clearClassInformation()
{
    m_classInformationTabs = nullptr;
    clearLayout(m_classInformationLayout);

    StartupProfiler::recordSubPrepClassInformationLifecycle(
        QStringLiteral("view-clear-requested"),
        runtimeMetricsDetail(runtimeMetrics())
        );
}

void SubPrepPage::clearDirty()
{
    m_dirty = false;
}
