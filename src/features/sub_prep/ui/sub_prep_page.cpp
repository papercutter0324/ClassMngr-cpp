#include "sub_prep_page.h"

#include "core/application_services.h"
#include "core/fontmanager.h"
#include "core/resource_paths.h"
#include "data/data_service.h"
#include "features/campus/data/campus_json_repository.h"
#include "features/sub_prep/ui/sub_prep_class_information_model.h"
#include "features/sub_prep/ui/sub_prep_print_service.h"
#include "ui/shared/constants/gui_constants.h"
#include "ui/shared/styles/roles.h"
#include "ui/shared/utils/widget_sizing.h"
#include "ui/shared/widgets/sectioncards/class_info_section_card.h"
#include "ui/shared/widgets/sections/schedule_section_widget.h"
#include "ui/shared/widgets/text_fit_push_button.h"

#include <algorithm>
#include <utility>

#include <QEvent>
#include <QFont>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLayout>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QShowEvent>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QTextEdit>
#include <QTimer>
#include <QVariant>
#include <QVBoxLayout>
#include <QtAssert>

namespace
{
constexpr int AutosaveDelayMs = 750;
constexpr int OfficeNumberFieldWidth = 115;
constexpr int CompactFieldWidth = 170;
constexpr int TextEditVerticalPadding = 24;
constexpr int ClassNotesLines = 4;
constexpr int TeacherNotesLines = 4;

const QString NotAvailableText =
    QStringLiteral("N/A");

namespace SettingsKeys
{
const QString MyInfoCampus =
    QStringLiteral("myInfo/campus");
const QString MyInfoZoomLoginId =
    QStringLiteral("myInfo/zoomLoginId");
const QString MyInfoZoomPassword =
    QStringLiteral("myInfo/zoomPassword");
const QString MyInfoZoomNotAvailable =
    QStringLiteral("myInfo/zoomNotAvailable");
const QString LegacyZoomLoginId =
    QStringLiteral("subPrep/personalZoomEmail");
const QString LegacyZoomPassword =
    QStringLiteral("subPrep/personalZoomPassword");
const QString LegacyZoomNotAvailable =
    QStringLiteral("subPrep/personalZoomNotAvailable");
const QString ClassMaterials =
    QStringLiteral("subPrep/classMaterials");
const QString BookReportGrading =
    QStringLiteral("subPrep/bookReportGrading");
const QString BookReportSpecialInstructions =
    QStringLiteral("subPrep/bookReportSpecialInstructions");
const QString SubNotes =
    QStringLiteral("subPrep/subComments");
}

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

CampusJsonRepository campusRepository()
{
    return CampusJsonRepository(
        ResourcePaths::Campuses::directory()
        );
}

QString campusDisplayName(
    const CampusInfo& campus
    )
{
    return campus.campusName.trimmed().isEmpty()
        ? campus.id.trimmed()
        : campus.campusName.trimmed();
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

QVariant loadSettingWithLegacyFallback(
    DataService* dataService,
    const QString& primaryKey,
    const QString& legacyKey,
    const QVariant& defaultValue
    )
{
    if (!dataService)
    {
        return defaultValue;
    }

    QVariant value =
        dataService->loadSetting(
            primaryKey,
            QVariant()
            );

    if (value.isValid())
    {
        return value;
    }

    value =
        dataService->loadSetting(
            legacyKey,
            QVariant()
            );

    if (!value.isValid())
    {
        return defaultValue;
    }

    dataService->saveSetting(
        primaryKey,
        value
        );

    return value;
}

int textEditHeightForLines(
    const QTextEdit* edit,
    int lines
    )
{
    if (!edit)
    {
        return 0;
    }

    return edit->fontMetrics().lineSpacing() * lines
        + TextEditVerticalPadding;
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

QLabel* createInlineValue(
    const QString& label,
    const QString& value,
    QWidget* parent
    )
{
    auto* field =
        new QLabel(
            QStringLiteral("%1: %2")
                .arg(
                    label,
                    valueOrNa(value)
                    ),
            parent
            );

    field->setTextInteractionFlags(
        Qt::TextSelectableByMouse
        | Qt::TextSelectableByKeyboard
        );
    field->setWordWrap(true);
    field->setSizePolicy(
        QSizePolicy::Expanding,
        QSizePolicy::Preferred
        );

    return field;
}

QFrame* createSeparator(
    QWidget* parent
    )
{
    auto* separator =
        new QFrame(parent);
    separator->setFrameShape(QFrame::HLine);
    separator->setFrameShadow(QFrame::Sunken);
    separator->setProperty(
        "role",
        UiRoles::Separator
        );

    return separator;
}
}

SubPrepPage::SubPrepPage(
    ApplicationServices* services,
    QWidget* parent
    )
    : BasePage(parent)
    , m_services(services)
{
    Q_ASSERT(m_services);

    setProperty("role", UiRoles::SubPrep);

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
        &SubPrepPage::autosave
        );

    loadPageData();
}

void SubPrepPage::saveData()
{
    saveSubPrepInternal();
}

bool SubPrepPage::saveChanges()
{
    if (m_autosaveTimer)
    {
        m_autosaveTimer->stop();
    }

    return saveSubPrepInternal();
}

bool SubPrepPage::hasUnsavedChanges() const
{
    return m_dirty;
}

void SubPrepPage::discardChanges()
{
    if (m_autosaveTimer)
    {
        m_autosaveTimer->stop();
    }

    loadPageData();
}

void SubPrepPage::refresh()
{
    BasePage::refresh();

    if (!isVisible())
    {
        return;
    }

    loadPersonalZoomInformation();
    loadCampuses();
    refreshGeneratedContent();

    if (!m_dirty)
    {
        loadStoredSettings();
    }
}

void SubPrepPage::retranslateUi()
{
    if (m_titleLabel)
    {
        m_titleLabel->setText(
            tr("Sub Prep")
            );
    }

    if (m_subtitleLabel)
    {
        m_subtitleLabel->setText(
            tr("Prepare substitute materials and class notes.")
            );
    }

    if (m_printButton)
    {
        m_printButton->setText(
            tr("Print Sub Prep")
            );
        m_printButton->setToolTip(
            tr("Print all sub prep information as an A4 PDF.")
            );
    }

    if (m_importantInformationHeading)
    {
        m_importantInformationHeading->setText(
            tr("Important Information")
            );
    }

    if (m_subNotesHeading)
    {
        m_subNotesHeading->setText(
            tr("Sub Notes")
            );
    }

    if (m_campusCard)
    {
        m_campusCard->setTitle(
            tr("Campus Information")
            );
    }

    if (m_zoomCard)
    {
        m_zoomCard->setTitle(
            tr("Personal Zoom Information")
            );
    }

    if (m_materialsCard)
    {
        m_materialsCard->setTitle(
            tr("Class Materials")
            );
    }

    if (m_gradingCard)
    {
        m_gradingCard->setTitle(
            tr("Book Report Grading")
            );
    }

    if (m_scheduleHeading)
    {
        m_scheduleHeading->setText(
            tr("Schedule")
            );
    }

    if (m_classInformationHeading)
    {
        m_classInformationHeading->setText(
            tr("Class Information")
            );
    }

    if (m_notesCard)
    {
        m_notesCard->setTitle(
            tr("Notes")
            );
    }

    if (m_officeNumberLabel)
    {
        m_officeNumberLabel->setText(
            tr("Office Number")
            );
    }

    if (m_officeWifiLabel)
    {
        m_officeWifiLabel->setText(
            tr("Office WiFi")
            );
    }

    if (m_officeWifiPasswordLabel)
    {
        m_officeWifiPasswordLabel->setText(
            tr("WiFi Password")
            );
    }

    if (m_photocopierCodeLabel)
    {
        m_photocopierCodeLabel->setText(
            tr("Photocopier Code")
            );
    }

    if (m_zoomLoginIdLabel)
    {
        m_zoomLoginIdLabel->setText(
            tr("Zoom Login ID")
            );
    }

    if (m_zoomPasswordLabel)
    {
        m_zoomPasswordLabel->setText(
            tr("Zoom Password")
            );
    }

    if (m_gradingInstructionsLabel)
    {
        m_gradingInstructionsLabel->setText(
            tr("Grading Instructions")
            );
    }

    if (m_specialInstructionsLabel)
    {
        m_specialInstructionsLabel->setText(
            tr("Special Instructions")
            );
    }

    if (m_scheduleWidget)
    {
        m_scheduleWidget->retranslateUi();
    }

    rebuildClassInformation();
}

void SubPrepPage::scrollToSection(
    SubPrepSection section
    )
{
    m_currentSection = section;

    QWidget* target = nullptr;

    switch (section)
    {
    case SubPrepSection::ImportantInformation:
        target = m_importantInformationHeading;
        break;

    case SubPrepSection::SubNotes:
        target = m_subNotesHeading;
        break;
    }

    if (!m_scrollArea || !target)
    {
        return;
    }

    QTimer::singleShot(
        0,
        this,
        [this, target]()
        {
            if (!m_scrollArea || !target)
            {
                return;
            }

            m_scrollArea->ensureWidgetVisible(
                target,
                0,
                0
                );

            if (auto* scrollBar = m_scrollArea->verticalScrollBar())
            {
                scrollBar->setValue(
                    target->y()
                    );
            }
        }
        );
}

void SubPrepPage::scrollToTop()
{
    m_currentSection =
        SubPrepSection::ImportantInformation;

    QTimer::singleShot(
        0,
        this,
        [this]()
        {
            if (auto* scrollBar =
                    m_scrollArea
                        ? m_scrollArea->verticalScrollBar()
                        : nullptr)
            {
                scrollBar->setValue(
                    scrollBar->minimum()
                    );
            }
        }
        );
}

QString SubPrepPage::currentSectionName() const
{
    switch (m_currentSection)
    {
    case SubPrepSection::ImportantInformation:
        return tr("Important Information");

    case SubPrepSection::SubNotes:
        return tr("Sub Notes");
    }

    return {};
}

QString SubPrepPage::currentSectionKey() const
{
    switch (m_currentSection)
    {
    case SubPrepSection::ImportantInformation:
        return QStringLiteral("sub_prep_important");

    case SubPrepSection::SubNotes:
        return QStringLiteral("sub_prep_notes");
    }

    return {};
}

void SubPrepPage::showEvent(
    QShowEvent* event
    )
{
    BasePage::showEvent(event);

    loadPersonalZoomInformation();
    loadCampuses();
    refreshGeneratedContent();

    if (!m_dirty)
    {
        loadStoredSettings();
    }
}

bool SubPrepPage::eventFilter(
    QObject* watched,
    QEvent* event
    )
{
    if (
        watched == m_gradingInstructionsEdit
        && event
        && event->type() == QEvent::FocusOut
        && restoreGradingDefaultIfNeeded()
        )
    {
        handleEditableChanged();
    }

    return BasePage::eventFilter(
        watched,
        event
        );
}

void SubPrepPage::handleEditableChanged()
{
    if (m_loading)
    {
        return;
    }

    m_dirty = true;

    if (m_autosaveTimer)
    {
        m_autosaveTimer->start();
    }
}

void SubPrepPage::autosave()
{
    if (m_dirty)
    {
        saveSubPrepInternal();
    }
}

void SubPrepPage::printSubPrep()
{
    if (hasUnsavedChanges() && !saveChanges())
    {
        return;
    }

    refreshGeneratedContent();

    SubPrepPrintService::Request request;
    request.parent = this;
    request.campus = {
        m_officeNumberEdit ? m_officeNumberEdit->text() : QString(),
        m_officeWifiEdit ? m_officeWifiEdit->text() : QString(),
        m_officeWifiPasswordEdit ? m_officeWifiPasswordEdit->text() : QString(),
        m_photocopierCodeEdit ? m_photocopierCodeEdit->text() : QString()
    };
    request.zoom = {
        m_zoomLoginIdEdit ? m_zoomLoginIdEdit->text() : QString(),
        m_zoomPasswordEdit ? m_zoomPasswordEdit->text() : QString()
    };
    request.classMaterials =
        m_classMaterialsEdit
            ? m_classMaterialsEdit->toPlainText()
            : QString();
    request.gradingInstructions =
        m_gradingInstructionsEdit
            ? m_gradingInstructionsEdit->toPlainText()
            : QString();
    request.specialInstructions =
        m_specialInstructionsEdit
            ? m_specialInstructionsEdit->toPlainText()
            : QString();
    request.schedule =
        m_scheduleWidget
            ? m_scheduleWidget->scheduleModel()
            : ScheduleViewModel();
    request.classInformation = buildClassInformation();
    request.subNotes =
        m_subNotesEdit
            ? m_subNotesEdit->toPlainText()
            : QString();

    const SubPrepPrintService::Result result =
        SubPrepPrintService::printSubPrep(request);

    if (result.status == SubPrepPrintService::Status::Failed)
    {
        QMessageBox::warning(
            this,
            tr("Print Sub Prep"),
            result.message
            );
    }
}

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
            tr("Print Sub Prep"),
            m_scrollContent
            );
    m_printButton->setObjectName(
        QStringLiteral("subPrepPrintButton")
        );
    m_printButton->setMinimumWidth(130);
    m_printButton->setToolTip(
        tr("Print all sub prep information as an A4 PDF.")
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
        new ScheduleSectionWidget(
            m_services,
            m_scheduleCard,
            ScheduleSectionMode::ReadOnly
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
            tr("Sub Notes"),
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
        &SubPrepPage::printSubPrep
        );
}

void SubPrepPage::loadPageData()
{
    m_loading = true;

    if (m_autosaveTimer)
    {
        m_autosaveTimer->stop();
    }

    loadStoredSettings();
    loadPersonalZoomInformation();
    loadCampuses();
    refreshGeneratedContent();

    m_loading = false;
    clearDirty();
}

void SubPrepPage::loadStoredSettings()
{
    auto* dataService =
        openDataService(m_services);

    if (!dataService)
    {
        return;
    }

    const QSignalBlocker materialsBlocker(m_classMaterialsEdit);
    const QSignalBlocker gradingBlocker(m_gradingInstructionsEdit);
    const QSignalBlocker specialBlocker(m_specialInstructionsEdit);
    const QSignalBlocker notesBlocker(m_subNotesEdit);

    m_classMaterialsEdit->setPlainText(
        dataService
            ->loadSetting(
                SettingsKeys::ClassMaterials,
                QString()
                )
            .toString()
        );

    const QVariant storedGrading =
        dataService->loadSetting(
            SettingsKeys::BookReportGrading,
            QVariant()
            );
    const QVariant storedSpecial =
        dataService->loadSetting(
            SettingsKeys::BookReportSpecialInstructions,
            QVariant()
            );

    if (storedGrading.isValid())
    {
        m_gradingInstructionsEdit->setPlainText(
            storedGrading.toString()
            );
        m_specialInstructionsEdit->setPlainText(
            storedSpecial.isValid()
                ? storedSpecial.toString()
                : QString()
            );
    }
    else
    {
        m_gradingInstructionsEdit->setPlainText(
            defaultGradingInstructions()
            );
        m_specialInstructionsEdit->setPlainText(
            storedSpecial.isValid()
                ? storedSpecial.toString()
                : defaultSpecialInstructions()
            );
    }

    m_subNotesEdit->setPlainText(
        dataService
            ->loadSetting(
                SettingsKeys::SubNotes,
                QString()
                )
            .toString()
        );
}

void SubPrepPage::loadPersonalZoomInformation()
{
    auto* dataService =
        openDataService(m_services);

    if (!dataService)
    {
        return;
    }

    const QSignalBlocker loginBlocker(m_zoomLoginIdEdit);
    const QSignalBlocker passwordBlocker(m_zoomPasswordEdit);

    const QString loginId =
        loadSettingWithLegacyFallback(
            dataService,
            SettingsKeys::MyInfoZoomLoginId,
            SettingsKeys::LegacyZoomLoginId,
            NotAvailableText
            )
            .toString();
    const QString password =
        loadSettingWithLegacyFallback(
            dataService,
            SettingsKeys::MyInfoZoomPassword,
            SettingsKeys::LegacyZoomPassword,
            NotAvailableText
            )
            .toString();
    const bool unavailable =
        loadSettingWithLegacyFallback(
            dataService,
            SettingsKeys::MyInfoZoomNotAvailable,
            SettingsKeys::LegacyZoomNotAvailable,
            true
            )
            .toBool();

    m_zoomLoginIdEdit->setText(
        unavailable
            ? NotAvailableText
            : valueOrNa(loginId)
        );
    m_zoomPasswordEdit->setText(
        unavailable
            ? NotAvailableText
            : valueOrNa(password)
        );

    updateReadOnlyFieldWidths();
}

void SubPrepPage::loadCampuses()
{
    auto* dataService =
        openDataService(m_services);

    if (!dataService)
    {
        return;
    }

    const bool wasLoading =
        m_loading;
    m_loading = true;

    m_campuses =
        campusRepository().loadCampuses();

    const QString savedCampus =
        dataService
            ->loadSetting(
                SettingsKeys::MyInfoCampus,
                QString()
                )
            .toString();

    QString campusId;

    for (const CampusInfo& campus : std::as_const(m_campuses))
    {
        if (
            campus.id.compare(
                savedCampus.trimmed(),
                Qt::CaseInsensitive
                ) == 0
            || campusDisplayName(campus).compare(
                savedCampus.trimmed(),
                Qt::CaseInsensitive
                ) == 0
            )
        {
            campusId = campus.id;
            break;
        }
    }

    if (campusId.isEmpty() && !m_campuses.isEmpty())
    {
        campusId = m_campuses.first().id;
    }

    loadCampusFields(campusId);
    updateReadOnlyFieldWidths();

    m_loading = wasLoading;
}

void SubPrepPage::loadCampusFields(
    const QString& campusId
    )
{
    CampusInfo campus;
    bool found = false;

    for (const CampusInfo& candidate : std::as_const(m_campuses))
    {
        if (
            candidate.id.compare(
                campusId,
                Qt::CaseInsensitive
                ) == 0
            )
        {
            campus = candidate;
            found = true;
            break;
        }
    }

    const QSignalBlocker officeBlocker(m_officeNumberEdit);
    const QSignalBlocker wifiBlocker(m_officeWifiEdit);
    const QSignalBlocker wifiPasswordBlocker(m_officeWifiPasswordEdit);
    const QSignalBlocker photocopierBlocker(m_photocopierCodeEdit);

    m_officeNumberEdit->setText(
        found
            ? valueOrNa(campus.officeNumber)
            : NotAvailableText
        );
    m_officeWifiEdit->setText(
        found
            ? valueOrNa(campus.officeWifi)
            : NotAvailableText
        );
    m_officeWifiPasswordEdit->setText(
        found
            ? valueOrNa(campus.officeWifiPassword)
            : NotAvailableText
        );
    m_photocopierCodeEdit->setText(
        found
            ? valueOrNa(campus.photocopierCode)
            : NotAvailableText
        );

    updateReadOnlyFieldWidths();
}

void SubPrepPage::updateReadOnlyFieldWidths()
{
    WidgetSizing::updateTextAwareFieldWidth(
        m_officeNumberEdit,
        OfficeNumberFieldWidth
        );

    for (QLineEdit* edit : {
             m_officeWifiEdit,
             m_officeWifiPasswordEdit,
             m_photocopierCodeEdit,
             m_zoomLoginIdEdit,
             m_zoomPasswordEdit
             })
    {
        WidgetSizing::updateTextAwareFieldWidth(
            edit,
            CompactFieldWidth
            );
    }

    if (m_campusCard)
    {
        m_campusCard->updateGeometry();
    }

    if (m_zoomCard)
    {
        m_zoomCard->updateGeometry();
    }
}

bool SubPrepPage::saveSubPrepInternal()
{
    auto* dataService =
        openDataService(m_services);

    if (!dataService)
    {
        return false;
    }

    if (m_autosaveTimer)
    {
        m_autosaveTimer->stop();
    }

    restoreGradingDefaultIfNeeded();

    dataService->saveSetting(
        SettingsKeys::ClassMaterials,
        m_classMaterialsEdit->toPlainText()
        );
    dataService->saveSetting(
        SettingsKeys::BookReportGrading,
        m_gradingInstructionsEdit->toPlainText()
        );
    dataService->saveSetting(
        SettingsKeys::BookReportSpecialInstructions,
        m_specialInstructionsEdit->toPlainText()
        );
    dataService->saveSetting(
        SettingsKeys::SubNotes,
        m_subNotesEdit->toPlainText()
        );

    clearDirty();
    return true;
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
    auto* dataService =
        openDataService(m_services);

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
        m_scheduleWidget->visibleClassIds();
    options.visibleDays =
        visibleScheduleDays(
            state.showWeekends
            );
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
