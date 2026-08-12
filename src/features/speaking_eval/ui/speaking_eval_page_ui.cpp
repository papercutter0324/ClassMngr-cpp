#include "speaking_eval_page_p.h"

void SpeakingEvalPage::buildUi()
{
    contentLayout()->setContentsMargins(
        UiConstants::Pages::Margin,
        UiConstants::Pages::Margin,
        UiConstants::Pages::Margin,
        0
        );

    contentLayout()->setSpacing(
        UiConstants::Pages::Spacing
        );

    m_pageHeader = new PageHeader(
        tr("Speaking Evaluation"),
        tr("No class selected"),
        this
        );
    contentLayout()->addWidget(m_pageHeader);
    contentLayout()->addSpacing(
        UiConstants::Pages::HeaderContentSpacing
        );

    m_tabsContainer =
        new QWidget(this);
    m_tabsContainer->setSizePolicy(
        QSizePolicy::Expanding,
        QSizePolicy::Maximum
        );

    auto* tabsLayout =
        new QVBoxLayout(m_tabsContainer);
    tabsLayout->setContentsMargins(0, 0, 0, 0);
    tabsLayout->setSpacing(8);

    m_classTabsContainer =
        new QWidget(m_tabsContainer);
    m_classTabsLayout =
        new QVBoxLayout(m_classTabsContainer);
    m_classTabsLayout->setContentsMargins(0, 0, 0, 0);
    m_classTabsLayout->setSpacing(0);

    tabsLayout->addWidget(m_classTabsContainer);

    m_evaluationTabs =
        new NavigationTabWidget(
            NavigationTabKind::Section,
            QStringLiteral("speakingEvalEvaluationTabBar"),
            m_tabsContainer
            );
    m_evaluationTabs->setObjectName("speakingEvalEvaluationTabs");
    m_evaluationTabs->setSizePolicy(
        QSizePolicy::Expanding,
        QSizePolicy::Maximum
        );

    for (const QString& evaluationName : evaluationNames())
    {
        auto* page =
            new QWidget(m_evaluationTabs);
        page->setProperty(
            "evaluation_name",
            evaluationName
            );

        m_evaluationTabs->addTab(
            page,
            evaluationLabel(evaluationName)
            );
    }

    tabsLayout->addWidget(m_evaluationTabs);
    contentLayout()->addWidget(m_tabsContainer);

    m_undoStack =
        new QUndoStack(this);

    m_undoStack->setUndoLimit(100);

    m_model =
        new SpeakingEvalModel(this);

    m_table =
        new SpeakingEvalTableView(this);

    m_table->setSizePolicy(
        QSizePolicy::Expanding,
        QSizePolicy::Expanding
        );

    m_table->setModel(m_model);
    m_table->setUndoStack(m_undoStack);

    auto* header =
        new SpeakingEvalHeaderView(
            Qt::Horizontal,
            m_table
            );

    m_table->setHorizontalHeader(header);

    m_delegate =
        new SpeakingEvalDelegate(m_table);

    m_table->setItemDelegate(m_delegate);

    m_emptyLabel =
        new QLabel(
            tr("No classes available"),
            this
            );
    m_emptyLabel->setObjectName("pageSubtitle");
    m_emptyLabel->setAlignment(Qt::AlignCenter);
    m_emptyLabel->setFont(
        FontManager::getUiFont(12)
        );

    contentLayout()->addWidget(m_emptyLabel);
    contentLayout()->addWidget(m_table);

    setupTable();

    clearLayout(
        bottomLayout()
        );

    bottomLayout()->addStretch();

    m_importNamesButton =
        new TextFitPushButton(
            tr("Import Names"),
            this
            );

    m_importNamesButton->setSizePolicy(
        QSizePolicy::Expanding,
        QSizePolicy::Preferred
        );

    bottomLayout()->addWidget(m_importNamesButton);
    bottomLayout()->addSpacing(20);

    const QList<QString> reportLabels{
        tr("Report Editor"),
        tr("Generate Comments")
    };

    m_reportButtons.clear();

    for (int index = 0; index < reportLabels.size(); ++index)
    {
        auto* button =
            new TextFitPushButton(
                reportLabels[index],
                this
                );

        button->setSizePolicy(
            QSizePolicy::Expanding,
            QSizePolicy::Preferred
            );

        bottomLayout()->addWidget(button);
        m_reportButtons.append(button);

        if (index == 1)
        {
            bottomLayout()->addSpacing(20);
        }
    }

    bottomLayout()->addSpacing(20);

    m_koreanKeyboardButton = new QPushButton(this);
    m_koreanKeyboardButton->setObjectName(
        QStringLiteral("speakingEvalKoreanKeyboardButton")
        );
    m_koreanKeyboardButton->setMinimumSize(44, 40);
    m_koreanKeyboardButton->setMaximumWidth(52);

    m_koreanKeyboardButton->setToolTip(
        tr("Open Korean / English on-screen keyboard")
        );
    m_koreanKeyboardButton->setAccessibleName(
        tr("Korean Keyboard")
        );
    m_onScreenKeyboard = new OnScreenKeyboard(this);
    m_onScreenKeyboard->setTriggerButton(
        m_koreanKeyboardButton
        );

    bottomLayout()->addWidget(m_koreanKeyboardButton);

    m_saveButton =
        new TextFitPushButton(
            tr("Save Changes"),
            this
            );

    m_saveButton->setSizePolicy(
        QSizePolicy::Expanding,
        QSizePolicy::Preferred
        );

    bottomLayout()->addWidget(m_saveButton);
    bottomLayout()->addStretch();

    connect(
        m_importNamesButton,
        &QPushButton::clicked,
        this,
        &SpeakingEvalPage::importNames
        );

    if (!m_reportButtons.isEmpty())
    {
        connect(
            m_reportButtons.at(0),
            &QPushButton::clicked,
            this,
            &SpeakingEvalPage::showReports
            );
    }
    if (m_reportButtons.size() > 1)
    {
        connect(
            m_reportButtons.at(1),
            &QPushButton::clicked,
            this,
            &SpeakingEvalPage::generateClassAiComments
            );
    }

    connect(
        m_koreanKeyboardButton,
        &QPushButton::clicked,
        this,
        &SpeakingEvalPage::openKoreanKeyboard
        );

    connect(
        m_evaluationTabs,
        &NavigationTabWidget::currentChanged,
        this,
        [this](int)
        {
            if (
                m_rebuildingClassTabs
                || m_restoringClassTabs
                || m_syncingEvaluationTabs
                )
            {
                return;
            }

            const int selectedClassId =
                currentClassIdFromTabs(m_classTabs);
            activateEvaluation(
                selectedClassId > 0
                    ? selectedClassId
                    : m_classroom.id,
                currentEvaluationNameFromTabs()
                );
        }
        );

    connect(
        m_table->selectionModel(),
        &QItemSelectionModel::currentChanged,
        this,
        [this](const QModelIndex&, const QModelIndex&)
        {
            if (
                m_onScreenKeyboard
                && m_onScreenKeyboard->isVisible()
                )
            {
                QTimer::singleShot(
                    0,
                    this,
                    [this]()
                    {
                        if (m_onScreenKeyboard)
                        {
                            m_onScreenKeyboard->retarget(m_table);
                        }
                    }
                    );
            }
        }
        );

    connect(
        m_model,
        &SpeakingEvalModel::dirtyChanged,
        this,
        &SpeakingEvalPage::updateActions
        );

    connect(
        m_model,
        &SpeakingEvalModel::dataModified,
        this,
        [this]()
        {
            updateActions();
            scheduleAutosave();
        }
        );

    connect(
        m_model,
        &QAbstractItemModel::dataChanged,
        this,
        [this](
            const QModelIndex& topLeft,
            const QModelIndex& bottomRight,
            const QList<int>&
            )
        {
            handleNameCellChanged(
                topLeft,
                bottomRight
                );
        }
        );

    setEvaluationEditorAvailable(false);
}

void SpeakingEvalPage::scheduleAutosave()
{
    m_autosave->setSaveAvailable(
        m_classroom.id > 0
        && !m_evaluationName.trimmed().isEmpty()
        );
    m_autosave->setValid(!m_model || !m_model->hasErrors());
    m_autosave->setDirty(m_model && m_model->isDirty());
}

void SpeakingEvalPage::setupTable()
{
    if (!m_table)
    {
        return;
    }

    m_table->setShowGrid(false);

    for (int column = 0; column < SpeakingEval::ColumnCount; ++column)
    {
        m_table->setColumnWidth(
            column,
            SpeakingEval::columnWidth(
                SpeakingEval::columnFromInt(column)
                )
            );
    }

    for (int row = 0; row < SpeakingEval::RowCount; ++row)
    {
        m_table->setRowHeight(
            row,
            SpeakingEval::RowHeight
            );
    }
}

void SpeakingEvalPage::updateHeaderText()
{
    if (!m_pageHeader)
    {
        return;
    }

    m_pageHeader->setTitle(
        m_evaluationName.trimmed().isEmpty()
            ? tr("Speaking Evaluation")
            : tr("%1 Speaking Evaluation").arg(m_evaluationName)
        );

    if (m_classroom.id <= 0)
    {
        m_pageHeader->setSubtitle(
            tr("No class selected")
            );
        return;
    }

    const QString sidebarName =
        sidebarClassDisplayName(
            m_services
                ? m_services->classService()
                : nullptr,
            m_services
                ? m_services->teacherService()
                : nullptr,
            m_classroom.id
            );

    if (!sidebarName.isEmpty())
    {
        m_pageHeader->setSubtitle(
            sidebarName
            );
        return;
    }

    m_pageHeader->setSubtitle(
        m_classroom.name.trimmed().isEmpty()
            ? tr("Class %1").arg(m_classroom.id)
            : m_classroom.name.trimmed()
        );
}
