#include "class_details_page.h"
#include "ui/shared/dialogs/user_prompt_service.h"

#include "ui/shared/widgets/text_fit_push_button.h"
#include "ui/shared/pages/autosave_coordinator.h"
#include "ui/shared/pages/page_header.h"
#include "ui/shared/pages/scrollable_page_body.h"
#include "ui/shared/validation/form_validation_binder.h"

#include "ui/shared/widgets/sections/class_details_section.h"
#include "ui/shared/widgets/sections/class_schedule_section.h"
#include "ui/shared/widgets/sectioncards/class_info_section_card.h"
#include "ui/shared/widgets/sectioncards/class_time_row.h"

#include "core/application_services.h"
#include "domain/models/class_info.h"
#include "domain/models/teacher.h"
#include "features/classes/config/class_info_config.h"
#include "domain/rules/schedule_value_parser.h"
#include "domain/validation/validation_result.h"
#include "next/application/class_details_save_use_case.h"
#include "next/application/class_details_validation_policy.h"
#include "next/application/class_details_schedule_conflict_query.h"
#include "next/application/class_details_validation_context_query.h"
#include "next/application/class_details_page_query.h"
#include "next/platform/application_services_class_details_save_port.h"
#include "next/platform/application_services_class_details_schedule_conflict_port.h"
#include "next/platform/application_services_class_details_validation_context_port.h"
#include "next/platform/application_services_class_details_page_read_port.h"
#include "core/fontmanager.h"
#include "ui/shared/constants/gui_constants.h"
#include "ui/shared/styles/roles.h"
#include "core/utils/sidebar_node_naming.h"

#include <QFont>
#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QTime>
#include <QStringList>
#include <QVBoxLayout>
#include <QWidget>
#include <QtAssert>

#include <optional>
#include <string>
#include <vector>

namespace
{
SectionCard* addSectionCard(
    QVBoxLayout* layout,
    const QString& title,
    QWidget* parent
    )
{
    auto* card = new SectionCard(title, parent);

    layout->addWidget(
        card,
        0,
        Qt::AlignTop
        );

    return card;
}

std::optional<ClassMngr::Next::Domain::ScheduleTime> toDomainScheduleTime(
    const ClassTime& time
    )
{
    const auto day = ScheduleValueParser::parseWeekday(time.day);
    const auto parseNormalizedTime = [](const QString& value)
        -> std::optional<QTime>
    {
        const QTime parsed = QTime::fromString(
            value,
            QStringLiteral("h:mm AP")
            );
        if (!parsed.isValid()
            || parsed.toString(QStringLiteral("h:mm AP")) != value)
        {
            return std::nullopt;
        }

        return parsed;
    };
    const auto start = parseNormalizedTime(time.startTime);
    const auto end = parseNormalizedTime(time.endTime);
    if (!day || !start || !end)
    {
        return std::nullopt;
    }

    return ClassMngr::Next::Domain::ScheduleTime::fromMinutes(
        static_cast<int>(day->value),
        start->hour() * 60 + start->minute(),
        end->hour() * 60 + end->minute()
        );
}

std::optional<std::vector<ClassMngr::Next::Domain::ScheduleTime>>
toDomainScheduleTimes(const QList<ClassTime>& times)
{
    std::vector<ClassMngr::Next::Domain::ScheduleTime> result;
    result.reserve(static_cast<std::size_t>(times.size()));
    for (const ClassTime& time : times)
    {
        const auto converted = toDomainScheduleTime(time);
        if (!converted)
        {
            return std::nullopt;
        }
        result.push_back(*converted);
    }
    return result;
}

std::optional<ClassMngr::Next::Application::ClassDetailsSaveRequest>
classDetailsSaveRequest(const ClassInfo& info)
{
    const auto classId = ClassMngr::Next::Domain::ClassId::fromString(
        std::to_string(info.classId)
        );
    const auto regularTimes = toDomainScheduleTimes(info.classTimes);
    const auto intensiveTimes = toDomainScheduleTimes(info.intensiveTimes);
    if (!classId || !regularTimes || !intensiveTimes)
    {
        return std::nullopt;
    }

    return ClassMngr::Next::Application::ClassDetailsSaveRequest{
        .classId = *classId,
        .classGrade = info.classGrade.toStdU16String(),
        .classLevel = info.classLevel.toStdU16String(),
        .readingBook = info.readingBook.toStdU16String(),
        .essayBook = info.essayBook.toStdU16String(),
        .classColor = info.classColor.toStdU16String(),
        .fontColor = info.fontColor.toStdU16String(),
        .regularTimes = *regularTimes,
        .intensiveTimes = *intensiveTimes
    };
}

QString displayText(const std::string& value)
{
    return QString::fromUtf8(
        value.data(),
        static_cast<qsizetype>(value.size())
        );
}

std::string sourceText(const QString& value)
{
    return value.toUtf8().toStdString();
}

std::vector<std::u16string> validationChoices(const QStringList& choices)
{
    std::vector<std::u16string> values;
    values.reserve(static_cast<std::size_t>(choices.size()));
    for (const QString& choice : choices)
    {
        values.push_back(choice.toStdU16String());
    }
    return values;
}

ClassMngr::Next::Application::ClassDetailsValidationCatalog
classDetailsValidationCatalog()
{
    using namespace ClassMngr::Next::Application;

    ClassDetailsValidationCatalog catalog;
    catalog.grades.reserve(
        static_cast<std::size_t>(ClassInfoConfig::Grades.size())
        );
    for (const QString& gradeName : ClassInfoConfig::Grades)
    {
        ClassDetailsValidationGradeCatalog grade;
        grade.name = gradeName.toStdU16String();
        const QStringList levels = ClassInfoConfig::levelsForGrade(gradeName);
        grade.levels.reserve(static_cast<std::size_t>(levels.size()));
        for (const QString& levelName : levels)
        {
            grade.levels.push_back({
                .name = levelName.toStdU16String(),
                .readingBooks = validationChoices(
                    ClassInfoConfig::readingBooks(gradeName, levelName)
                    ),
                .essayBooks = validationChoices(
                    ClassInfoConfig::essayBooks(gradeName, levelName)
                    )
            });
        }
        catalog.grades.push_back(std::move(grade));
    }
    return catalog;
}

ClassMngr::Next::Application::ClassDetailsValidationInput
classDetailsValidationInput(const ClassInfo& info)
{
    using namespace ClassMngr::Next::Application;

    ClassDetailsValidationInput input;
    input.classId = info.classId;
    input.teacherId = info.teacherId;
    input.classGrade = info.classGrade.toStdU16String();
    input.classLevel = info.classLevel.toStdU16String();
    input.readingBook = info.readingBook.toStdU16String();
    input.essayBook = info.essayBook.toStdU16String();
    input.classColor = info.classColor.toStdU16String();
    input.fontColor = info.fontColor.toStdU16String();
    input.notes = info.notes.toStdU16String();
    input.timeFillerActivities = info.timeFillerActivities.toStdU16String();

    const auto copyRows = [](const QList<ClassTime>& times)
    {
        std::vector<ClassDetailsScheduleValidationRow> rows;
        rows.reserve(static_cast<std::size_t>(times.size()));
        for (const ClassTime& time : times)
        {
            rows.push_back({
                .day = time.day.toStdU16String(),
                .startTime = time.startTime.toStdU16String(),
                .endTime = time.endTime.toStdU16String()
            });
        }
        return rows;
    };
    input.regularTimes = copyRows(info.classTimes);
    input.intensiveTimes = copyRows(info.intensiveTimes);
    return input;
}

ClassMngr::Next::Application::ClassDetailsValidationOutput
validateClassDetailsInput(const ClassInfo& info)
{
    return ClassMngr::Next::Application::
        ClassDetailsValidationPolicy::normalizeAndValidate(
            classDetailsValidationInput(info),
            classDetailsValidationCatalog()
            );
}

QString validationIssueCode(
    ClassMngr::Next::Application::ClassDetailsValidationCode code
    )
{
    using Code = ClassMngr::Next::Application::ClassDetailsValidationCode;
    switch (code)
    {
    case Code::ClassIdInvalid:
        return QStringLiteral("class_info.class_id.invalid");
    case Code::TeacherIdInvalid:
        return QStringLiteral("class_info.teacher_id.invalid");
    case Code::GradeRequired:
        return QStringLiteral("class_info.grade.required");
    case Code::LevelRequired:
        return QStringLiteral("class_info.level.required");
    case Code::ValueNotAllowed:
        return QStringLiteral("class_info.value.not_allowed");
    case Code::BookRequiresGradeLevel:
        return QStringLiteral("class_info.book.requires_grade_level");
    case Code::InvalidHexColor:
        return QStringLiteral("color.invalid_hex");
    case Code::TextLengthOutOfBounds:
        return QStringLiteral("validation.length.out_of_bounds");
    case Code::InvalidWeekday:
        return QStringLiteral("schedule.weekday.invalid");
    case Code::InvalidTimeFormat:
        return QStringLiteral("schedule.time.invalid_format");
    case Code::EndNotAfterStart:
        return QStringLiteral("schedule.time.end_not_after_start");
    case Code::DuplicateSlot:
        return QStringLiteral("class_time.duplicate_slot");
    }
    return {};
}

QString validationIssueField(
    const ClassMngr::Next::Application::ClassDetailsValidationIssue& issue
    )
{
    using Field = ClassMngr::Next::Application::ClassDetailsValidationField;
    switch (issue.field)
    {
    case Field::ClassId:
        return QStringLiteral("classId");
    case Field::TeacherId:
        return QStringLiteral("teacherId");
    case Field::ClassGrade:
        return QStringLiteral("classGrade");
    case Field::ClassLevel:
        return QStringLiteral("classLevel");
    case Field::ReadingBook:
        return QStringLiteral("readingBook");
    case Field::EssayBook:
        return QStringLiteral("essayBook");
    case Field::ClassColor:
        return QStringLiteral("classColor");
    case Field::FontColor:
        return QStringLiteral("fontColor");
    case Field::Notes:
        return QStringLiteral("notes");
    case Field::TimeFillerActivities:
        return QStringLiteral("timeFillerActivities");
    case Field::ScheduleDay:
    case Field::ScheduleStartTime:
    case Field::ScheduleEndTime:
    {
        const QString prefix = issue.schedule
                == ClassMngr::Next::Application::
                    ClassDetailsValidationSchedule::Intensive
            ? QStringLiteral("intensiveTimes")
            : QStringLiteral("classTimes");
        const QString suffix = issue.field == Field::ScheduleDay
            ? QStringLiteral("day")
            : issue.field == Field::ScheduleStartTime
                ? QStringLiteral("startTime")
                : QStringLiteral("endTime");
        return QStringLiteral("%1[%2].%3")
            .arg(prefix)
            .arg(static_cast<qulonglong>(issue.row))
            .arg(suffix);
    }
    }
    return {};
}

int validationIssueColumn(
    ClassMngr::Next::Application::ClassDetailsValidationField field
    )
{
    using Field = ClassMngr::Next::Application::ClassDetailsValidationField;
    switch (field)
    {
    case Field::ScheduleDay:
        return 0;
    case Field::ScheduleStartTime:
        return 1;
    case Field::ScheduleEndTime:
        return 2;
    default:
        return -1;
    }
}

ValidationResult legacyValidationResult(
    const ClassMngr::Next::Application::ClassDetailsValidationOutput& output
    )
{
    using Code = ClassMngr::Next::Application::ClassDetailsValidationCode;

    ValidationResult result;
    for (const auto& issue : output.issues)
    {
        QVariantMap arguments;
        if (issue.integerValue)
        {
            arguments.insert(QStringLiteral("value"), *issue.integerValue);
        }
        if (issue.value)
        {
            arguments.insert(
                QStringLiteral("value"),
                QString::fromStdU16String(*issue.value)
                );
        }
        if (issue.start)
        {
            arguments.insert(
                QStringLiteral("start"),
                QString::fromStdU16String(*issue.start)
                );
        }
        if (issue.end)
        {
            arguments.insert(
                QStringLiteral("end"),
                QString::fromStdU16String(*issue.end)
                );
        }
        if (issue.code == Code::ValueNotAllowed)
        {
            QStringList allowedValues;
            allowedValues.reserve(
                static_cast<qsizetype>(issue.allowedValues.size())
                );
            for (const std::u16string& value : issue.allowedValues)
            {
                allowedValues.append(QString::fromStdU16String(value));
            }
            arguments.insert(QStringLiteral("allowedValues"), allowedValues);
        }
        if (!issue.duplicateRows.empty())
        {
            QVariantList duplicateRows;
            duplicateRows.reserve(
                static_cast<qsizetype>(issue.duplicateRows.size())
                );
            for (const int row : issue.duplicateRows)
            {
                duplicateRows.append(row);
            }
            arguments.insert(QStringLiteral("duplicateRows"), duplicateRows);
        }
        if (issue.length)
        {
            arguments.insert(
                QStringLiteral("length"),
                static_cast<qlonglong>(*issue.length)
                );
        }
        if (issue.minimum)
        {
            arguments.insert(
                QStringLiteral("minimum"),
                static_cast<qlonglong>(*issue.minimum)
                );
        }
        if (issue.maximum)
        {
            arguments.insert(
                QStringLiteral("maximum"),
                static_cast<qlonglong>(*issue.maximum)
                );
        }

        const bool scheduleField = issue.field ==
                ClassMngr::Next::Application::
                    ClassDetailsValidationField::ScheduleDay
            || issue.field == ClassMngr::Next::Application::
                   ClassDetailsValidationField::ScheduleStartTime
            || issue.field == ClassMngr::Next::Application::
                   ClassDetailsValidationField::ScheduleEndTime;
        result.add({
            .code = validationIssueCode(issue.code),
            .field = validationIssueField(issue),
            .row = scheduleField ? static_cast<int>(issue.row) : -1,
            .column = validationIssueColumn(issue.field),
            .severity = ValidationSeverity::Error,
            .arguments = std::move(arguments)
        });
    }
    return result;
}

ClassInfo normalizedClassDetailsInfo(
    const ClassInfo& source,
    const ClassMngr::Next::Application::ClassDetailsValidationInput& normalized
    )
{
    ClassInfo info = source;
    info.classGrade = QString::fromStdU16String(normalized.classGrade);
    info.classLevel = QString::fromStdU16String(normalized.classLevel);
    info.readingBook = QString::fromStdU16String(normalized.readingBook);
    info.essayBook = QString::fromStdU16String(normalized.essayBook);
    info.classColor = QString::fromStdU16String(normalized.classColor);
    info.fontColor = QString::fromStdU16String(normalized.fontColor);
    info.notes = QString::fromStdU16String(normalized.notes);
    info.timeFillerActivities =
        QString::fromStdU16String(normalized.timeFillerActivities);

    const auto copyRows = [](
        const std::vector<
            ClassMngr::Next::Application::ClassDetailsScheduleValidationRow
            >& rows
        )
    {
        QList<ClassTime> times;
        times.reserve(static_cast<qsizetype>(rows.size()));
        for (const auto& row : rows)
        {
            times.append({
                QString::fromStdU16String(row.day),
                QString::fromStdU16String(row.startTime),
                QString::fromStdU16String(row.endTime)
            });
        }
        return times;
    };
    info.classTimes = copyRows(normalized.regularTimes);
    info.intensiveTimes = copyRows(normalized.intensiveTimes);
    return info;
}

ClassMngr::Next::Application::ClassDetailsPageFields displayFields(
    const ClassInfo& info
    )
{
    ClassMngr::Next::Application::ClassDetailsPageFields fields;
    fields.classGrade = sourceText(info.classGrade);
    fields.classLevel = sourceText(info.classLevel);
    fields.readingBook = sourceText(info.readingBook);
    fields.essayBook = sourceText(info.essayBook);
    fields.classColor = sourceText(info.classColor);
    fields.fontColor = sourceText(info.fontColor);
    fields.regularSchedule.reserve(
        static_cast<std::size_t>(info.classTimes.size())
        );
    fields.intensiveSchedule.reserve(
        static_cast<std::size_t>(info.intensiveTimes.size())
        );
    for (const ClassTime& time : info.classTimes)
    {
        fields.regularSchedule.push_back({
            sourceText(time.day),
            sourceText(time.startTime),
            sourceText(time.endTime)
        });
    }
    for (const ClassTime& time : info.intensiveTimes)
    {
        fields.intensiveSchedule.push_back({
            sourceText(time.day),
            sourceText(time.startTime),
            sourceText(time.endTime)
        });
    }
    return fields;
}

QList<ClassTime> displaySchedule(
    const std::vector<
        ClassMngr::Next::Application::ClassDetailsPageScheduleRow
        >& rows
    )
{
    QList<ClassTime> result;
    result.reserve(static_cast<qsizetype>(rows.size()));
    for (const auto& value : rows)
    {
        ClassTime row;
        row.day = displayText(value.day);
        row.startTime = displayText(value.startTime);
        row.endTime = displayText(value.endTime);
        result.append(std::move(row));
    }
    return result;
}

std::optional<ClassMngr::Next::Application::ClassDetailsPageReadSnapshot>
readDisplaySnapshot(
    ApplicationServices* services,
    ClassMngr::Next::Application::ClassDetailsPageReadPort* injectedPort,
    const int classId
    )
{
    if (!services || classId < 0)
    {
        return std::nullopt;
    }

    const auto typedClassId =
        ClassMngr::Next::Domain::ClassId::fromString(
            std::to_string(classId)
            );
    if (!typedClassId)
    {
        return std::nullopt;
    }

    const auto execute = [&typedClassId](
        ClassMngr::Next::Application::ClassDetailsPageReadPort& readPort
        ) -> std::optional<
            ClassMngr::Next::Application::ClassDetailsPageReadSnapshot
            >
    {
        const ClassMngr::Next::Application::ClassDetailsPageQuery query(
            readPort
            );
        auto loaded = query.execute(*typedClassId);
        if (!loaded)
        {
            return std::nullopt;
        }
        return std::move(loaded.value());
    };

    if (injectedPort)
    {
        return execute(*injectedPort);
    }

    ClassMngr::Next::Platform::
        ApplicationServicesClassDetailsPageReadPort readPort(*services);
    return execute(readPort);
}
}

ClassDetailsPage::ClassDetailsPage(
    ApplicationServices* services,
    bool embedded,
    QWidget* parent,
    ClassMngr::Next::Application::ClassDetailsSavePort* savePort,
    ClassMngr::Next::Application::ClassDetailsPageReadPort* displayReadPort,
    ClassMngr::Next::Application::ClassDetailsScheduleConflictPort*
        scheduleConflictPort,
    ClassMngr::Next::Application::ClassDetailsValidationContextPort*
        validationContextPort
    )
    : BasePage(parent)
    , m_services(services)
    , m_savePort(savePort)
    , m_displayReadPort(displayReadPort)
    , m_scheduleConflictPort(scheduleConflictPort)
    , m_validationContextPort(validationContextPort)
    , m_embedded(embedded)
    , m_autosave(new AutosaveCoordinator(this))
{
    Q_ASSERT(m_services);

    setProperty("role", UiRoles::ClassInfo);

    if (m_embedded)
    {
        setPageLayoutMargins({});
    }

    buildUi();
    m_autosave->bindSaveButton(m_saveButton);
    connect(
        m_autosave,
        &AutosaveCoordinator::saveRequested,
        this,
        [this](bool interactive) {
            saveClassInfoInternal(interactive);
        }
        );

    connect(
        m_detailsSection,
        &ClassDetailsSection::dataChanged,
        this,
        &ClassDetailsPage::markDirty
        );

    connect(
        m_scheduleSection,
        &ClassScheduleSection::dataChanged,
        this,
        &ClassDetailsPage::markDirty
        );
}

void ClassDetailsPage::buildUi()
{
    contentLayout()->setContentsMargins(
        m_embedded ? 0 : UiConstants::Pages::Margin,
        m_embedded ? 0 : UiConstants::Pages::Margin,
        m_embedded ? 0 : UiConstants::Pages::Margin,
        0
        );

    contentLayout()->setSpacing(
        m_embedded
            ? 12
            : UiConstants::Pages::Spacing
        );

    if (m_embedded)
    {
        m_embeddedHeading = new QLabel(tr("Class Details"), this);
        m_embeddedHeading->setObjectName(
            QStringLiteral("classDetailsHeading")
            );
        m_embeddedHeading->setFont(
            FontManager::getUiFont(18, QFont::DemiBold)
            );
        contentLayout()->addWidget(m_embeddedHeading);
    }

    m_pageHeader = new PageHeader(
        tr("Class Information"),
        tr("No class selected"),
        this
        );

    if (m_embedded)
    {
        m_pageHeader->hide();
    }
    else
    {
        contentLayout()->addWidget(m_pageHeader);
        contentLayout()->addSpacing(
            UiConstants::Pages::HeaderContentSpacing
            );
    }

    m_pageBody = new ScrollablePageBody(
        this,
        QMargins(0, 0, 0, 0),
        UiConstants::ClassInfo::Page::ContentSpacing
        );
    m_pageBody->setHorizontalScrollBarPolicy(
        Qt::ScrollBarAsNeeded
        );
    m_pageBody->setVerticalScrollBarPolicy(
        Qt::ScrollBarAsNeeded
        );
    m_scrollContent = m_pageBody->contentWidget();
    m_scrollContentLayout = m_pageBody->contentLayout();

    contentLayout()->addWidget(
        m_pageBody
        );

    m_detailsCard =
        addSectionCard(
            m_scrollContentLayout,
            tr("Class Details"),
            m_scrollContent
            );
    m_detailsCard->setObjectName(
        QStringLiteral("classDetailsCard")
        );

    m_detailsSection =
        new ClassDetailsSection(
            m_services,
            m_detailsCard
            );

    m_detailsCard->contentLayout()->addWidget(
        m_detailsSection
        );

    m_scheduleCard =
        addSectionCard(
            m_scrollContentLayout,
            tr("Class Times"),
            m_scrollContent
            );
    m_scheduleCard->setObjectName(
        QStringLiteral("classTimesCard")
        );

    m_scheduleSection =
        new ClassScheduleSection(m_scheduleCard);

    m_scheduleCard->contentLayout()->addWidget(
        m_scheduleSection
        );

    m_validationBinder = new FormValidationBinder(
        m_autosave,
        m_pageBody,
        this
        );

    const auto addValidationMessage =
        [this](SectionCard* card, const QString& objectName)
        {
            QLabel* label = m_validationBinder->createMessageLabel(card);
            label->setObjectName(objectName);
            card->contentLayout()->addWidget(label);
            return label;
        };

    m_gradeValidationMessage = addValidationMessage(
        m_detailsCard,
        QStringLiteral("classGradeValidationMessage")
        );
    m_levelValidationMessage = addValidationMessage(
        m_detailsCard,
        QStringLiteral("classLevelValidationMessage")
        );
    m_readingBookValidationMessage = addValidationMessage(
        m_detailsCard,
        QStringLiteral("classReadingBookValidationMessage")
        );
    m_essayBookValidationMessage = addValidationMessage(
        m_detailsCard,
        QStringLiteral("classEssayBookValidationMessage")
        );
    m_colorValidationMessage = addValidationMessage(
        m_detailsCard,
        QStringLiteral("classColorValidationMessage")
        );
    m_regularScheduleValidationMessage = addValidationMessage(
        m_scheduleCard,
        QStringLiteral("classRegularScheduleValidationMessage")
        );
    m_intensiveScheduleValidationMessage = addValidationMessage(
        m_scheduleCard,
        QStringLiteral("classIntensiveScheduleValidationMessage")
        );

    m_validationBinder->registerField(
        QStringLiteral("classGrade"),
        m_detailsSection->gradeEditor(),
        m_gradeValidationMessage
        );
    m_validationBinder->registerField(
        QStringLiteral("classLevel"),
        m_detailsSection->levelEditor(),
        m_levelValidationMessage
        );
    m_validationBinder->registerField(
        QStringLiteral("readingBook"),
        m_detailsSection->readingBookEditor(),
        m_readingBookValidationMessage
        );
    m_validationBinder->registerField(
        QStringLiteral("essayBook"),
        m_detailsSection->essayBookEditor(),
        m_essayBookValidationMessage
        );
    m_validationBinder->registerField(
        QStringLiteral("classColor"),
        m_detailsSection->colorEditor(),
        m_colorValidationMessage
        );
    m_validationBinder->registerField(
        QStringLiteral("fontColor"),
        m_detailsSection->colorEditor()
        );
    m_scheduleSection->setFocusPolicy(Qt::StrongFocus);
    m_validationBinder->registerFieldPrefix(
        QStringLiteral("classTimes["),
        m_scheduleSection,
        m_regularScheduleValidationMessage
        );
    m_validationBinder->registerFieldPrefix(
        QStringLiteral("intensiveTimes["),
        m_scheduleSection,
        m_intensiveScheduleValidationMessage
        );

    updateScrollContentMinimumWidth();

    m_saveButton =
        new TextFitPushButton(
            tr("Save Changes")
            );

    m_saveButton->setEnabled(false);
    m_saveButton->setObjectName(QStringLiteral("classInfoSaveButton"));

    bottomLayout()->addStretch();
    bottomLayout()->addWidget(
        m_saveButton
        );

    updateActions();
}

void ClassDetailsPage::updateScrollContentMinimumWidth()
{
    if (
        !m_scrollContent
        || !m_scrollContentLayout
        )
    {
        return;
    }

    m_scrollContentLayout->activate();

    m_scrollContent->setMinimumWidth(
        m_scrollContent->minimumSizeHint().width()
        );
}

void ClassDetailsPage::markDirty()
{
    if (m_autosave->isLoading())
    {
        return;
    }

    updateFormValidation();
    m_autosave->markDirty();
}

void ClassDetailsPage::clearDirty()
{
    m_autosave->markClean();
    updateActions();
}

void ClassDetailsPage::loadClass(
    const Classroom& classroom
    )
{
    m_autosave->setLoading(true);

    refresh();

    m_classroom = classroom;

    m_displaySnapshot = readDisplaySnapshot(
        m_services,
        m_displayReadPort,
        classroom.id
        );
    const ClassMngr::Next::Application::ClassDetailsPageFields emptyFields;
    const auto& fields = m_displaySnapshot && m_displaySnapshot->classFields
        ? m_displaySnapshot->classFields.value()
        : emptyFields;
    const int studentCount = m_displaySnapshot && m_displaySnapshot->studentCount
        ? m_displaySnapshot->studentCount.value()
        : 0;

    updateTitle(m_displaySnapshot ? &*m_displaySnapshot : nullptr);

    m_detailsSection->loadInfo(
        displayText(fields.classGrade),
        displayText(fields.classLevel),
        displayText(fields.readingBook),
        displayText(fields.essayBook),
        displayText(fields.classColor),
        displayText(fields.fontColor),
        studentCount
        );

    m_scheduleSection->loadSchedules(
        displaySchedule(fields.regularSchedule),
        displaySchedule(fields.intensiveSchedule)
        );

    refreshScheduleValidationBindings();
    m_validationBinder->clear();

    updateScrollContentMinimumWidth();

    m_autosave->setLoading(false);

    clearDirty();
}

void ClassDetailsPage::clearDatabaseState()
{
    m_autosave->setLoading(true);

    m_classroom = {};
    m_displaySnapshot.reset();
    m_detailsSection->loadInfo({}, {}, {}, {}, {}, {}, 0);
    m_scheduleSection->loadSchedules(
        QList<ClassTime>{},
        QList<ClassTime>{}
        );
    refreshScheduleValidationBindings();
    m_validationBinder->clear();
    updateTitle(nullptr);
    m_pageHeader->setSubtitle(
        tr("No class selected")
        );
    updateScrollContentMinimumWidth();

    m_autosave->setLoading(false);
    clearDirty();
}

void ClassDetailsPage::updateTitle(
    const ClassMngr::Next::Application::ClassDetailsPageReadSnapshot* snapshot
    )
{
    m_pageHeader->setTitle(tr("Class Information"));
    if (m_classroom.id < 0)
    {
        m_pageHeader->setSubtitle(tr("No class selected"));
        return;
    }

    QString displayName;
    if (snapshot && snapshot->classFields)
    {
        const auto& fields = snapshot->classFields.value();
        ClassInfo classInfo;
        classInfo.classId = m_classroom.id;
        classInfo.classGrade = displayText(fields.classGrade);
        classInfo.classLevel = displayText(fields.classLevel);
        classInfo.classTimes = displaySchedule(fields.regularSchedule);

        Teacher teacher;
        if (snapshot->teacherDisplayName)
        {
            teacher.preferredName = displayText(
                snapshot->teacherDisplayName.value()
                );
        }

        displayName = SidebarNodeNaming::formatClassDisplayName(
            classInfo,
            teacher
            );
    }

    const QString fallbackName =
        m_classroom.name.trimmed().isEmpty()
            ? tr("Class %1").arg(m_classroom.id)
            : m_classroom.name.trimmed();

    m_pageHeader->setSubtitle(
        displayName.trimmed().isEmpty()
            ? fallbackName
            : displayName
        );
}

void ClassDetailsPage::saveData()
{
    saveClassInfoInternal(true);
}

bool ClassDetailsPage::saveChanges()
{
    m_autosave->cancelPendingSave();

    if (!hasUnsavedChanges())
    {
        return true;
    }

    return saveClassInfoInternal(true);
}

bool ClassDetailsPage::hasUnsavedChanges() const
{
    return m_autosave->isDirty();
}

void ClassDetailsPage::discardChanges()
{
    m_autosave->cancelPendingSave();

    loadClass(m_classroom);
}

void ClassDetailsPage::setSaveMode(
    SaveMode mode
    )
{
    m_autosave->setSaveMode(mode);
}

void ClassDetailsPage::updateActions()
{
    m_autosave->setSaveAvailable(m_classroom.id >= 0);
    m_autosave->setSaveMode(m_autosave->saveMode());
}

ClassInfo ClassDetailsPage::classInfoFromForm() const
{
    ClassInfo info;
    info.classId = m_classroom.id;
    info.classGrade = m_detailsSection->grade();
    info.classLevel = m_detailsSection->level();
    info.readingBook = m_detailsSection->readingBook();
    info.essayBook = m_detailsSection->essayBook();
    info.classColor = m_detailsSection->classColor();
    info.fontColor = m_detailsSection->fontColor();
    info.classTimes = m_scheduleSection->regularTimes();
    info.intensiveTimes = m_scheduleSection->intensiveTimes();
    return info;
}

void ClassDetailsPage::refreshScheduleValidationBindings()
{
    if (!m_validationBinder)
    {
        return;
    }

    const auto bindRows =
        [this](const QList<ClassTimeRow*>& rows, const QString& fieldPrefix)
        {
            for (qsizetype index = 0; index < rows.size(); ++index)
            {
                ClassTimeRow* row = rows.at(index);
                if (!row)
                {
                    continue;
                }

                const QString rowPrefix =
                    QStringLiteral("%1[%2]").arg(fieldPrefix).arg(index);
                m_validationBinder->registerField(
                    rowPrefix + QStringLiteral(".day"),
                    row->dayCombo()
                    );
                m_validationBinder->registerField(
                    rowPrefix + QStringLiteral(".startTime"),
                    row->startHourCombo()
                    );
                m_validationBinder->registerField(
                    rowPrefix + QStringLiteral(".endTime"),
                    row->endCombo()
                    );
            }
        };

    m_validationBinder->unregisterFieldsWithPrefix(QStringLiteral("classTimes["));
    m_validationBinder->unregisterFieldsWithPrefix(
        QStringLiteral("intensiveTimes[")
        );
    bindRows(m_scheduleSection->regularRows(), QStringLiteral("classTimes"));
    bindRows(
        m_scheduleSection->intensiveRows(),
        QStringLiteral("intensiveTimes")
        );
}

void ClassDetailsPage::updateFormValidation()
{
    if (!m_validationBinder)
    {
        return;
    }

    if (m_classroom.id < 0)
    {
        m_validationBinder->clear();
        return;
    }

    refreshScheduleValidationBindings();
    const auto validation = validateClassDetailsInput(classInfoFromForm());
    m_validationBinder->setValidation(legacyValidationResult(validation));
}

bool ClassDetailsPage::saveClassInfoInternal(
    bool showMessages
    )
{
    if (m_classroom.id < 0)
    {
        return true;
    }

    ClassInfo info = classInfoFromForm();
    const auto typedClassId =
        ClassMngr::Next::Domain::ClassId::fromString(
            std::to_string(m_classroom.id)
            );
    if (typedClassId)
    {
        ClassMngr::Next::Platform::
            ApplicationServicesClassDetailsValidationContextPort defaultPort(
                m_services
                );
        const ClassMngr::Next::Application::
            ClassDetailsValidationContextPort& contextPort =
                m_validationContextPort
                    ? *m_validationContextPort
                    : defaultPort;
        const auto context = ClassMngr::Next::Application::
            ClassDetailsValidationContextQuery::execute(
                *typedClassId,
                contextPort
                );
        if (context)
        {
            const auto& values = context.value();
            info.teacherId = values.teacherId;
            info.notes = QString::fromStdU16String(values.notes);
            info.timeFillerActivities = QString::fromStdU16String(
                values.timeFillerActivities
                );
        }
        else
        {
            const ClassInfo emptyContext;
            info.teacherId = emptyContext.teacherId;
            info.notes = emptyContext.notes;
            info.timeFillerActivities = emptyContext.timeFillerActivities;
        }
    }
    else
    {
        const ClassInfo emptyContext;
        info.teacherId = emptyContext.teacherId;
        info.notes = emptyContext.notes;
        info.timeFillerActivities = emptyContext.timeFillerActivities;
    }

    refreshScheduleValidationBindings();
    const auto validation = validateClassDetailsInput(info);
    m_validationBinder->setValidation(legacyValidationResult(validation));
    if (m_validationBinder->hasErrors())
    {
        m_validationBinder->focusFirstError();
        return false;
    }

    if (
        showScheduleConflicts(
            info.classTimes,
            ScheduleType::Regular,
            tr("Regular Schedule Conflicts"),
            showMessages
            )
        )
    {
        return false;
    }

    if (
        showScheduleConflicts(
            info.intensiveTimes,
            ScheduleType::Intensive,
            tr("Intensive Schedule Conflicts"),
            showMessages
            )
        )
    {
        return false;
    }

    const ClassInfo normalizedInfo = normalizedClassDetailsInfo(
        info,
        validation.normalized
        );
    const auto request = classDetailsSaveRequest(normalizedInfo);
    ClassMngr::Next::Platform::
        ApplicationServicesClassDetailsSavePort defaultSavePort(m_services);
    const ClassMngr::Next::Application::ClassDetailsSavePort& savePort =
        m_savePort ? *m_savePort : defaultSavePort;
    const ClassMngr::Next::Domain::Result<void> saved = request
        ? ClassMngr::Next::Application::ClassDetailsSaveUseCase::execute(
              *request,
              savePort
              )
        : ClassMngr::Next::Domain::Result<void>::failure({
              .code = ClassMngr::Next::Domain::ErrorCode::Validation,
              .message = "Class schedule values could not be converted.",
              .recoverable = true
          });

    if (!saved)
    {
        m_autosave->markDirty(false);

        if (showMessages)
        {
            DialogServices::showWarning(
                this,
                tr("Save Class Information"),
                QString::fromUtf8(
                    saved.error().message.data(),
                    static_cast<qsizetype>(saved.error().message.size())
                    )
                );
        }

        return false;
    }

    clearDirty();

    if (auto refreshedSnapshot = readDisplaySnapshot(
            m_services,
            m_displayReadPort,
            m_classroom.id
            ))
    {
        m_displaySnapshot = std::move(refreshedSnapshot);
    }

    if (m_displaySnapshot)
    {
        m_displaySnapshot->classFields =
            ClassMngr::Next::Domain::Result<
                ClassMngr::Next::Application::ClassDetailsPageFields
                >::success(displayFields(info));
    }
    else if (const auto classId =
                 ClassMngr::Next::Domain::ClassId::fromString(
                     std::to_string(info.classId)
                     ))
    {
        const ClassMngr::Next::Domain::OperationError missingTeacher{
            .code = ClassMngr::Next::Domain::ErrorCode::NotFound,
            .message = "The class teacher display name is unavailable.",
            .recoverable = true
        };
        const ClassMngr::Next::Domain::OperationError missingRoster{
            .code = ClassMngr::Next::Domain::ErrorCode::NotFound,
            .message = "The class student count is unavailable.",
            .recoverable = true
        };
        m_displaySnapshot =
            ClassMngr::Next::Application::ClassDetailsPageReadSnapshot{
                *classId,
                ClassMngr::Next::Domain::Result<
                    ClassMngr::Next::Application::ClassDetailsPageFields
                    >::success(displayFields(info)),
                ClassMngr::Next::Domain::Result<std::string>::failure(
                    missingTeacher
                    ),
                ClassMngr::Next::Domain::Result<int>::failure(missingRoster)
            };
    }
    updateTitle(m_displaySnapshot ? &*m_displaySnapshot : nullptr);

    emit classInfoSaved(
        m_classroom.id
        );

    return true;
}

void ClassDetailsPage::refresh()
{
    BasePage::refresh();
}

void ClassDetailsPage::retranslateUi()
{
    if (m_embeddedHeading)
    {
        m_embeddedHeading->setText(tr("Class Details"));
    }

    m_displaySnapshot = readDisplaySnapshot(
        m_services,
        m_displayReadPort,
        m_classroom.id
        );
    updateTitle(m_displaySnapshot ? &*m_displaySnapshot : nullptr);

    if (m_detailsCard)
    {
        m_detailsCard->setTitle(
            tr("Class Details")
            );
    }

    if (m_scheduleCard)
    {
        m_scheduleCard->setTitle(
            tr("Class Times")
            );
    }

    if (m_detailsSection)
    {
        m_detailsSection->retranslateUi();
    }

    if (m_scheduleSection)
    {
        m_scheduleSection->retranslateUi();
    }

    updateScrollContentMinimumWidth();
    updateActions();
}

bool ClassDetailsPage::showScheduleConflicts(
    const QList<ClassTime>& times,
    ScheduleType type,
    const QString& title,
    bool showMessage
    )
{
    const auto typedClassId =
        ClassMngr::Next::Domain::ClassId::fromString(
            std::to_string(m_classroom.id)
            );
    const auto typedTimes = toDomainScheduleTimes(times);

    const auto loadedConflicts = [&]()
        -> ClassMngr::Next::Application::
            ClassDetailsScheduleConflictResult
    {
        if (!typedClassId || !typedTimes)
        {
            return ClassMngr::Next::Application::
                ClassDetailsScheduleConflictResult::failure({
                    .code = ClassMngr::Next::Domain::ErrorCode::Validation,
                    .message = "Class schedule values could not be checked.",
                    .recoverable = true
                });
        }

        const ClassMngr::Next::Application::
            ClassDetailsScheduleConflictRequest request{
                .classId = *typedClassId,
                .mode = type == ScheduleType::Regular
                    ? ClassMngr::Next::Application::
                          ClassDetailsScheduleMode::Regular
                    : ClassMngr::Next::Application::
                          ClassDetailsScheduleMode::Intensive,
                .candidateTimes = *typedTimes
            };

        ClassMngr::Next::Platform::
            ApplicationServicesClassDetailsScheduleConflictPort defaultPort(
                m_services
                );
        const ClassMngr::Next::Application::
            ClassDetailsScheduleConflictPort& port =
                m_scheduleConflictPort
                    ? *m_scheduleConflictPort
                    : defaultPort;

        return ClassMngr::Next::Application::
            ClassDetailsScheduleConflictQuery::execute(request, port);
    }();

    if (!loadedConflicts)
    {
        if (showMessage)
        {
            DialogServices::showWarning(
                this,
                title,
                displayText(loadedConflicts.error().message)
                );
        }

        return true;
    }

    const std::vector<
        ClassMngr::Next::Application::ClassDetailsScheduleConflict
        >& conflicts = loadedConflicts.value();

    if (conflicts.empty())
    {
        return false;
    }

    if (!showMessage)
    {
        return true;
    }

    QStringList details;

    for (const ClassMngr::Next::Application::
             ClassDetailsScheduleConflict& conflict : conflicts)
    {
        QString conflictingClass =
            QString::fromStdU16String(conflict.conflictingClassName);

        if (
            conflictingClass == QString::fromStdU16String(conflict.className)
            )
        {
            conflictingClass =
                tr("another time in this class");
        }

        details.append(
            tr("%1 %2-%3 conflicts with %4.")
                .arg(QString::fromStdU16String(conflict.day))
                .arg(QString::fromStdU16String(conflict.startTime))
                .arg(QString::fromStdU16String(conflict.endTime))
                .arg(conflictingClass)
            );
    }

    DialogServices::showWarning(
        this,
        title,
        tr("Please resolve these schedule conflicts before saving:\n\n%1")
            .arg(details.join('\n'))
        );

    return true;
}
