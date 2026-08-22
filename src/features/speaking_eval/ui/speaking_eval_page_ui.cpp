#include "speaking_eval_page_p.h"

#include "domain/validation/speaking_eval_validator.h"
#include "ui/shared/validation/form_validation_binder.h"

void SpeakingEvalPage::buildUi()
{
    const int pageMargin = m_embedded ? 0 : UiConstants::Pages::Margin;
    contentLayout()->setContentsMargins(
        pageMargin,
        pageMargin,
        pageMargin,
        0
        );

    contentLayout()->setSpacing(
        m_embedded
            ? 12
            : UiConstants::Pages::Spacing
        );

    if (!m_embedded)
    {
        m_pageHeader = new PageHeader(
            tr("Speaking Evaluation"),
            tr("No class selected"),
            this
            );
        contentLayout()->addWidget(m_pageHeader);
        contentLayout()->addSpacing(
            UiConstants::Pages::HeaderContentSpacing
            );
    }
    else
    {
        auto* topBar = new QWidget(this);
        auto* topLayout = new QHBoxLayout(topBar);
        topLayout->setContentsMargins(0, 0, 0, 0);
        topLayout->setSpacing(10);

        m_embeddedHeading = new QLabel(topBar);
        m_embeddedHeading->setObjectName(
            QStringLiteral("classEvaluationsHeading")
            );
        m_embeddedHeading->setFont(
            FontManager::getUiFont(18, QFont::DemiBold)
            );
        topLayout->addWidget(m_embeddedHeading);
        topLayout->addStretch();

        m_embeddedEvaluationLabel = new QLabel(topBar);
        m_embeddedEvaluationLabel->setObjectName(
            QStringLiteral("classEvaluationsEvaluationLabel")
            );
        m_embeddedEvaluationLabel->setFont(FontManager::getUiFont(12));
        topLayout->addWidget(m_embeddedEvaluationLabel);

        m_embeddedEvaluationCombo = new QComboBox(topBar);
        m_embeddedEvaluationCombo->setObjectName(
            QStringLiteral("classEvaluationsEvaluationCombo")
            );
        m_embeddedEvaluationCombo->setFont(FontManager::getUiFont(12));
        m_embeddedEvaluationCombo->setMinimumWidth(170);

        for (const QString& evaluationName : evaluationNames())
        {
            m_embeddedEvaluationCombo->addItem(
                evaluationLabel(evaluationName),
                evaluationName
                );
        }

        topLayout->addWidget(m_embeddedEvaluationCombo);
        contentLayout()->addWidget(topBar);
    }

    m_tabsContainer =
        new QWidget(this);
    m_tabsContainer->setVisible(!m_embedded);
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
    if (m_embedded)
    {
        m_evaluationTabs->setObjectName("classEvaluationsEvaluationTabs");
    }
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
    if (!m_embedded)
    {
        contentLayout()->addWidget(m_tabsContainer);
    }

    m_undoStack =
        new QUndoStack(this);

    m_undoStack->setUndoLimit(100);

    m_model =
        new SpeakingEvalModel(this);

    m_table =
        new SpeakingEvalTableView(this);
    if (m_embedded)
    {
        m_table->setObjectName("classEvaluationsTable");
    }

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

    m_validationBinder = new FormValidationBinder(m_autosave, nullptr, this);
    m_validationMessage = m_validationBinder->createMessageLabel(this);
    m_validationMessage->setObjectName(
        QStringLiteral("speakingEvalValidationMessage")
        );
    m_validationBinder->registerFieldPrefix(
        QStringLiteral("rows["),
        m_table,
        m_validationMessage
        );

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
    contentLayout()->addWidget(m_validationMessage);

    setupTable();

    clearLayout(bottomLayout());

    if (!m_embedded)
    {
        bottomLayout()->addStretch();
    }

    m_importNamesButton =
        new TextFitPushButton(
            tr("Import Names"),
            this
            );
    if (m_embedded)
    {
        m_importNamesButton->setObjectName(
            "classEvaluationsImportNamesButton");
    }

    m_importNamesButton->setSizePolicy(
        m_embedded
            ? QSizePolicy::Maximum
            : QSizePolicy::Expanding,
        QSizePolicy::Preferred
        );

    bottomLayout()->addWidget(m_importNamesButton);

    if (m_embedded)
    {
        bottomLayout()->addStretch();
    }
    else
    {
        bottomLayout()->addSpacing(20);
    }

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
            m_embedded
                ? QSizePolicy::Maximum
                : QSizePolicy::Expanding,
            QSizePolicy::Preferred
            );

        bottomLayout()->addWidget(button);
        if (m_embedded)
        {
            button->setObjectName(
                index == 0
                    ? "classEvaluationsReportEditorButton"
                    : "classEvaluationsGenerateCommentsButton"
                );
        }
        m_reportButtons.append(button);

        if (index == 1 && !m_embedded)
        {
            bottomLayout()->addSpacing(20);
        }
    }

    if (!m_embedded)
    {
        bottomLayout()->addSpacing(20);
    }

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
    m_koreanKeyboardButton->setVisible(!m_embedded);

    m_saveButton =
        new TextFitPushButton(
            tr("Save Changes"),
            this
            );
    m_saveButton->setObjectName(QStringLiteral("speakingEvalSaveButton"));

    m_saveButton->setSizePolicy(
        QSizePolicy::Expanding,
        QSizePolicy::Preferred
        );

    bottomLayout()->addWidget(m_saveButton);
    if (!m_embedded)
    {
        bottomLayout()->addStretch();
    }

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

    if (m_embeddedEvaluationCombo)
    {
        connect(
            m_embeddedEvaluationCombo,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this,
            [this](int index)
            {
                if (m_evaluationTabs)
                {
                    m_evaluationTabs->setCurrentIndex(index);
                }
            }
            );
    }

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
            if (m_updatingValidation)
            {
                return;
            }

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
    updateEvaluationValidation();
    m_autosave->setSaveAvailable(
        m_classroom.id > 0
        && !m_evaluationName.trimmed().isEmpty()
        );
    m_autosave->setDirty(m_model && m_model->isDirty());
}

void SpeakingEvalPage::updateEvaluationValidation()
{
    if (!m_validationBinder || !m_model || m_updatingValidation)
    {
        return;
    }

    const SpeakingEvalRows rows = SpeakingEvalValidator::normalized(m_model->rows());
    const ValidationResult validation = SpeakingEvalValidator::validate(
        m_classroom.id,
        m_evaluationName,
        rows
        );

    m_updatingValidation = true;
    m_validationBinder->setValidation(
        validation,
        [](const ValidationIssue& issue)
        {
            return issue.field.startsWith(QStringLiteral("rows["))
                ? QObject::tr("Correct the highlighted evaluation cells.")
                : QString();
        }
        );
    m_model->setDomainValidation(validation);
    m_updatingValidation = false;

    if (m_table)
    {
        m_table->viewport()->update();
    }
}

void SpeakingEvalPage::focusFirstEvaluationError()
{
    if (!m_validationBinder)
    {
        return;
    }

    for (const ValidationIssue& issue : m_validationBinder->validation().errors())
    {
        if (issue.row >= 0
            && issue.row < SpeakingEval::RowCount
            && issue.column >= 0
            && issue.column < SpeakingEval::ColumnCount)
        {
            selectEvaluationCell(
                issue.row,
                SpeakingEval::columnFromInt(issue.column)
                );
            return;
        }
    }

    m_validationBinder->focusFirstError();
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
    if (m_embeddedHeading)
    {
        m_embeddedHeading->setText(tr("Speaking Evaluations"));
    }

    if (m_embeddedEvaluationLabel)
    {
        m_embeddedEvaluationLabel->setText(tr("Evaluation"));
    }

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
