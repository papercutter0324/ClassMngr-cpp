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

    auto* headerLayout =
        new QVBoxLayout;

    headerLayout->setContentsMargins(
        UiConstants::Pages::HeaderMargin,
        UiConstants::Pages::HeaderMargin,
        UiConstants::Pages::HeaderMargin,
        UiConstants::Pages::HeaderMargin
        );

    headerLayout->setSpacing(
        UiConstants::Pages::HeaderSpacing
        );

    m_titleLabel =
        new QLabel(
            tr("Speaking Evaluation"),
            this
            );

    m_titleLabel->setObjectName("pageTitle");
    m_titleLabel->setFont(
        FontManager::getUiFont(
            UiConstants::Pages::TitleFontSize,
            QFont::Bold
            )
        );

    m_subtitleLabel =
        new QLabel(
            tr("No class selected"),
            this
            );

    m_subtitleLabel->setObjectName("pageSubtitle");
    m_subtitleLabel->setFont(
        FontManager::getUiFont(
            UiConstants::Pages::SubtitleFontSize
            )
        );

    headerLayout->addWidget(m_titleLabel);
    headerLayout->addWidget(m_subtitleLabel);
    contentLayout()->addLayout(headerLayout);
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
        new UniformWidthTabWidget(
            UniformWidthTabKind::Section,
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
        tr("Create Reports"),
        tr("Export / Print Reports")
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

    m_koreanKeyboardButton =
        new TextFitPushButton(
            tr("Korean Keyboard"),
            this
            );

    m_koreanKeyboardButton->setToolTip(
        tr("Open Korean typing website")
        );

    m_koreanKeyboardButton->setSizePolicy(
        QSizePolicy::Expanding,
        QSizePolicy::Preferred
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
            &SpeakingEvalPage::exportReports
            );
    }

    connect(
        m_koreanKeyboardButton,
        &QPushButton::clicked,
        this,
        &SpeakingEvalPage::openKoreanKeyboard
        );

    connect(
        m_saveButton,
        &QPushButton::clicked,
        this,
        &SpeakingEvalPage::saveData
        );

    connect(
        m_evaluationTabs,
        &QTabWidget::currentChanged,
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

            activateEvaluation(
                currentClassIdFromTabs(m_classTabs),
                currentEvaluationNameFromTabs()
                );
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
    if (
        m_loadingEvaluation
        || m_saveMode != SaveMode::Automatic
        || !m_autosaveTimer
        || m_classroom.id <= 0
        || m_evaluationName.trimmed().isEmpty()
        )
    {
        return;
    }

    m_autosaveTimer->start();
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
    if (!m_titleLabel || !m_subtitleLabel)
    {
        return;
    }

    m_titleLabel->setText(
        m_evaluationName.trimmed().isEmpty()
            ? tr("Speaking Evaluation")
            : tr("%1 Speaking Evaluation").arg(m_evaluationName)
        );

    if (m_classroom.id <= 0)
    {
        m_subtitleLabel->setText(
            tr("No class selected")
            );
        return;
    }

    const QString sidebarName =
        sidebarClassDisplayName(
            m_services
                ? m_services->dataService()
                : nullptr,
            m_classroom.id
            );

    if (!sidebarName.isEmpty())
    {
        m_subtitleLabel->setText(
            sidebarName
            );
        return;
    }

    m_subtitleLabel->setText(
        m_classroom.name.trimmed().isEmpty()
            ? tr("Class %1").arg(m_classroom.id)
            : m_classroom.name.trimmed()
        );
}
