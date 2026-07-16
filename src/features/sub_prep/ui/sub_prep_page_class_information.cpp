#include "sub_prep_page_p.h"

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
    auto* dataService =
        openDataService(m_services);

    if (
        !dataService
        || !m_scheduleWidget
        || !m_classInformationLayout
        )
    {
        return;
    }

    clearClassInformation();

    const auto groups =
        buildClassInformation();

    if (groups.isEmpty())
    {
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

    for (int groupIndex = 0; groupIndex < groups.size(); ++groupIndex)
    {
        const auto& group =
            groups.at(groupIndex);

        auto* teacherCard =
            new SectionCard(
                QStringLiteral("%1: %2")
                    .arg(
                        group.displayName,
                        group.classListText
                        ),
                m_classInformationContent
                );
        teacherCard->setObjectName(
            QStringLiteral("subPrepTeacherSectionCard")
            );
        teacherCard->setProperty(
            "teacherId",
            group.teacher.id
            );

        for (int classIndex = 0; classIndex < group.classes.size(); ++classIndex)
        {
            const auto& details =
                group.classes.at(classIndex);

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

            if (classIndex + 1 < group.classes.size())
            {
                teacherCard->contentLayout()->addWidget(
                    createSeparator(teacherCard)
                    );
            }
        }

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

        m_classInformationLayout->addWidget(teacherCard);
    }
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
    auto* dataService = openDataService(m_services);

    if (!dataService || !m_scheduleWidget)
    {
        return {};
    }

    QList<SubPrepClassInformation::SourceClass> sources;

    for (const Classroom& classroom : dataService->getClasses())
    {
        SubPrepClassInformation::SourceClass source;
        source.classroom = classroom;
        source.info =
            dataService->loadClassInfo(
                classroom.id
                );
        source.studentCount =
            dataService->getRosterStudentCount(
                classroom.id
                );

        if (source.info.teacherId > 0)
        {
            source.teacher =
                dataService->getTeacher(
                    source.info.teacherId
                    );
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
        state.showIntensive;

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
    clearLayout(m_classInformationLayout);
}

void SubPrepPage::clearDirty()
{
    m_dirty = false;
}
