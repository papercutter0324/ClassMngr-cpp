#include "sub_prep_page_p.h"

void SubPrepPage::buildUi()
{
    contentLayout()->setContentsMargins(0, 0, 0, 0);
    contentLayout()->setSpacing(0);

    m_scrollArea =
        new QScrollArea(this);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setFrameShape(QFrame::NoFrame);
    m_scrollArea->setHorizontalScrollBarPolicy(
        Qt::ScrollBarAsNeeded
        );
    m_scrollArea->setVerticalScrollBarPolicy(
        Qt::ScrollBarAsNeeded
        );

    m_scrollContent =
        new QWidget(m_scrollArea);
    m_scrollContentLayout =
        new QVBoxLayout(m_scrollContent);
    m_scrollContentLayout->setContentsMargins(
        UiConstants::Pages::Margin,
        UiConstants::Pages::Margin,
        UiConstants::Pages::Margin,
        UiConstants::Pages::Margin
        );
    m_scrollContentLayout->setSpacing(
        UiConstants::ClassInfo::Page::ContentSpacing
        );
    m_scrollContentLayout->setAlignment(Qt::AlignTop);

    m_scrollArea->setWidget(m_scrollContent);
    contentLayout()->addWidget(m_scrollArea);

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
            tr("Sub Prep"),
            m_scrollContent
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
            tr("Prepare substitute materials and class notes."),
            m_scrollContent
            );
    m_subtitleLabel->setObjectName("pageSubtitle");
    m_subtitleLabel->setFont(
        FontManager::getUiFont(
            UiConstants::Pages::SubtitleFontSize
            )
        );

    auto* titleRow =
        new QHBoxLayout;
    titleRow->setContentsMargins(0, 0, 0, 0);
    titleRow->setSpacing(UiConstants::Pages::HeaderSpacing);
    titleRow->addWidget(m_titleLabel);
    titleRow->addStretch();

    m_printButton =
        new TextFitPushButton(
            tr("Generate Sub Prep"),
            m_scrollContent
            );
    m_printButton->setObjectName(
        QStringLiteral("subPrepPrintButton")
        );
    m_printButton->setMinimumWidth(130);
    m_printButton->setToolTip(
        tr("Create a dated Sub Prep package with by-day rosters and optional paper copies.")
        );
    titleRow->addWidget(m_printButton);

    headerLayout->addLayout(titleRow);
    headerLayout->addWidget(m_subtitleLabel);
    m_scrollContentLayout->addLayout(headerLayout);
    m_scrollContentLayout->addSpacing(
        UiConstants::Pages::HeaderContentSpacing
        );

    m_importantInformationHeading =
        createTopLevelHeading(
            tr("Important Information"),
            m_scrollContent
            );
    m_importantInformationHeading->setObjectName(
        QStringLiteral("subPrepImportantInformationHeading")
        );
    m_scrollContentLayout->addWidget(
        m_importantInformationHeading
        );

    m_campusCard =
        new SectionCard(
            tr("Campus Information"),
            m_scrollContent
            );
    m_campusCard->setProperty(
        "subPrepSection",
        QStringLiteral("campus")
        );

    auto* campusGrid =
        new QGridLayout;
    campusGrid->setHorizontalSpacing(
        UiConstants::ClassInfo::Form::HorizontalSpacing
        );
    campusGrid->setVerticalSpacing(
        UiConstants::ClassInfo::Form::VerticalSpacing
        );

    m_officeNumberEdit =
        new QLineEdit(m_campusCard);
    m_officeWifiEdit =
        new QLineEdit(m_campusCard);
    m_officeWifiPasswordEdit =
        new QLineEdit(m_campusCard);
    m_photocopierCodeEdit =
        new QLineEdit(m_campusCard);

    const QList<QLineEdit*> campusEdits{
        m_officeNumberEdit,
        m_officeWifiEdit,
        m_officeWifiPasswordEdit,
        m_photocopierCodeEdit
    };

    for (QLineEdit* edit : campusEdits)
    {
        edit->setReadOnly(true);
    }

    WidgetSizing::installTextAwareFieldWidth(
        m_officeNumberEdit,
        OfficeNumberFieldWidth
        );

    for (QLineEdit* edit : {
             m_officeWifiEdit,
             m_officeWifiPasswordEdit,
             m_photocopierCodeEdit
             })
    {
        WidgetSizing::installTextAwareFieldWidth(
            edit,
            CompactFieldWidth
            );
    }

    m_officeNumberLabel =
        createFieldLabel(tr("Office Number"), m_campusCard);
    m_officeWifiLabel =
        createFieldLabel(tr("Office WiFi"), m_campusCard);
    m_officeWifiPasswordLabel =
        createFieldLabel(tr("WiFi Password"), m_campusCard);
    m_photocopierCodeLabel =
        createFieldLabel(tr("Photocopier Code"), m_campusCard);

    const QList<QLabel*> campusLabels{
        m_officeNumberLabel,
        m_officeWifiLabel,
        m_officeWifiPasswordLabel,
        m_photocopierCodeLabel
    };

    for (int column = 0; column < campusLabels.size(); ++column)
    {
        campusGrid->addWidget(
            campusLabels.at(column),
            0,
            column,
            Qt::AlignLeft
            );
        campusGrid->addWidget(
            campusEdits.at(column),
            1,
            column
            );
        campusGrid->setColumnStretch(column, 1);
    }

    m_campusCard->contentLayout()->addLayout(campusGrid);
    m_scrollContentLayout->addWidget(m_campusCard);

    m_zoomCard =
        new SectionCard(
            tr("Personal Zoom Information"),
            m_scrollContent
            );
    m_zoomCard->setProperty(
        "subPrepSection",
        QStringLiteral("zoom")
        );

    auto* zoomGrid =
        new QGridLayout;
    zoomGrid->setHorizontalSpacing(
        UiConstants::ClassInfo::Form::HorizontalSpacing
        );
    zoomGrid->setVerticalSpacing(
        UiConstants::ClassInfo::Form::VerticalSpacing
        );

    m_zoomLoginIdEdit =
        new QLineEdit(m_zoomCard);
    m_zoomLoginIdEdit->setObjectName(
        QStringLiteral("subPrepZoomLoginIdEdit")
        );
    m_zoomPasswordEdit =
        new QLineEdit(m_zoomCard);
    m_zoomPasswordEdit->setObjectName(
        QStringLiteral("subPrepZoomPasswordEdit")
        );
    m_zoomLoginIdEdit->setReadOnly(true);
    m_zoomPasswordEdit->setReadOnly(true);

    WidgetSizing::installTextAwareFieldWidth(
        m_zoomLoginIdEdit,
        CompactFieldWidth
        );
    WidgetSizing::installTextAwareFieldWidth(
        m_zoomPasswordEdit,
        CompactFieldWidth
        );

    m_zoomLoginIdLabel =
        createFieldLabel(tr("Zoom Login ID"), m_zoomCard);
    m_zoomPasswordLabel =
        createFieldLabel(tr("Zoom Password"), m_zoomCard);

    zoomGrid->addWidget(
        m_zoomLoginIdLabel,
        0,
        0,
        Qt::AlignLeft
        );
    zoomGrid->addWidget(
        m_zoomPasswordLabel,
        0,
        1,
        Qt::AlignLeft
        );
    zoomGrid->addWidget(m_zoomLoginIdEdit, 1, 0);
    zoomGrid->addWidget(m_zoomPasswordEdit, 1, 1);
    zoomGrid->setColumnStretch(0, 1);
    zoomGrid->setColumnStretch(1, 1);

    m_zoomCard->contentLayout()->addLayout(zoomGrid);
    m_scrollContentLayout->addWidget(m_zoomCard);

    m_materialsCard =
        new SectionCard(
            tr("Class Materials"),
            m_scrollContent
            );
    m_materialsCard->setProperty(
        "subPrepSection",
        QStringLiteral("materials")
        );
    m_classMaterialsEdit =
        createTextEdit(6, false, m_materialsCard);
    m_classMaterialsEdit->setObjectName(
        QStringLiteral("subPrepClassMaterialsEdit")
        );
    m_materialsCard->contentLayout()->addWidget(
        m_classMaterialsEdit
        );
    m_scrollContentLayout->addWidget(m_materialsCard);

    m_gradingCard =
        new SectionCard(
            tr("Book Report Grading"),
            m_scrollContent
            );
    m_gradingCard->setProperty(
        "subPrepSection",
        QStringLiteral("grading")
        );

    m_gradingInstructionsLabel =
        createFieldLabel(
            tr("Grading Instructions"),
            m_gradingCard
            );
    m_gradingCard->contentLayout()->addWidget(
        m_gradingInstructionsLabel
        );

    m_gradingInstructionsEdit =
        createTextEdit(5, false, m_gradingCard);
    m_gradingInstructionsEdit->setObjectName(
        QStringLiteral("subPrepGradingInstructionsEdit")
        );
    m_gradingInstructionsEdit->installEventFilter(this);
    m_gradingCard->contentLayout()->addWidget(
        m_gradingInstructionsEdit
        );

    m_specialInstructionsLabel =
        createFieldLabel(
            tr("Special Instructions"),
            m_gradingCard
            );
    m_gradingCard->contentLayout()->addWidget(
        m_specialInstructionsLabel
        );

    m_specialInstructionsEdit =
        createTextEdit(4, false, m_gradingCard);
    m_specialInstructionsEdit->setObjectName(
        QStringLiteral("subPrepSpecialInstructionsEdit")
        );
    m_gradingCard->contentLayout()->addWidget(
        m_specialInstructionsEdit
        );
    m_scrollContentLayout->addWidget(m_gradingCard);

    m_scheduleHeading =
        createTopLevelHeading(
            tr("Schedule"),
            m_scrollContent
            );
    m_scheduleHeading->setObjectName(
        QStringLiteral("subPrepScheduleHeading")
        );
    m_scrollContentLayout->addWidget(m_scheduleHeading);

    m_scheduleCard =
        new SectionCard(
            QString(),
            m_scrollContent
            );
    m_scheduleCard->setProperty(
        "subPrepSection",
        QStringLiteral("schedule")
        );
    m_scheduleWidget =
        new ScheduleWidget(
            m_services,
            m_scheduleCard,
            ScheduleMode::ReadOnly
            );
    m_scheduleWidget->setObjectName(
        QStringLiteral("subPrepScheduleWidget")
        );
    m_scheduleCard->contentLayout()->addWidget(
        m_scheduleWidget
        );
    m_scrollContentLayout->addWidget(m_scheduleCard);

    m_classInformationHeading =
        createTopLevelHeading(
            tr("Class Information"),
            m_scrollContent
            );
    m_classInformationHeading->setObjectName(
        QStringLiteral("subPrepClassInformationHeading")
        );
    m_scrollContentLayout->addWidget(m_classInformationHeading);

    m_classInformationContent =
        new QWidget(m_scrollContent);
    m_classInformationContent->setProperty(
        "subPrepSection",
        QStringLiteral("class_information")
        );
    m_classInformationContent->setObjectName(
        QStringLiteral("subPrepClassInformationContent")
        );
    m_classInformationLayout =
        new QVBoxLayout(m_classInformationContent);
    m_classInformationLayout->setContentsMargins(0, 0, 0, 0);
    m_classInformationLayout->setSpacing(
        UiConstants::ClassInfo::Page::ContentSpacing
        );
    m_classInformationLayout->setAlignment(Qt::AlignTop);
    m_scrollContentLayout->addWidget(
        m_classInformationContent
        );

    m_subNotesHeading =
        createTopLevelHeading(
            tr("Additional Notes"),
            m_scrollContent
            );
    m_scrollContentLayout->addWidget(
        m_subNotesHeading
        );

    m_notesCard =
        new SectionCard(
            tr("Notes"),
            m_scrollContent
            );
    m_notesCard->setProperty(
        "subPrepSection",
        QStringLiteral("sub_notes")
        );
    m_subNotesEdit =
        createTextEdit(10, false, m_notesCard);
    m_subNotesEdit->setObjectName(
        QStringLiteral("subPrepNotesEdit")
        );
    m_notesCard->contentLayout()->addWidget(
        m_subNotesEdit
        );
    m_scrollContentLayout->addWidget(m_notesCard);

    connect(
        m_classMaterialsEdit,
        &QTextEdit::textChanged,
        this,
        &SubPrepPage::handleEditableChanged
        );
    connect(
        m_gradingInstructionsEdit,
        &QTextEdit::textChanged,
        this,
        &SubPrepPage::handleEditableChanged
        );
    connect(
        m_specialInstructionsEdit,
        &QTextEdit::textChanged,
        this,
        &SubPrepPage::handleEditableChanged
        );
    connect(
        m_subNotesEdit,
        &QTextEdit::textChanged,
        this,
        &SubPrepPage::handleEditableChanged
        );
    connect(
        m_printButton,
        &QPushButton::clicked,
        this,
        &SubPrepPage::generateSubPrep
        );
}

