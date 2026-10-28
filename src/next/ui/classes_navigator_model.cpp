#include "next/ui/classes_navigator_model.h"

#include "domain/config/class_details_catalog.h"
#include "domain/rules/schedule_value_parser.h"
#include "next/application/class_tab_navigation_ordering_policy.h"

#include <QCoreApplication>
#include <QFont>
#include <QLocale>
#include <QTime>
#include <QVariant>

#include <algorithm>
#include <limits>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace ClassMngr::Next::Ui
{
namespace
{

constexpr int UnknownOrder = 1'000;

QString translated(const char* value)
{
    return QCoreApplication::translate("NextClassesPage", value);
}

QString fromValue(const std::u16string& value)
{
    return QString::fromStdU16String(value).trimmed();
}

const Application::ClassesNavigationDetails* detailsFor(
    const Application::ClassesNavigationSnapshot* navigation,
    const Domain::ClassId& classId
    )
{
    if (!navigation)
    {
        return nullptr;
    }
    const auto found = std::find_if(
        navigation->classes.cbegin(), navigation->classes.cend(),
        [&classId](const auto& entry) { return entry.classId == classId; }
        );
    return found != navigation->classes.cend() && found->details
        ? &found->details.value()
        : nullptr;
}

int weekdayRank(const QString& day)
{
    const QString trimmed = day.trimmed();
    const auto parsed = ScheduleValueParser::parseWeekday(trimmed);
    if (!parsed || parsed->text != trimmed)
    {
        return UnknownOrder;
    }
    switch (parsed->value)
    {
    case ScheduleValueParser::Weekday::Monday: return 0;
    case ScheduleValueParser::Weekday::Tuesday: return 1;
    case ScheduleValueParser::Weekday::Wednesday: return 2;
    case ScheduleValueParser::Weekday::Thursday: return 3;
    case ScheduleValueParser::Weekday::Friday: return 4;
    case ScheduleValueParser::Weekday::Saturday: return 5;
    case ScheduleValueParser::Weekday::Sunday: return 6;
    }
    return UnknownOrder;
}

int gradeRank(const QString& grade)
{
    const std::string candidate = grade.trimmed().toStdString();
    const auto grades = ::ClassMngr::Domain::ClassDetailsCatalog::grades();
    const auto found = std::find(grades.begin(), grades.end(), candidate);
    return found == grades.end()
        ? UnknownOrder
        : static_cast<int>(std::distance(grades.begin(), found));
}

int levelRank(const QString& grade, const QString& level)
{
    const auto levels = ::ClassMngr::Domain::ClassDetailsCatalog::levelsForGrade(
        grade.trimmed().toStdString()
        );
    const std::string candidate = level.trimmed().toStdString();
    const auto found = std::find(levels.begin(), levels.end(), candidate);
    return found == levels.end()
        ? UnknownOrder
        : static_cast<int>(std::distance(levels.begin(), found));
}

const std::vector<Application::ClassesNavigationScheduleRow>& preferredSchedule(
    const Application::ClassesNavigationDetails& details
    )
{
    return details.regularSchedule.empty()
        ? details.intensiveSchedule
        : details.regularSchedule;
}

QString compactStartTime(const QString& source)
{
    const QString trimmed = source.trimmed();
    if (trimmed.isEmpty())
    {
        return {};
    }
    const bool hasMeridiem = trimmed.contains(
            QStringLiteral("AM"), Qt::CaseInsensitive)
        || trimmed.contains(QStringLiteral("PM"), Qt::CaseInsensitive);
    const auto parsed = ScheduleValueParser::parseDisplayTime(trimmed);
    if (parsed)
    {
        if (!hasMeridiem)
        {
            return parsed->toString(QStringLiteral("H:mm"));
        }
        QString value = parsed->toString(QStringLiteral("h:mm AP"));
        value.remove(QStringLiteral(" AM"));
        value.remove(QStringLiteral(" PM"));
        return value;
    }
    QString fallback = trimmed;
    fallback.remove(QStringLiteral(" AM"), Qt::CaseInsensitive);
    fallback.remove(QStringLiteral(" PM"), Qt::CaseInsensitive);
    return fallback.trimmed();
}

QString dayCode(const QString& source)
{
    const QString trimmed = source.trimmed();
    const auto parsed = ScheduleValueParser::parseWeekday(trimmed);
    if (!parsed || parsed->text != trimmed)
    {
        return trimmed;
    }
    switch (parsed->value)
    {
    case ScheduleValueParser::Weekday::Monday: return QStringLiteral("M");
    case ScheduleValueParser::Weekday::Tuesday: return QStringLiteral("T");
    case ScheduleValueParser::Weekday::Wednesday: return QStringLiteral("W");
    case ScheduleValueParser::Weekday::Thursday: return QStringLiteral("Th");
    case ScheduleValueParser::Weekday::Friday: return QStringLiteral("F");
    case ScheduleValueParser::Weekday::Saturday: return translated("Sat");
    case ScheduleValueParser::Weekday::Sunday: return translated("Sun");
    }
    return trimmed;
}

QString compressedDays(QStringList days)
{
    days.removeDuplicates();
    std::sort(days.begin(), days.end(), [](const QString& left, const QString& right) {
        return weekdayRank(left) < weekdayRank(right);
    });
    QStringList codes;
    for (const QString& day : days)
    {
        const QString code = dayCode(day);
        if (!code.isEmpty())
        {
            codes.push_back(code);
        }
    }
    if (codes == QStringList{QStringLiteral("M"), QStringLiteral("W")})
    {
        return QStringLiteral("M/W");
    }
    if (codes == QStringList{QStringLiteral("M"), QStringLiteral("F")})
    {
        return QStringLiteral("M/F");
    }
    if (codes == QStringList{
            QStringLiteral("M"), QStringLiteral("W"), QStringLiteral("F")})
    {
        return QStringLiteral("M/W/F");
    }
    if (codes == QStringList{QStringLiteral("T"), QStringLiteral("Th")})
    {
        return QStringLiteral("T/Th");
    }
    return codes.join(QLatin1Char('/'));
}

QString preferredScheduleText(
    const Application::ClassesNavigationDetails* details
    )
{
    if (!details)
    {
        return {};
    }
    const bool intensive = details->regularSchedule.empty();
    const auto& rows = preferredSchedule(*details);
    if (rows.empty())
    {
        return translated("No time");
    }

    struct TimeGroup final
    {
        QString start;
        QStringList days;
    };
    QList<TimeGroup> groups;
    for (const auto& row : rows)
    {
        const QString start = compactStartTime(fromValue(row.startTime));
        if (start.isEmpty())
        {
            continue;
        }
        const QString day = fromValue(row.day);
        auto found = std::find_if(groups.begin(), groups.end(),
            [&start](const TimeGroup& group) { return group.start == start; });
        if (found == groups.end())
        {
            groups.push_back({start, {day}});
        }
        else if (!found->days.contains(day))
        {
            found->days.push_back(day);
        }
    }

    QStringList labels;
    for (const TimeGroup& group : groups)
    {
        labels.push_back(QStringLiteral("%1 %2")
            .arg(compressedDays(group.days), group.start));
    }
    const QString schedule = labels.isEmpty()
        ? translated("No time")
        : labels.join(QStringLiteral("; "));
    return intensive
        ? QStringLiteral("%1 %2").arg(translated("Int"), schedule)
        : schedule;
}

QString classNameText(
    const Application::ClassesListEntry& classroom,
    const Application::ClassesNavigationDetails* details,
    const bool includeGrade
    )
{
    const QString grade = details ? fromValue(details->grade) : QString{};
    const QString level = details ? fromValue(details->level) : QString{};
    if (includeGrade && !grade.isEmpty() && !level.isEmpty())
    {
        return QStringLiteral("%1 %2").arg(grade, level);
    }
    if (!level.isEmpty())
    {
        return level;
    }
    if (!grade.isEmpty())
    {
        return grade;
    }
    const QString name = fromValue(classroom.className);
    return name.isEmpty()
        ? translated("Class %1").arg(QString::fromStdString(
            classroom.classId.value()))
        : name;
}

QString baseLabel(
    const Application::ClassesListEntry& classroom,
    const Application::ClassesNavigationDetails* details,
    const bool includeGrade
    )
{
    const QString name = classNameText(classroom, details, includeGrade);
    if (!details)
    {
        return name;
    }
    return QStringLiteral("%1 • %2")
        .arg(name, preferredScheduleText(details));
}

QString teacherLabel(const Application::ClassesNavigationDetails* details)
{
    if (!details)
    {
        return {};
    }
    const QString english = fromValue(details->teacherEnglishName);
    return english.isEmpty()
        ? fromValue(details->teacherKoreanName)
        : english;
}

Application::ClassTabNavigationOrderingFact orderingFact(
    const Application::ClassesListEntry& classroom,
    const Application::ClassesNavigationDetails* details
    )
{
    int firstDay = UnknownOrder;
    int firstTime = UnknownOrder;
    if (details)
    {
        const auto& schedule = preferredSchedule(*details);
        for (const auto& row : schedule)
        {
            firstDay = std::min(firstDay, weekdayRank(fromValue(row.day)));
        }
        for (const auto& row : schedule)
        {
            if (weekdayRank(fromValue(row.day)) != firstDay)
            {
                continue;
            }
            const auto time = ScheduleValueParser::parseDisplayTime(
                fromValue(row.startTime)
                );
            if (time)
            {
                firstTime = std::min(
                    firstTime,
                    time->hour() * 60 + time->minute()
                    );
            }
        }
    }
    const QString grade = details ? fromValue(details->grade) : QString{};
    const QString level = details ? fromValue(details->level) : QString{};
    return {
        .gradeRank = Application::ClassTabNavigationOrderRank{gradeRank(grade)},
        .levelRank = Application::ClassTabNavigationOrderRank{
            levelRank(grade, level)
        },
        .earliestScheduleDayRank =
            Application::ClassTabNavigationOrderRank{firstDay},
        .earliestStartMinuteRank =
            Application::ClassTabNavigationOrderRank{firstTime},
        .baseLabel = baseLabel(classroom, details, true).toStdU16String(),
        .classId = classroom.classId
    };
}

int compareLabels(
    const std::u16string_view left,
    const std::u16string_view right
    )
{
    return QString::localeAwareCompare(
        QString::fromStdU16String(std::u16string(left)),
        QString::fromStdU16String(std::u16string(right))
        );
}

bool selectedDayFilterMatches(
    const Application::ClassDayFilterPolicy& filter,
    const Application::ClassesNavigationDetails* details
    )
{
    if (!details)
    {
        return true;
    }
    Application::ClassDayScheduleDays schedule;
    schedule.regularDayKeys.reserve(details->regularSchedule.size());
    schedule.intensiveDayKeys.reserve(details->intensiveSchedule.size());
    for (const auto& row : details->regularSchedule)
    {
        schedule.regularDayKeys.push_back(
            fromValue(row.day).toCaseFolded().toUtf8().toStdString()
            );
    }
    for (const auto& row : details->intensiveSchedule)
    {
        schedule.intensiveDayKeys.push_back(
            fromValue(row.day).toCaseFolded().toUtf8().toStdString()
            );
    }
    return Application::classDayFilterMatches(filter, schedule);
}

} // namespace

ClassesNavigatorModel::ClassesNavigatorModel(QObject* parent)
    : QAbstractListModel(parent)
{
}

int ClassesNavigatorModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid())
    {
        return 0;
    }
    const std::vector<Row>* rows = rowsForGroup(m_currentGroupKey);
    return rows ? static_cast<int>(rows->size()) : 0;
}

QVariant ClassesNavigatorModel::data(
    const QModelIndex& modelIndex,
    const int role
    ) const
{
    if (!modelIndex.isValid() || modelIndex.parent().isValid())
    {
        return {};
    }
    const auto* rows = rowsForGroup(m_currentGroupKey);
    if (!rows || modelIndex.row() < 0
        || modelIndex.row() >= static_cast<int>(rows->size()))
    {
        return {};
    }
    const Row& row = rows->at(static_cast<std::size_t>(modelIndex.row()));
    if (row.classIndex >= m_classes.classes.size())
    {
        return {};
    }
    if (role == Qt::DisplayRole)
    {
        return row.label;
    }
    if (role == Qt::ToolTipRole)
    {
        return fromValue(m_classes.classes[row.classIndex].className);
    }
    if (role == ClassIdRole)
    {
        return QString::fromStdString(
            m_classes.classes[row.classIndex].classId.value()
            );
    }
    return {};
}

Qt::ItemFlags ClassesNavigatorModel::flags(const QModelIndex& index) const
{
    return index.isValid()
        ? Qt::ItemIsEnabled | Qt::ItemIsSelectable
        : Qt::NoItemFlags;
}

void ClassesNavigatorModel::setSnapshot(
    Application::ClassesListSnapshot classes,
    std::optional<Application::ClassesNavigationSnapshot> navigation,
    Application::ClassDayFilterPolicy filter
    )
{
    beginResetModel();
    m_classes = std::move(classes);
    m_navigation = std::move(navigation);
    m_filter = std::move(filter);
    rebuild();
    endResetModel();
}

void ClassesNavigatorModel::setCurrentGroup(const QString& key)
{
    if (key == m_currentGroupKey || !rowsForGroup(key))
    {
        return;
    }
    beginResetModel();
    m_currentGroupKey = key;
    endResetModel();
}

void ClassesNavigatorModel::clear()
{
    beginResetModel();
    m_classes = {};
    m_navigation.reset();
    m_gradeGroups.clear();
    m_allGroup = {};
    m_groups.clear();
    m_currentGroupKey.clear();
    endResetModel();
}

void ClassesNavigatorModel::retranslate()
{
    beginResetModel();
    rebuild();
    endResetModel();
}

const std::vector<ClassesNavigatorModel::Group>&
ClassesNavigatorModel::groups() const noexcept
{
    return m_groups;
}

const QString& ClassesNavigatorModel::currentGroup() const noexcept
{
    return m_currentGroupKey;
}

int ClassesNavigatorModel::classCount() const noexcept
{
    return static_cast<int>(m_classes.classes.size());
}

int ClassesNavigatorModel::visibleClassCount() const noexcept
{
    return static_cast<int>(m_allGroup.rows.size());
}

int ClassesNavigatorModel::gradeGroupCount() const noexcept
{
    return static_cast<int>(m_gradeGroups.size());
}

bool ClassesNavigatorModel::hasNavigationMetadata() const noexcept
{
    return m_navigation.has_value();
}

std::optional<Domain::ClassId> ClassesNavigatorModel::classIdAt(
    const QModelIndex& index
    ) const
{
    if (!index.isValid() || index.parent().isValid())
    {
        return std::nullopt;
    }
    return classIdAtRow(index.row());
}

std::optional<Domain::ClassId> ClassesNavigatorModel::classIdAtRow(
    const int row
    ) const
{
    const auto* rows = rowsForGroup(m_currentGroupKey);
    if (!rows || row < 0 || row >= static_cast<int>(rows->size()))
    {
        return std::nullopt;
    }
    const Row& item = rows->at(static_cast<std::size_t>(row));
    return item.classIndex < m_classes.classes.size()
        ? std::optional<Domain::ClassId>(
            m_classes.classes[item.classIndex].classId)
        : std::nullopt;
}

QModelIndex ClassesNavigatorModel::indexForClassId(
    const Domain::ClassId& classId
    ) const
{
    const auto* rows = rowsForGroup(m_currentGroupKey);
    if (!rows)
    {
        return {};
    }
    const auto found = std::find_if(rows->cbegin(), rows->cend(),
        [this, &classId](const Row& row) {
            return row.classIndex < m_classes.classes.size()
                && m_classes.classes[row.classIndex].classId == classId;
        });
    return found == rows->cend()
        ? QModelIndex{}
        : index(static_cast<int>(std::distance(rows->cbegin(), found)), 0);
}

QString ClassesNavigatorModel::labelForClassId(
    const Domain::ClassId& classId
    ) const
{
    for (const Row& row : m_allGroup.rows)
    {
        if (row.classIndex < m_classes.classes.size()
            && m_classes.classes[row.classIndex].classId == classId)
        {
            return row.label;
        }
    }
    return {};
}

std::vector<Domain::ClassId> ClassesNavigatorModel::classIdsForGroup(
    const QString& key
    ) const
{
    std::vector<Domain::ClassId> result;
    const auto* rows = rowsForGroup(key);
    if (!rows)
    {
        return result;
    }
    result.reserve(rows->size());
    for (const Row& row : *rows)
    {
        if (row.classIndex < m_classes.classes.size())
        {
            result.push_back(m_classes.classes[row.classIndex].classId);
        }
    }
    return result;
}

QString ClassesNavigatorModel::allClassesGroupKey()
{
    return QStringLiteral("__classes_all__");
}

const std::vector<ClassesNavigatorModel::Row>*
ClassesNavigatorModel::rowsForGroup(const QString& key) const noexcept
{
    if (key == m_allGroup.group.key && !m_allGroup.group.key.isEmpty())
    {
        return &m_allGroup.rows;
    }
    const auto found = std::find_if(
        m_gradeGroups.cbegin(), m_gradeGroups.cend(),
        [&key](const GradeGroup& group) { return group.group.key == key; }
        );
    return found == m_gradeGroups.cend() ? nullptr : &found->rows;
}

std::vector<ClassesNavigatorModel::Row>*
ClassesNavigatorModel::rowsForGroup(const QString& key) noexcept
{
    if (key == m_allGroup.group.key && !m_allGroup.group.key.isEmpty())
    {
        return &m_allGroup.rows;
    }
    const auto found = std::find_if(
        m_gradeGroups.begin(), m_gradeGroups.end(),
        [&key](const GradeGroup& group) { return group.group.key == key; }
        );
    return found == m_gradeGroups.end() ? nullptr : &found->rows;
}

void ClassesNavigatorModel::rebuild()
{
    m_gradeGroups.clear();
    m_groups.clear();
    m_allGroup = {};
    m_allGroup.group = {allClassesGroupKey(), translated("All")};

    struct SortRow final
    {
        std::size_t classIndex = 0;
        Application::ClassTabNavigationOrderingFact fact;
    };
    std::vector<SortRow> sorted;
    sorted.reserve(m_classes.classes.size());
    for (std::size_t index = 0; index < m_classes.classes.size(); ++index)
    {
        const auto& classroom = m_classes.classes[index];
        const auto* details = detailsFor(
            m_navigation ? &*m_navigation : nullptr,
            classroom.classId
            );
        if (!selectedDayFilterMatches(m_filter, details))
        {
            continue;
        }
        sorted.push_back({index, orderingFact(classroom, details)});
    }
    std::sort(sorted.begin(), sorted.end(), [](const SortRow& left, const SortRow& right) {
        return Application::classTabNavigationFactLessThan(
            left.fact, right.fact, compareLabels);
    });

    const auto makeRows = [this](
        const std::vector<std::size_t>& classIndexes,
        const bool includeGrade
        ) {
        struct Candidate final
        {
            Row row;
            QString base;
            QString teacher;
            QString id;
        };
        std::vector<Candidate> candidates;
        candidates.reserve(classIndexes.size());
        for (const std::size_t index : classIndexes)
        {
            const auto& classroom = m_classes.classes[index];
            const auto* details = detailsFor(
                m_navigation ? &*m_navigation : nullptr,
                classroom.classId
                );
            candidates.push_back({
                .row = {index, {}},
                .base = baseLabel(classroom, details, includeGrade),
                .teacher = teacherLabel(details),
                .id = QString::fromStdString(classroom.classId.value())
            });
        }
        std::vector<Row> rows;
        rows.reserve(candidates.size());
        for (Candidate& candidate : candidates)
        {
            const auto baseCount = std::count_if(
                candidates.cbegin(), candidates.cend(),
                [&candidate](const Candidate& other) {
                    return other.base == candidate.base;
                }
                );
            QString label = candidate.base;
            if (baseCount > 1)
            {
                QString expanded = candidate.base;
                if (!candidate.teacher.isEmpty())
                {
                    expanded = QStringLiteral("%1 • %2")
                        .arg(candidate.base, candidate.teacher);
                }
                const bool teacherDuplicate = std::any_of(
                    candidates.cbegin(), candidates.cend(),
                    [&candidate, &expanded](const Candidate& other) {
                        return other.row.classIndex != candidate.row.classIndex
                            && other.base == candidate.base
                            && (other.teacher.isEmpty()
                                ? other.base
                                : QStringLiteral("%1 • %2")
                                    .arg(other.base, other.teacher)) == expanded;
                    }
                    );
                label = candidate.teacher.isEmpty() || teacherDuplicate
                    ? QStringLiteral("%1 #%2").arg(expanded, candidate.id)
                    : expanded;
            }
            candidate.row.label = std::move(label);
            rows.push_back(std::move(candidate.row));
        }
        return rows;
    };

    std::vector<std::size_t> allIndexes;
    allIndexes.reserve(sorted.size());
    for (const SortRow& item : sorted)
    {
        allIndexes.push_back(item.classIndex);
    }
    m_allGroup.rows = makeRows(allIndexes, true);

    struct PendingGrade final
    {
        QString key;
        QString label;
        int rank = UnknownOrder;
        std::vector<std::size_t> indexes;
    };
    std::vector<PendingGrade> pending;
    for (const SortRow& item : sorted)
    {
        const auto& classroom = m_classes.classes[item.classIndex];
        const auto* details = detailsFor(
            m_navigation ? &*m_navigation : nullptr,
            classroom.classId
            );
        const QString grade = details ? fromValue(details->grade) : QString{};
        const int rank = gradeRank(grade);
        const QString key = rank == UnknownOrder
            ? QStringLiteral("__classes_other__")
            : QStringLiteral("grade:") + grade;
        auto found = std::find_if(pending.begin(), pending.end(),
            [&key](const PendingGrade& group) { return group.key == key; });
        if (found == pending.end())
        {
            pending.push_back({
                .key = key,
                .label = rank == UnknownOrder ? translated("Other") : grade,
                .rank = rank,
                .indexes = {item.classIndex}
            });
        }
        else
        {
            found->indexes.push_back(item.classIndex);
        }
    }
    std::sort(pending.begin(), pending.end(), [](const auto& left, const auto& right) {
        if (left.rank != right.rank)
        {
            return left.rank < right.rank;
        }
        return QString::localeAwareCompare(left.label, right.label) < 0;
    });

    for (PendingGrade& pendingGroup : pending)
    {
        GradeGroup group;
        group.group = {pendingGroup.key, pendingGroup.label};
        group.rows = makeRows(pendingGroup.indexes, false);
        m_gradeGroups.push_back(std::move(group));
    }

    m_groups.reserve(m_gradeGroups.size() + (m_allGroup.rows.empty() ? 0 : 1));
    for (const GradeGroup& group : m_gradeGroups)
    {
        m_groups.push_back(group.group);
    }
    if (!m_allGroup.rows.empty())
    {
        m_groups.push_back(m_allGroup.group);
    }

    if (!rowsForGroup(m_currentGroupKey))
    {
        m_currentGroupKey = m_allGroup.rows.empty()
            ? QString{}
            : m_allGroup.group.key;
    }
}

} // namespace ClassMngr::Next::Ui
