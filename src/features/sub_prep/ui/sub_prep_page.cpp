#include "sub_prep_page.h"

#include "core/application_services.h"
#include "core/fontmanager.h"
#include "core/resource_paths.h"
#include "features/campus/data/campus_json_repository.h"
#include "features/sub_prep/ui/sub_prep_class_navigation.h"
#include "domain/models/class_info.h"
#include "domain/models/classroom.h"
#include "domain/models/teacher.h"
#include "data/data_service.h"
#include "ui/shared/constants/gui_constants.h"
#include "ui/shared/styles/roles.h"
#include "ui/shared/utils/widget_sizing.h"
#include "ui/shared/widgets/sectioncards/class_info_section_card.h"
#include "ui/shared/widgets/uniform_width_tab_bar.h"
#include "core/utils/sidebar_node_naming.h"

#include <algorithm>
#include <utility>

#include <QEvent>
#include <QFont>
#include <QFontMetrics>
#include <QFrame>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QScrollArea>
#include <QScrollBar>
#include <QShowEvent>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QStringList>
#include <QTabWidget>
#include <QTextEdit>
#include <QTimer>
#include <QVBoxLayout>
#include <QtAssert>

namespace
{
constexpr int AutosaveDelayMs = 750;
constexpr int OfficeNumberFieldWidth = 115;
constexpr int CompactFieldWidth = 170;
constexpr int TextEditVerticalPadding = 24;
constexpr int ClassTabContentTopMargin = 16;

const QString NotAvailableText =
    QStringLiteral("N/A");

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

namespace SettingsKeys
{
const QString MyInfoCampus =
    QStringLiteral("myInfo/campus");
const QString ClassMaterials =
    QStringLiteral("subPrep/classMaterials");
const QString BookReportGrading =
    QStringLiteral("subPrep/bookReportGrading");
const QString SubComments =
    QStringLiteral("subPrep/subComments");
}

struct ClassSummary
{
    Classroom classroom;
    ClassInfo info;
    Teacher teacher;
    QString displayName;
    int studentCount = 0;
};

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

} // namespace

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

    refreshGeneratedContent();

    if (!m_dirty)
    {
        loadStoredSettings();
        loadCampuses();
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

    if (m_importantInformationHeading)
    {
        m_importantInformationHeading->setText(
            tr("Important Information")
            );
    }

    if (m_subCommentsHeading)
    {
        m_subCommentsHeading->setText(
            tr("Sub Comments")
            );
    }

    if (m_campusCard)
    {
        m_campusCard->setTitle(
            tr("Campus Information")
            );
    }

    if (m_materialsCard)
    {
        m_materialsCard->setTitle(
            tr("Lesson Materials and Grading")
            );
    }

    if (m_commentsCard)
    {
        m_commentsCard->setTitle(
            tr("Comments")
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

    if (m_classMaterialsLabel)
    {
        m_classMaterialsLabel->setText(
            tr("Class Materials")
            );
    }

    if (m_bookReportGradingLabel)
    {
        m_bookReportGradingLabel->setText(
            tr("Book Report Grading")
            );
    }

    refreshGeneratedContent();
}

void SubPrepPage::scrollToSection(
    SubPrepSection section
    )
{
    m_currentSection =
        section;

    QLabel* target = nullptr;

    switch (section)
    {
    case SubPrepSection::ImportantInformation:
        target =
            m_importantInformationHeading;
        break;
    case SubPrepSection::SubComments:
        target =
            m_subCommentsHeading;
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

    case SubPrepSection::SubComments:
        return tr("Sub Comments");
    }

    return QString();
}

QString SubPrepPage::currentSectionKey() const
{
    switch (m_currentSection)
    {
    case SubPrepSection::ImportantInformation:
        return QStringLiteral("sub_prep_important");

    case SubPrepSection::SubComments:
        return QStringLiteral("sub_prep_comments");
    }

    return QString();
}

void SubPrepPage::showEvent(
    QShowEvent* event
    )
{
    BasePage::showEvent(event);

    refreshGeneratedContent();

    if (!m_dirty)
    {
        loadStoredSettings();
        loadCampuses();
    }
}

bool SubPrepPage::eventFilter(
    QObject* watched,
    QEvent* event
    )
{
    if (
        watched == m_bookReportGradingEdit
        && event->type() == QEvent::FocusOut
        )
    {
        if (restoreBookReportDefaultIfNeeded())
        {
            handleEditableChanged();
        }
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
    if (!m_dirty)
    {
        return;
    }

    saveSubPrepInternal();
}

void SubPrepPage::buildUi()
{
    contentLayout()->setContentsMargins(
        0,
        0,
        0,
        0
        );
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

    m_scrollArea->setWidget(
        m_scrollContent
        );
    contentLayout()->addWidget(
        m_scrollArea
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

    headerLayout->addWidget(m_titleLabel);
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
    m_scrollContentLayout->addWidget(
        m_importantInformationHeading
        );

    m_campusCard =
        new SectionCard(
            tr("Campus Information"),
            m_scrollContent
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

    m_officeNumberEdit->setReadOnly(true);
    m_officeWifiEdit->setReadOnly(true);
    m_officeWifiPasswordEdit->setReadOnly(true);
    m_photocopierCodeEdit->setReadOnly(true);

    WidgetSizing::installTextAwareFieldWidth(
        m_officeNumberEdit,
        OfficeNumberFieldWidth
        );
    WidgetSizing::installTextAwareFieldWidth(
        m_officeWifiEdit,
        CompactFieldWidth
        );
    WidgetSizing::installTextAwareFieldWidth(
        m_officeWifiPasswordEdit,
        CompactFieldWidth
        );
    WidgetSizing::installTextAwareFieldWidth(
        m_photocopierCodeEdit,
        CompactFieldWidth
        );

    m_officeNumberLabel =
        createFieldLabel(tr("Office Number"), m_campusCard);
    m_officeWifiLabel =
        createFieldLabel(tr("Office WiFi"), m_campusCard);
    m_officeWifiPasswordLabel =
        createFieldLabel(tr("WiFi Password"), m_campusCard);
    m_photocopierCodeLabel =
        createFieldLabel(tr("Photocopier Code"), m_campusCard);

    campusGrid->addWidget(
        m_officeNumberLabel,
        0,
        0,
        Qt::AlignLeft
        );
    campusGrid->addWidget(
        m_officeWifiLabel,
        0,
        1,
        Qt::AlignLeft
        );
    campusGrid->addWidget(
        m_officeWifiPasswordLabel,
        0,
        2,
        Qt::AlignLeft
        );
    campusGrid->addWidget(
        m_photocopierCodeLabel,
        0,
        3,
        Qt::AlignLeft
        );
    campusGrid->addWidget(
        m_officeNumberEdit,
        1,
        0
        );
    campusGrid->addWidget(
        m_officeWifiEdit,
        1,
        1
        );
    campusGrid->addWidget(
        m_officeWifiPasswordEdit,
        1,
        2
        );
    campusGrid->addWidget(
        m_photocopierCodeEdit,
        1,
        3
        );
    for (int column = 0; column < 4; ++column)
    {
        campusGrid->setColumnStretch(
            column,
            1
            );
    }

    m_campusCard->contentLayout()->addLayout(
        campusGrid
        );
    m_scrollContentLayout->addWidget(
        m_campusCard
        );

    m_materialsCard =
        new SectionCard(
            tr("Lesson Materials and Grading"),
            m_scrollContent
            );

    m_classMaterialsLabel =
        createFieldLabel(tr("Class Materials"), m_materialsCard);
    m_materialsCard->contentLayout()->addWidget(
        m_classMaterialsLabel
        );

    m_classMaterialsEdit =
        createTextEdit(
            6,
            false,
            m_materialsCard
            );
    m_materialsCard->contentLayout()->addWidget(
        m_classMaterialsEdit
        );

    m_bookReportGradingLabel =
        createFieldLabel(tr("Book Report Grading"), m_materialsCard);
    m_materialsCard->contentLayout()->addWidget(
        m_bookReportGradingLabel
        );

    m_bookReportGradingEdit =
        createTextEdit(
            5,
            false,
            m_materialsCard
            );
    m_bookReportGradingEdit->installEventFilter(this);
    m_materialsCard->contentLayout()->addWidget(
        m_bookReportGradingEdit
        );

    m_scrollContentLayout->addWidget(
        m_materialsCard
        );

    m_subCommentsHeading =
        createTopLevelHeading(
            tr("Sub Comments"),
            m_scrollContent
        );
    m_scrollContentLayout->addWidget(
        m_subCommentsHeading
        );

    m_commentsCard =
        new SectionCard(
            tr("Comments"),
            m_scrollContent
            );

    m_subCommentsEdit =
        createTextEdit(
            10,
            false,
            m_commentsCard
            );
    m_commentsCard->contentLayout()->addWidget(
        m_subCommentsEdit
        );
    m_scrollContentLayout->addWidget(
        m_commentsCard
        );

    connect(
        m_classMaterialsEdit,
        &QTextEdit::textChanged,
        this,
        &SubPrepPage::handleEditableChanged
        );
    connect(
        m_bookReportGradingEdit,
        &QTextEdit::textChanged,
        this,
        &SubPrepPage::handleEditableChanged
        );
    connect(
        m_subCommentsEdit,
        &QTextEdit::textChanged,
        this,
        &SubPrepPage::handleEditableChanged
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
    const QSignalBlocker gradingBlocker(m_bookReportGradingEdit);
    const QSignalBlocker commentsBlocker(m_subCommentsEdit);

    m_classMaterialsEdit->setPlainText(
        dataService
            ->loadSetting(
                SettingsKeys::ClassMaterials,
                QString()
                )
            .toString()
        );

    const QString grading =
        dataService
            ->loadSetting(
                SettingsKeys::BookReportGrading,
                defaultBookReportText()
                )
            .toString();

    m_bookReportGradingEdit->setPlainText(
        grading.trimmed().isEmpty()
            ? defaultBookReportText()
            : grading
        );

    m_subCommentsEdit->setPlainText(
        dataService
            ->loadSetting(
                SettingsKeys::SubComments,
                QString()
                )
            .toString()
        );
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
    const QString normalizedCampus =
        savedCampus.trimmed();

    for (const CampusInfo& campus : std::as_const(m_campuses))
    {
        if (
            campus.id.compare(
                normalizedCampus,
                Qt::CaseInsensitive
                ) == 0
            || campusDisplayName(campus).compare(
                normalizedCampus,
                Qt::CaseInsensitive
                ) == 0
            )
        {
            campusId =
                campus.id;
            break;
        }
    }

    if (
        campusId.isEmpty()
        && !m_campuses.isEmpty()
        )
    {
        campusId =
            m_campuses.first().id;
    }

    loadCampusFields(
        campusId
        );

    updateCampusFieldWidths();

    m_loading =
        wasLoading;
}

void SubPrepPage::loadCampusFields(
    const QString& campusId
    )
{
    const bool wasLoading =
        m_loading;

    m_loading = true;

    CampusInfo campus;
    bool foundCampus = false;

    for (const CampusInfo& campusInfo : m_campuses)
    {
        if (
            campusInfo.id.compare(
                campusId,
                Qt::CaseInsensitive
                ) == 0
            )
        {
            campus =
                campusInfo;
            foundCampus =
                true;
            break;
        }
    }

    const QSignalBlocker officeBlocker(m_officeNumberEdit);
    const QSignalBlocker wifiBlocker(m_officeWifiEdit);
    const QSignalBlocker wifiPasswordBlocker(m_officeWifiPasswordEdit);
    const QSignalBlocker photocopierBlocker(m_photocopierCodeEdit);

    const auto fieldText =
        [foundCampus](const QString& value)
        {
            return foundCampus
                ? value
                : NotAvailableText;
        };

    m_officeNumberEdit->setText(
        fieldText(campus.officeNumber)
        );
    m_officeWifiEdit->setText(
        fieldText(campus.officeWifi)
        );
    m_officeWifiPasswordEdit->setText(
        fieldText(campus.officeWifiPassword)
        );
    m_photocopierCodeEdit->setText(
        !foundCampus || campus.photocopierCode.trimmed().isEmpty()
            ? NotAvailableText
            : campus.photocopierCode
        );

    updateCampusFieldWidths();

    m_loading =
        wasLoading;
}

void SubPrepPage::updateCampusFieldWidths()
{
    WidgetSizing::updateTextAwareFieldWidth(
        m_officeNumberEdit,
        OfficeNumberFieldWidth
        );
    WidgetSizing::updateTextAwareFieldWidth(
        m_officeWifiEdit,
        CompactFieldWidth
        );
    WidgetSizing::updateTextAwareFieldWidth(
        m_officeWifiPasswordEdit,
        CompactFieldWidth
        );
    WidgetSizing::updateTextAwareFieldWidth(
        m_photocopierCodeEdit,
        CompactFieldWidth
        );

    if (m_campusCard)
    {
        m_campusCard->updateGeometry();
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

    restoreBookReportDefaultIfNeeded();

    dataService->saveSetting(
        SettingsKeys::ClassMaterials,
        m_classMaterialsEdit->toPlainText()
        );
    dataService->saveSetting(
        SettingsKeys::BookReportGrading,
        m_bookReportGradingEdit->toPlainText()
        );
    dataService->saveSetting(
        SettingsKeys::SubComments,
        m_subCommentsEdit->toPlainText()
        );

    clearDirty();

    return true;
}

void SubPrepPage::refreshGeneratedContent()
{
}

void SubPrepPage::rebuildClassInformation()
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

        if (m_classInfoSubtitle)
        {
            m_classInfoSubtitle->setText(
                tr("Select a class")
                );
        }

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

    QList<SubPrepClassNavigation::ClassEntry> navigationEntries;

    for (const ClassSummary& summary : std::as_const(summaries))
    {
        SubPrepClassNavigation::ClassEntry entry;
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

    const SubPrepClassNavigation::Model navigation =
        SubPrepClassNavigation::build(
            navigationEntries
            );

    if (m_classInfoSubtitle)
    {
        m_classInfoSubtitle->setText(
            navigation.mode == SubPrepClassNavigation::Mode::GradeGrouped
                ? tr("Grouped by grade")
                : tr("Select a class")
            );
    }

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

    if (navigation.mode == SubPrepClassNavigation::Mode::Flat)
    {
        auto* tabs =
            new UniformWidthTabWidget(
                UniformWidthTabKind::Class,
                QStringLiteral("subPrepClassTabBar"),
                m_classInformationContent
                );
        tabs->setObjectName("subPrepClassTabs");

        for (const SubPrepClassNavigation::ClassTab& tab
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
            QStringLiteral("subPrepGradeTabBar"),
            m_classInformationContent
            );
    gradeTabs->setObjectName("subPrepGradeTabs");

    int selectedGradeIndex = -1;
    int selectedClassIndex = -1;

    for (const SubPrepClassNavigation::GradeGroup& group
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
                QStringLiteral("subPrepClassTabBar"),
                gradePage
                );
        classTabs->setObjectName("subPrepClassTabs");

        for (const SubPrepClassNavigation::ClassTab& tab
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
                        QStringLiteral("subPrepClassTabs"),
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
                    QStringLiteral("subPrepClassTabs"),
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

bool SubPrepPage::normalizeLineEdit(
    QLineEdit* edit,
    const QString& defaultText
    )
{
    if (
        !edit
        || !edit->text().trimmed().isEmpty()
        )
    {
        return false;
    }

    const QSignalBlocker blocker(edit);

    edit->setText(
        defaultText
        );

    return true;
}

bool SubPrepPage::restoreBookReportDefaultIfNeeded()
{
    if (
        !m_bookReportGradingEdit
        || !m_bookReportGradingEdit
                ->toPlainText()
                .trimmed()
                .isEmpty()
        )
    {
        return false;
    }

    const QSignalBlocker blocker(m_bookReportGradingEdit);

    m_bookReportGradingEdit->setPlainText(
        defaultBookReportText()
        );

    return true;
}

QString SubPrepPage::defaultBookReportText() const
{
    return tr(
        "Scoring: 0 / 20 / 40 / 60 / 80 / 100\n"
        "Comments: Please leave a comment about what the student did well "
        "and what they need to work on.\n"
        "Additional Rules: N/A"
        );
}

QLabel* SubPrepPage::createTopLevelHeading(
    const QString& text,
    QWidget* parent
    ) const
{
    auto* label =
        new QLabel(
            text,
            parent
            );

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
        new QLabel(
            text,
            parent
            );

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

    edit->setReadOnly(
        readOnly
        );
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
    clearLayout(
        m_classInformationLayout
        );
}

void SubPrepPage::clearDirty()
{
    m_dirty = false;
}
