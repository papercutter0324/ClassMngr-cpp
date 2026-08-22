#include "sub_prep_page_p.h"
#include "ui/shared/dialogs/user_prompt_service.h"

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

QList<SubPrepClassInformation::TeacherGroup>
SubPrepPage::buildClassInformation() const
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
    ) const
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

    for (const Classroom& classroom : *classes)
    {
        SubPrepClassInformation::SourceClass source;
        source.classroom = classroom;
        source.info =
            classService->classInfo(
                classroom.id
                ).value_or(ClassInfo{});
        source.studentCount =
            rosterService->studentCount(
                classroom.id
                ).value_or(0);

        if (source.info.teacherId > 0)
        {
            source.teacher = teacherService->teacher(source.info.teacherId)
                .value_or(Teacher{});
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
}

void SubPrepPage::clearDirty()
{
    m_dirty = false;
}
