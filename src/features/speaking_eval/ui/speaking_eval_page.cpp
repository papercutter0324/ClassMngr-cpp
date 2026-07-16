#include "speaking_eval_page_p.h"

SpeakingEvalPage::SpeakingEvalPage(
    ApplicationServices* services,
    QWidget* parent
    )
    : BasePage(parent)
    , m_services(services)
{
    setProperty("role", UiRoles::SpeakingEvals);

    buildUi();

    m_autosaveTimer =
        new QTimer(this);
    m_autosaveTimer->setSingleShot(true);
    m_autosaveTimer->setInterval(
        AutosaveDelayMs
        );

    connect(
        m_autosaveTimer,
        &QTimer::timeout,
        this,
        &SpeakingEvalPage::autosave
        );
}

void SpeakingEvalPage::loadEvaluation(
    const Classroom& classroom,
    const QString& evaluationName
    )
{
    loadEvaluations(
        classroom.id,
        evaluationName
        );
}

void SpeakingEvalPage::loadEvaluations(
    int selectedClassId,
    const QString& selectedEvaluationName
    )
{
    auto* dataService =
        m_services
            ? m_services->dataService()
            : nullptr;

    if (!dataService || !dataService->isOpen())
    {
        m_evaluationClasses.clear();
        rebuildClassTabs(-1);

        m_syncingEvaluationTabs = true;
        if (m_evaluationTabs)
        {
            m_evaluationTabs->setCurrentIndex(0);
        }
        m_syncingEvaluationTabs = false;

        loadEvaluationData({}, {});
        setEvaluationEditorAvailable(false);
        return;
    }

    m_evaluationClasses =
        dataService->getClasses();

    int classId =
        selectedClassId > 0
            ? selectedClassId
            : m_classroom.id;

    if (classroomById(classId).id <= 0)
    {
        classId =
            firstEvaluationClassId();
    }

    const QString evaluationName =
        normalizedEvaluationName(
            selectedEvaluationName.trimmed().isEmpty()
                ? m_evaluationName
                : selectedEvaluationName
            );

    rebuildClassTabs(classId);

    m_syncingEvaluationTabs = true;
    if (m_evaluationTabs)
    {
        for (int index = 0; index < m_evaluationTabs->count(); ++index)
        {
            QWidget* page =
                m_evaluationTabs->widget(index);

            if (
                page
                && page->property("evaluation_name").toString() == evaluationName
                )
            {
                m_evaluationTabs->setCurrentIndex(index);
                break;
            }
        }
    }
    m_syncingEvaluationTabs = false;

    const Classroom classroom =
        classroomById(classId);

    loadEvaluationData(
        classroom,
        classroom.id > 0
            ? evaluationName
            : QString()
        );

    setEvaluationEditorAvailable(
        classroom.id > 0
        );
}

void SpeakingEvalPage::loadEvaluationData(
    const Classroom& classroom,
    const QString& evaluationName
    )
{
    if (m_autosaveTimer)
    {
        m_autosaveTimer->stop();
    }

    m_loadingEvaluation = true;

    m_classroom =
        classroom;

    m_evaluationName =
        evaluationName;

    SpeakingEvalRows rows;

    if (
        m_services
        && m_services->dataService()
        && m_classroom.id > 0
        )
    {
        rows =
            m_services
                ->dataService()
                ->loadSpeakingEval(
                    m_classroom.id,
                    m_evaluationName
                    );
    }

    if (rows.isEmpty())
    {
        rows =
            SpeakingEval::emptyRows();
    }

    m_model->loadData(rows);
    setupTable();
    updateHeaderText();
    updateActions();

    m_loadingEvaluation = false;
}

void SpeakingEvalPage::rebuildClassTabs(
    int selectedClassId
    )
{
    if (!m_classTabsLayout || !m_classTabsContainer)
    {
        return;
    }

    m_rebuildingClassTabs = true;

    while (QLayoutItem* item = m_classTabsLayout->takeAt(0))
    {
        if (QWidget* widget = item->widget())
        {
            widget->deleteLater();
        }

        delete item;
    }

    m_classTabs = nullptr;

    auto* dataService =
        m_services
            ? m_services->dataService()
            : nullptr;

    QList<ClassTabNavigation::ClassEntry> entries;

    if (dataService && dataService->isOpen())
    {
        for (const Classroom& classroom : std::as_const(m_evaluationClasses))
        {
            if (classroom.id <= 0)
            {
                continue;
            }

            const ClassInfo info =
                dataService->loadClassInfo(
                    classroom.id
                    );

            Teacher teacher;

            if (info.teacherId > 0)
            {
                teacher =
                    dataService->getTeacher(
                        info.teacherId
                        );
            }

            ClassTabNavigation::ClassEntry entry;
            entry.classId =
                classroom.id;
            entry.classroomName =
                classroom.name;
            entry.grade =
                info.classGrade;
            entry.level =
                info.classLevel;
            entry.regularTimes =
                info.classTimes;
            entry.intensiveTimes =
                info.intensiveTimes;
            entry.teacherEn =
                teacher.teacherEn;
            entry.teacherKr =
                teacher.teacherKr;

            entries.append(entry);
        }
    }

    const ClassTabNavigation::Model navigation =
        ClassTabNavigation::build(entries);

    const auto createTabPage =
        [](QWidget* parent, int classId)
        {
            auto* page =
                new QWidget(parent);
            page->setProperty(
                "class_id",
                classId
                );
            return page;
        };

    const auto connectClassTabs =
        [this](QTabWidget* tabs)
        {
            connect(
                tabs,
                &QTabWidget::currentChanged,
                this,
                [this, tabs](int)
                {
                    if (
                        m_rebuildingClassTabs
                        || m_restoringClassTabs
                        || m_syncingEvaluationTabs
                        )
                    {
                        return;
                    }

                    activateEvaluation(
                        currentClassIdFromTabs(tabs),
                        currentEvaluationNameFromTabs()
                        );
                }
                );
        };

    if (navigation.mode == ClassTabNavigation::Mode::Flat)
    {
        auto* tabs =
            new UniformWidthTabWidget(
                UniformWidthTabKind::Class,
                QStringLiteral("speakingEvalClassTabBar"),
                m_classTabsContainer
                );
        tabs->setSizePolicy(
            QSizePolicy::Expanding,
            QSizePolicy::Maximum
            );
        tabs->setObjectName("speakingEvalClassTabs");

        for (const ClassTabNavigation::ClassTab& tab
             : navigation.flatClasses)
        {
            tabs->addTab(
                createTabPage(
                    tabs,
                    tab.classId
                    ),
                tab.label
                );
        }

        connectClassTabs(tabs);

        m_classTabs =
            tabs;
        m_classTabsLayout->addWidget(tabs);
    }
    else
    {
        auto* gradeTabs =
            new UniformWidthTabWidget(
                UniformWidthTabKind::Grade,
                QStringLiteral("speakingEvalGradeTabBar"),
                m_classTabsContainer
                );
        gradeTabs->setSizePolicy(
            QSizePolicy::Expanding,
            QSizePolicy::Maximum
            );
        gradeTabs->setObjectName("speakingEvalGradeTabs");

        for (const ClassTabNavigation::GradeGroup& group
             : navigation.gradeGroups)
        {
            auto* gradePage =
                new QWidget(gradeTabs);

            auto* gradeLayout =
                new QVBoxLayout(gradePage);
            gradeLayout->setContentsMargins(0, 0, 0, 0);
            gradeLayout->setSpacing(8);
            gradeLayout->setAlignment(Qt::AlignTop);

            auto* classTabs =
                new UniformWidthTabWidget(
                    UniformWidthTabKind::Class,
                    QStringLiteral("speakingEvalClassTabBar"),
                    gradePage
                    );
            classTabs->setSizePolicy(
                QSizePolicy::Expanding,
                QSizePolicy::Maximum
                );
            classTabs->setObjectName("speakingEvalClassTabs");

            for (const ClassTabNavigation::ClassTab& tab
                 : group.classes)
            {
                classTabs->addTab(
                    createTabPage(
                        classTabs,
                        tab.classId
                        ),
                    tab.label
                    );
            }

            connectClassTabs(classTabs);

            gradeLayout->addWidget(classTabs);
            gradeTabs->addTab(
                gradePage,
                group.label
                );
        }

        connect(
            gradeTabs,
            &QTabWidget::currentChanged,
            this,
            [this, gradeTabs](int)
            {
                if (
                    m_rebuildingClassTabs
                    || m_restoringClassTabs
                    || m_syncingEvaluationTabs
                    )
                {
                    return;
                }

                activateEvaluation(
                    currentClassIdFromTabs(gradeTabs),
                    currentEvaluationNameFromTabs()
                    );
            }
            );

        m_classTabs =
            gradeTabs;
        m_classTabsLayout->addWidget(gradeTabs);
    }

    m_classTabsContainer->setVisible(
        m_classTabs && m_classTabs->count() > 0
        );

    if (m_evaluationTabs && m_classTabs && m_classTabs->tabBar())
    {
        const int tabFontSize =
            m_classTabs->tabBar()->font().pointSize();

        const QFont evaluationTabFont =
            FontManager::getUiFont(
                tabFontSize > 0
                    ? tabFontSize
                    : FontManager::getPlatformFontSize()
                );

        m_evaluationTabs->setFont(
            evaluationTabFont
            );
        m_evaluationTabs->tabBar()->setFont(
            evaluationTabFont
            );
    }

    syncTabWidgetToClass(
        m_classTabs,
        selectedClassId
        );

    m_rebuildingClassTabs = false;
}

bool SpeakingEvalPage::activateEvaluation(
    int classId,
    const QString& evaluationName
    )
{
    if (
        classId <= 0
        || m_rebuildingClassTabs
        || m_restoringClassTabs
        || m_syncingEvaluationTabs
        )
    {
        return false;
    }

    const QString normalizedName =
        normalizedEvaluationName(evaluationName);

    if (
        classId == m_classroom.id
        && normalizedName == m_evaluationName
        )
    {
        return true;
    }

    if (m_classroom.id > 0 && !saveChanges())
    {
        restoreEvaluationTabSelection();
        return false;
    }

    const Classroom classroom =
        classroomById(classId);

    if (classroom.id <= 0)
    {
        restoreEvaluationTabSelection();
        return false;
    }

    loadEvaluationData(
        classroom,
        normalizedName
        );
    setEvaluationEditorAvailable(true);

    return true;
}

void SpeakingEvalPage::restoreEvaluationTabSelection()
{
    m_restoringClassTabs = true;
    syncTabWidgetToClass(
        m_classTabs,
        m_classroom.id
        );
    m_restoringClassTabs = false;

    m_syncingEvaluationTabs = true;

    if (m_evaluationTabs)
    {
        for (int index = 0; index < m_evaluationTabs->count(); ++index)
        {
            QWidget* page =
                m_evaluationTabs->widget(index);

            if (
                page
                && page->property("evaluation_name").toString() == m_evaluationName
                )
            {
                m_evaluationTabs->setCurrentIndex(index);
                break;
            }
        }
    }

    m_syncingEvaluationTabs = false;
}

void SpeakingEvalPage::syncTabWidgetToClass(
    QTabWidget* tabs,
    int classId
    )
{
    if (!tabs)
    {
        return;
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
            tabs->setCurrentIndex(index);
            return;
        }

        auto* nestedTabs =
            page
                ? page->findChild<QTabWidget*>(
                    QStringLiteral("speakingEvalClassTabs")
                    )
                : nullptr;

        if (!nestedTabs)
        {
            continue;
        }

        for (int childIndex = 0; childIndex < nestedTabs->count(); ++childIndex)
        {
            QWidget* childPage =
                nestedTabs->widget(childIndex);

            if (
                childPage
                && childPage->property("class_id").toInt() == classId
                )
            {
                tabs->setCurrentIndex(index);
                nestedTabs->setCurrentIndex(childIndex);
                return;
            }
        }
    }

    if (tabs->count() > 0)
    {
        tabs->setCurrentIndex(0);

        auto* nestedTabs =
            tabs->currentWidget()
                ? tabs->currentWidget()->findChild<QTabWidget*>(
                    QStringLiteral("speakingEvalClassTabs")
                    )
                : nullptr;

        if (nestedTabs && nestedTabs->count() > 0)
        {
            nestedTabs->setCurrentIndex(0);
        }
    }
}

int SpeakingEvalPage::currentClassIdFromTabs(
    QTabWidget* tabs
    ) const
{
    if (!tabs || tabs->currentIndex() < 0)
    {
        return -1;
    }

    QWidget* page =
        tabs->currentWidget();

    const int pageClassId =
        page
            ? page->property("class_id").toInt()
            : -1;

    if (pageClassId > 0)
    {
        return pageClassId;
    }

    auto* nestedTabs =
        page
            ? page->findChild<QTabWidget*>(
                QStringLiteral("speakingEvalClassTabs")
                )
            : nullptr;

    if (!nestedTabs || nestedTabs->currentIndex() < 0)
    {
        return -1;
    }

    QWidget* classPage =
        nestedTabs->currentWidget();

    return classPage
        ? classPage->property("class_id").toInt()
        : -1;
}

QString SpeakingEvalPage::currentEvaluationNameFromTabs() const
{
    if (!m_evaluationTabs || m_evaluationTabs->currentIndex() < 0)
    {
        return evaluationNames().constFirst();
    }

    QWidget* page =
        m_evaluationTabs->currentWidget();

    return normalizedEvaluationName(
        page
            ? page->property("evaluation_name").toString()
            : QString()
        );
}

Classroom SpeakingEvalPage::classroomById(
    int classId
    ) const
{
    for (const Classroom& classroom : m_evaluationClasses)
    {
        if (classroom.id == classId)
        {
            return classroom;
        }
    }

    return {};
}

int SpeakingEvalPage::firstEvaluationClassId() const
{
    for (const Classroom& classroom : m_evaluationClasses)
    {
        if (classroom.id > 0)
        {
            return classroom.id;
        }
    }

    return -1;
}

void SpeakingEvalPage::setEvaluationEditorAvailable(
    bool available
    )
{
    if (m_tabsContainer)
    {
        m_tabsContainer->setVisible(available);
    }

    if (m_emptyLabel)
    {
        m_emptyLabel->setVisible(!available);
    }

    if (m_table)
    {
        m_table->setVisible(available);
        m_table->setEnabled(available);
    }

    updateActions();
}
