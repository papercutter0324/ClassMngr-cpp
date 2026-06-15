#include "sub_prep_page.h"

#include "core/application_services.h"
#include "core/fontmanager.h"
#include "core/settingsmanager.h"
#include "models/class_info.h"
#include "models/classroom.h"
#include "models/teacher.h"
#include "services/dataservice.h"
#include "ui/constants/gui_constants.h"
#include "ui/styles/roles.h"
#include "ui/widgets/sectioncards/class_info_section_card.h"
#include "utils/sidebar_node_naming.h"

#include <algorithm>

#include <QComboBox>
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
#include <QTextEdit>
#include <QTimer>
#include <QVBoxLayout>
#include <QVariant>
#include <QtAssert>

namespace
{
constexpr int AutosaveDelayMs = 750;
constexpr int FieldMinimumWidth = 190;
constexpr int FieldMaximumWidth = FieldMinimumWidth * 2;
constexpr int CampusFieldWidth = 150;
constexpr int OfficeNumberFieldWidth = 115;
constexpr int CompactFieldWidth = 170;
constexpr int TextEditVerticalPadding = 24;

const QString NotAvailableText =
    QStringLiteral("N/A");
const QString DefaultCampusName =
    QStringLiteral("Default");

namespace SettingsKeys
{
const QString PersonalZoomEmail =
    QStringLiteral("subPrep/personalZoomEmail");
const QString PersonalZoomPassword =
    QStringLiteral("subPrep/personalZoomPassword");
const QString MyInfoZoomLoginId =
    QStringLiteral("myInfo/zoomLoginId");
const QString MyInfoZoomPassword =
    QStringLiteral("myInfo/zoomPassword");
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

struct TeacherGroup
{
    int teacherId = -1;
    Teacher teacher;
    QList<ClassSummary> classes;
};

void applyFieldWidth(
    QWidget* widget
    )
{
    if (!widget)
    {
        return;
    }

    widget->setMinimumWidth(
        FieldMinimumWidth
        );
    widget->setMaximumWidth(
        FieldMaximumWidth
        );
}

void applyFixedFieldWidth(
    QWidget* widget,
    int width
    )
{
    if (!widget)
    {
        return;
    }

    widget->setMinimumWidth(
        width
        );
    widget->setMaximumWidth(
        width
        );
    widget->setSizePolicy(
        QSizePolicy::Fixed,
        widget->sizePolicy().verticalPolicy()
        );
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

QString teacherSortKey(
    const Teacher& teacher
    )
{
    const QString koreanName =
        teacher.teacherKr.trimmed();

    if (!koreanName.isEmpty())
    {
        return koreanName;
    }

    const QString englishName =
        teacher.teacherEn.trimmed();

    if (!englishName.isEmpty())
    {
        return englishName;
    }

    return SidebarNodeNaming::formatTeacherDisplayName(
        teacher
        );
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

QVariant loadSettingWithLegacyFallback(
    DataService* dataService,
    const QString& primaryKey,
    const QString& legacyKey,
    const QVariant& defaultValue
    )
{
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

    if (value.isValid())
    {
        dataService->saveSetting(
            primaryKey,
            value
            );
        return value;
    }

    return defaultValue;
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
    applyFixedFieldWidth(
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
    grid->addWidget(
        createReadOnlyValueEdit(value, parent),
        valueRow,
        column,
        Qt::AlignLeft | Qt::AlignTop
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
        loadCampuses();
    }
}

void SubPrepPage::scrollToSection(
    SubPrepSection section
    )
{
    QLabel* target = nullptr;

    switch (section)
    {
    case SubPrepSection::ImportantInformation:
        target =
            m_importantInformationHeading;
        break;
    case SubPrepSection::ClassInformation:
        target =
            m_classInformationHeading;
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

void SubPrepPage::showEvent(
    QShowEvent* event
    )
{
    BasePage::showEvent(event);

    refreshGeneratedContent();

    if (!m_dirty)
    {
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

void SubPrepPage::handleCampusChanged(
    int index
    )
{
    if (
        m_loading
        || !m_campusCombo
        || index < 0
        )
    {
        return;
    }

    if (m_dirty)
    {
        saveSubPrepInternal();
    }

    const int campusId =
        m_campusCombo
            ->itemData(index)
            .toInt();

    if (campusId <= 0)
    {
        return;
    }

    m_currentCampusId =
        campusId;

    SettingsManager::instance()
        .setLastCampusId(
            campusId
            );

    loadCampusFields(
        campusId
        );
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
    headerLayout->setContentsMargins(0, 0, 0, 0);
    headerLayout->setSpacing(2);

    m_titleLabel =
        new QLabel(
            tr("Sub Prep"),
            m_scrollContent
            );
    m_titleLabel->setObjectName("pageTitle");
    m_titleLabel->setFont(
        FontManager::getUiFont(
            24,
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
        FontManager::getUiFont(11)
        );

    headerLayout->addWidget(m_titleLabel);
    headerLayout->addWidget(m_subtitleLabel);
    m_scrollContentLayout->addLayout(headerLayout);

    m_importantInformationHeading =
        createTopLevelHeading(
            tr("Important Information"),
            m_scrollContent
        );
    m_scrollContentLayout->addWidget(
        m_importantInformationHeading
        );

    auto* zoomCard =
        new SectionCard(
            tr("Personal Zoom Information"),
            m_scrollContent
            );
    auto* zoomGrid =
        new QGridLayout;
    zoomGrid->setHorizontalSpacing(
        UiConstants::ClassInfo::Form::HorizontalSpacing
        );
    zoomGrid->setVerticalSpacing(
        UiConstants::ClassInfo::Form::VerticalSpacing
        );

    m_zoomEmailEdit =
        new QLineEdit(zoomCard);
    m_zoomPasswordEdit =
        new QLineEdit(zoomCard);

    m_zoomEmailEdit->setReadOnly(true);
    m_zoomPasswordEdit->setReadOnly(true);

    applyFieldWidth(m_zoomEmailEdit);
    applyFieldWidth(m_zoomPasswordEdit);

    zoomGrid->addWidget(
        createFieldLabel(tr("Login Email"), zoomCard),
        0,
        0,
        Qt::AlignLeft
        );
    zoomGrid->addWidget(
        createFieldLabel(tr("Password"), zoomCard),
        0,
        1,
        Qt::AlignLeft
        );
    zoomGrid->addWidget(
        m_zoomEmailEdit,
        1,
        0,
        Qt::AlignLeft
        );
    zoomGrid->addWidget(
        m_zoomPasswordEdit,
        1,
        1,
        Qt::AlignLeft
        );
    zoomGrid->setColumnStretch(2, 1);

    zoomCard->contentLayout()->addLayout(
        zoomGrid
        );
    m_scrollContentLayout->addWidget(
        zoomCard
        );

    auto* campusCard =
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

    m_campusCombo =
        new QComboBox(campusCard);
    m_officeNumberEdit =
        new QLineEdit(campusCard);
    m_officeWifiEdit =
        new QLineEdit(campusCard);
    m_officeWifiPasswordEdit =
        new QLineEdit(campusCard);
    m_photocopierCodeEdit =
        new QLineEdit(campusCard);

    applyFixedFieldWidth(
        m_campusCombo,
        CampusFieldWidth
        );
    applyFixedFieldWidth(
        m_officeNumberEdit,
        OfficeNumberFieldWidth
        );
    applyFixedFieldWidth(
        m_officeWifiEdit,
        CompactFieldWidth
        );
    applyFixedFieldWidth(
        m_officeWifiPasswordEdit,
        CompactFieldWidth
        );
    applyFixedFieldWidth(
        m_photocopierCodeEdit,
        CompactFieldWidth
        );

    campusGrid->addWidget(
        createFieldLabel(tr("Campus"), campusCard),
        0,
        0,
        Qt::AlignLeft
        );
    campusGrid->addWidget(
        createFieldLabel(tr("Office Number"), campusCard),
        0,
        1,
        Qt::AlignLeft
        );
    campusGrid->addWidget(
        createFieldLabel(tr("Office WiFi"), campusCard),
        0,
        2,
        Qt::AlignLeft
        );
    campusGrid->addWidget(
        createFieldLabel(tr("WiFi Password"), campusCard),
        0,
        3,
        Qt::AlignLeft
        );
    campusGrid->addWidget(
        createFieldLabel(tr("Photocopier Code"), campusCard),
        0,
        4,
        Qt::AlignLeft
        );
    campusGrid->addWidget(
        m_campusCombo,
        1,
        0,
        Qt::AlignLeft
        );
    campusGrid->addWidget(
        m_officeNumberEdit,
        1,
        1,
        Qt::AlignLeft
        );
    campusGrid->addWidget(
        m_officeWifiEdit,
        1,
        2,
        Qt::AlignLeft
        );
    campusGrid->addWidget(
        m_officeWifiPasswordEdit,
        1,
        3,
        Qt::AlignLeft
        );
    campusGrid->addWidget(
        m_photocopierCodeEdit,
        1,
        4,
        Qt::AlignLeft
        );
    campusGrid->setColumnStretch(5, 1);

    campusCard->contentLayout()->addLayout(
        campusGrid
        );
    m_scrollContentLayout->addWidget(
        campusCard
        );

    auto* materialsCard =
        new SectionCard(
            tr("Lesson Materials and Grading"),
            m_scrollContent
            );

    materialsCard->contentLayout()->addWidget(
        createFieldLabel(tr("Class Materials"), materialsCard)
        );

    m_classMaterialsEdit =
        createTextEdit(
            6,
            false,
            materialsCard
            );
    materialsCard->contentLayout()->addWidget(
        m_classMaterialsEdit
        );

    materialsCard->contentLayout()->addWidget(
        createFieldLabel(tr("Time Filler Activities"), materialsCard)
        );

    m_timeFillerActivitiesEdit =
        createTextEdit(
            8,
            true,
            materialsCard
            );
    materialsCard->contentLayout()->addWidget(
        m_timeFillerActivitiesEdit
        );

    materialsCard->contentLayout()->addWidget(
        createFieldLabel(tr("Book Report Grading"), materialsCard)
        );

    m_bookReportGradingEdit =
        createTextEdit(
            5,
            false,
            materialsCard
            );
    m_bookReportGradingEdit->installEventFilter(this);
    materialsCard->contentLayout()->addWidget(
        m_bookReportGradingEdit
        );

    m_scrollContentLayout->addWidget(
        materialsCard
        );

    m_classInformationHeading =
        createTopLevelHeading(
            tr("Class Information"),
            m_scrollContent
        );
    m_scrollContentLayout->addWidget(
        m_classInformationHeading
        );

    auto* classInfoSubtitle =
        new QLabel(
            tr("Sorted by Co-Teacher"),
            m_scrollContent
            );
    classInfoSubtitle->setObjectName("pageSubtitle");
    classInfoSubtitle->setAlignment(Qt::AlignCenter);
    classInfoSubtitle->setFont(
        FontManager::getUiFont(11)
        );
    m_scrollContentLayout->addWidget(
        classInfoSubtitle
        );

    m_classInformationContent =
        new QWidget(m_scrollContent);
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

    m_subCommentsHeading =
        createTopLevelHeading(
            tr("Sub Comments"),
            m_scrollContent
        );
    m_scrollContentLayout->addWidget(
        m_subCommentsHeading
        );

    auto* commentsCard =
        new SectionCard(
            tr("Comments"),
            m_scrollContent
            );

    m_subCommentsEdit =
        createTextEdit(
            10,
            false,
            commentsCard
            );
    commentsCard->contentLayout()->addWidget(
        m_subCommentsEdit
        );
    m_scrollContentLayout->addWidget(
        commentsCard
        );

    connect(
        m_officeNumberEdit,
        &QLineEdit::textChanged,
        this,
        &SubPrepPage::handleEditableChanged
        );
    connect(
        m_officeWifiEdit,
        &QLineEdit::textChanged,
        this,
        &SubPrepPage::handleEditableChanged
        );
    connect(
        m_officeWifiPasswordEdit,
        &QLineEdit::textChanged,
        this,
        &SubPrepPage::handleEditableChanged
        );
    connect(
        m_photocopierCodeEdit,
        &QLineEdit::textChanged,
        this,
        &SubPrepPage::handleEditableChanged
        );

    for (auto* edit : {
             m_photocopierCodeEdit
         })
    {
        connect(
            edit,
            &QLineEdit::editingFinished,
            this,
            [this]
            {
                if (normalizeProtectedFields())
                {
                    handleEditableChanged();
                }
            }
            );
    }

    connect(
        m_campusCombo,
        &QComboBox::currentIndexChanged,
        this,
        &SubPrepPage::handleCampusChanged
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
        m_services
            ? m_services->dataService()
            : nullptr;

    if (!dataService)
    {
        return;
    }

    const QSignalBlocker emailBlocker(m_zoomEmailEdit);
    const QSignalBlocker passwordBlocker(m_zoomPasswordEdit);
    const QSignalBlocker materialsBlocker(m_classMaterialsEdit);
    const QSignalBlocker gradingBlocker(m_bookReportGradingEdit);
    const QSignalBlocker commentsBlocker(m_subCommentsEdit);

    const QString email =
        loadSettingWithLegacyFallback(
            dataService,
            SettingsKeys::MyInfoZoomLoginId,
            SettingsKeys::PersonalZoomEmail,
            NotAvailableText
            )
            .toString();
    const QString password =
        loadSettingWithLegacyFallback(
            dataService,
            SettingsKeys::MyInfoZoomPassword,
            SettingsKeys::PersonalZoomPassword,
            NotAvailableText
            )
            .toString();

    m_zoomEmailEdit->setText(
        email.trimmed().isEmpty()
            ? NotAvailableText
            : email
        );
    m_zoomPasswordEdit->setText(
        password.trimmed().isEmpty()
            ? NotAvailableText
            : password
        );
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
        m_services
            ? m_services->dataService()
            : nullptr;

    if (!dataService || !m_campusCombo)
    {
        return;
    }

    const bool wasLoading =
        m_loading;

    m_loading = true;

    m_campuses =
        dataService->getAllCampuses();

    if (m_campuses.isEmpty())
    {
        ensureDefaultCampus();
        m_campuses =
            dataService->getAllCampuses();
    }

    const QSignalBlocker comboBlocker(m_campusCombo);

    m_campusCombo->clear();

    for (const CampusRecord& campus : m_campuses)
    {
        m_campusCombo->addItem(
            campus.name.trimmed().isEmpty()
                ? DefaultCampusName
                : campus.name.trimmed(),
            campus.id
            );
    }

    int campusIdToSelect =
        m_currentCampusId;

    if (campusIdToSelect <= 0)
    {
        const auto savedCampusId =
            SettingsManager::instance()
                .getLastCampusId();

        if (savedCampusId.has_value())
        {
            campusIdToSelect =
                savedCampusId.value();
        }
    }

    int index =
        campusIdToSelect > 0
            ? m_campusCombo->findData(campusIdToSelect)
            : -1;

    if (
        index < 0
        && m_campusCombo->count() > 0
        )
    {
        index = 0;
    }

    if (index >= 0)
    {
        m_campusCombo->setCurrentIndex(index);

        m_currentCampusId =
            m_campusCombo
                ->itemData(index)
                .toInt();

        SettingsManager::instance()
            .setLastCampusId(
                m_currentCampusId
                );

        loadCampusFields(
            m_currentCampusId
            );
    }

    m_loading =
        wasLoading;
}

void SubPrepPage::loadCampusFields(
    int campusId
    )
{
    auto* dataService =
        m_services
            ? m_services->dataService()
            : nullptr;

    if (!dataService || campusId <= 0)
    {
        return;
    }

    const bool wasLoading =
        m_loading;

    m_loading = true;

    const CampusRecord campus =
        dataService->getCampus(
            campusId
            );

    const QSignalBlocker officeBlocker(m_officeNumberEdit);
    const QSignalBlocker wifiBlocker(m_officeWifiEdit);
    const QSignalBlocker wifiPasswordBlocker(m_officeWifiPasswordEdit);
    const QSignalBlocker photocopierBlocker(m_photocopierCodeEdit);

    m_officeNumberEdit->setText(
        campus.officeNumber
        );
    m_officeWifiEdit->setText(
        campus.officeWifi
        );
    m_officeWifiPasswordEdit->setText(
        campus.officeWifiPassword
        );
    m_photocopierCodeEdit->setText(
        campus.photocopierCode.trimmed().isEmpty()
            ? NotAvailableText
            : campus.photocopierCode
        );

    m_loading =
        wasLoading;
}

bool SubPrepPage::saveSubPrepInternal()
{
    auto* dataService =
        m_services
            ? m_services->dataService()
            : nullptr;

    if (!dataService)
    {
        return false;
    }

    if (m_autosaveTimer)
    {
        m_autosaveTimer->stop();
    }

    normalizeProtectedFields();

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

    const bool campusSaved =
        saveCurrentCampus();

    if (!campusSaved)
    {
        m_dirty = true;
        return false;
    }

    clearDirty();

    return true;
}

bool SubPrepPage::saveCurrentCampus()
{
    auto* dataService =
        m_services
            ? m_services->dataService()
            : nullptr;

    if (!dataService)
    {
        return false;
    }

    if (m_currentCampusId <= 0)
    {
        m_currentCampusId =
            ensureDefaultCampus();
    }

    CampusRecord campus =
        dataService->getCampus(
            m_currentCampusId
            );

    if (campus.id <= 0)
    {
        campus.name =
            DefaultCampusName;
    }

    campus.officeNumber =
        m_officeNumberEdit->text().trimmed();
    campus.officeWifi =
        m_officeWifiEdit->text().trimmed();
    campus.officeWifiPassword =
        m_officeWifiPasswordEdit->text().trimmed();
    campus.photocopierCode =
        m_photocopierCodeEdit->text().trimmed().isEmpty()
            ? NotAvailableText
            : m_photocopierCodeEdit->text().trimmed();

    const int savedCampusId =
        dataService->saveCampus(
            campus
            );

    if (savedCampusId <= 0)
    {
        return false;
    }

    m_currentCampusId =
        savedCampusId;

    SettingsManager::instance()
        .setLastCampusId(
            savedCampusId
            );

    return true;
}

int SubPrepPage::ensureDefaultCampus()
{
    auto* dataService =
        m_services
            ? m_services->dataService()
            : nullptr;

    if (!dataService)
    {
        return -1;
    }

    CampusRecord campus;
    campus.name =
        DefaultCampusName;
    campus.photocopierCode =
        NotAvailableText;

    const int campusId =
        dataService->saveCampus(
            campus
            );

    if (campusId > 0)
    {
        SettingsManager::instance()
            .setLastCampusId(
                campusId
                );
    }

    return campusId;
}

void SubPrepPage::refreshGeneratedContent()
{
    rebuildTimeFillerActivities();
    rebuildClassInformation();
}

void SubPrepPage::rebuildTimeFillerActivities()
{
    auto* dataService =
        m_services
            ? m_services->dataService()
            : nullptr;

    if (!dataService || !m_timeFillerActivitiesEdit)
    {
        return;
    }

    QString html;

    const QList<Classroom> classes =
        dataService->getClasses();

    for (const Classroom& classroom : classes)
    {
        const ClassInfo info =
            dataService->loadClassInfo(
                classroom.id
                );

        const QString activities =
            info.timeFillerActivities.trimmed();

        if (activities.isEmpty())
        {
            continue;
        }

        Teacher teacher;

        if (info.teacherId > 0)
        {
            teacher =
                dataService->getTeacher(
                    info.teacherId
                    );
        }

        QString displayName =
            SidebarNodeNaming::formatClassDisplayName(
                info,
                teacher
                )
                .trimmed();

        if (displayName.isEmpty())
        {
            displayName =
                classroom.name.trimmed().isEmpty()
                    ? tr("Class %1").arg(classroom.id)
                    : classroom.name.trimmed();
        }

        if (!html.isEmpty())
        {
            html.append(QStringLiteral("<br><br>"));
        }

        html.append(
            QStringLiteral(
                "<div><b><u>%1</u></b></div>"
                "<div>%2</div>"
                )
                .arg(
                    displayName.toHtmlEscaped(),
                    activities
                        .toHtmlEscaped()
                        .replace(
                            QLatin1Char('\n'),
                            QStringLiteral("<br>")
                            )
                    )
            );
    }

    const QSignalBlocker blocker(m_timeFillerActivitiesEdit);

    m_timeFillerActivitiesEdit->setHtml(
        html
        );
}

void SubPrepPage::rebuildClassInformation()
{
    auto* dataService =
        m_services
            ? m_services->dataService()
            : nullptr;

    if (!dataService || !m_classInformationLayout)
    {
        return;
    }

    clearClassInformation();

    QList<TeacherGroup> groups;
    TeacherGroup unassignedGroup;
    unassignedGroup.teacherId = -1;

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

        if (summary.teacher.id <= 0)
        {
            unassignedGroup.classes.append(
                summary
                );
            continue;
        }

        auto group =
            std::find_if(
                groups.begin(),
                groups.end(),
                [&summary](const TeacherGroup& candidate)
                {
                    return candidate.teacherId == summary.teacher.id;
                }
                );

        if (group == groups.end())
        {
            TeacherGroup newGroup;
            newGroup.teacherId =
                summary.teacher.id;
            newGroup.teacher =
                summary.teacher;
            newGroup.classes.append(
                summary
                );

            groups.append(
                newGroup
                );
        }
        else
        {
            group->classes.append(
                summary
                );
        }
    }

    const auto sortClasses =
        [](TeacherGroup& group)
        {
            std::sort(
                group.classes.begin(),
                group.classes.end(),
                [](const ClassSummary& first, const ClassSummary& second)
                {
                    return QString::localeAwareCompare(
                        first.displayName,
                        second.displayName
                        ) < 0;
                }
                );
        };

    for (TeacherGroup& group : groups)
    {
        sortClasses(group);
    }
    sortClasses(unassignedGroup);

    std::sort(
        groups.begin(),
        groups.end(),
        [](const TeacherGroup& first, const TeacherGroup& second)
        {
            return QString::localeAwareCompare(
                teacherSortKey(first.teacher),
                teacherSortKey(second.teacher)
                ) < 0;
        }
        );

    if (
        groups.isEmpty()
        && unassignedGroup.classes.isEmpty()
        )
    {
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

    if (!unassignedGroup.classes.isEmpty())
    {
        groups.append(
            unassignedGroup
            );
    }

    for (const TeacherGroup& group : groups)
    {
        const bool unassigned =
            group.teacherId <= 0;

        auto* teacherHeading =
            new QLabel(
                teacherHeadingText(
                    group.teacher,
                    unassigned
                    ),
                m_classInformationContent
                );
        teacherHeading->setObjectName("sectionTitle");
        teacherHeading->setFont(
            FontManager::getUiFont(
                16,
                QFont::DemiBold
                )
            );
        m_classInformationLayout->addWidget(
            teacherHeading
            );

        auto* teacherCard =
            new QFrame(m_classInformationContent);
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
        teacherGrid->setColumnStretch(0, 0);
        teacherGrid->setColumnStretch(1, 0);
        teacherGrid->setColumnStretch(2, 0);
        teacherGrid->setColumnStretch(3, 1);

        addHorizontalInfoField(
            teacherGrid,
            0,
            1,
            0,
            tr("Internet Type"),
            unassigned ? QString() : group.teacher.internetType,
            teacherCard
            );
        addHorizontalInfoField(
            teacherGrid,
            0,
            1,
            1,
            tr("WiFi Name"),
            unassigned ? QString() : group.teacher.wifiName,
            teacherCard
            );
        addHorizontalInfoField(
            teacherGrid,
            0,
            1,
            2,
            tr("WiFi Password"),
            unassigned ? QString() : group.teacher.wifiPassword,
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
            unassigned ? QString() : group.teacher.projectionType,
            teacherCard
            );
        addHorizontalInfoField(
            teacherGrid,
            3,
            4,
            1,
            tr("Zoom ID"),
            unassigned ? QString() : group.teacher.zoomId,
            teacherCard
            );
        addHorizontalInfoField(
            teacherGrid,
            3,
            4,
            2,
            tr("Zoom Password"),
            unassigned ? QString() : group.teacher.zoomPassword,
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
            unassigned ? QString() : group.teacher.notes
            );
        teacherLayout->addWidget(
            teacherNotes
            );

        m_classInformationLayout->addWidget(
            teacherCard
            );

        for (const ClassSummary& summary : group.classes)
        {
            auto* classCard =
                new SectionCard(
                    summary.displayName,
                    m_classInformationContent
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

            m_classInformationLayout->addWidget(
                classCard
                );
        }
    }
}

bool SubPrepPage::normalizeProtectedFields()
{
    bool changed = false;

    changed =
        normalizeLineEdit(
            m_photocopierCodeEdit,
            NotAvailableText
            )
        || changed;
    changed =
        restoreBookReportDefaultIfNeeded()
        || changed;

    return changed;
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
            20,
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
