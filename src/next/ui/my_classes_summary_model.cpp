#include "next/ui/my_classes_summary_model.h"

#include "domain/config/class_details_catalog.h"
#include "domain/rules/schedule_value_parser.h"
#include "next/application/class_tab_navigation_ordering_policy.h"

#include <QCoreApplication>
#include <QFont>
#include <QHash>
#include <QLocale>
#include <QTime>

#include <algorithm>
#include <string>

namespace ClassMngr::Next::Ui
{
namespace
{

constexpr int UnknownOrder = 1'000;

QString translated(const char* value)
{
    return QCoreApplication::translate("NextMyClassesPage", value);
}

QString value(const std::u16string& source)
{
    return QString::fromStdU16String(source).trimmed();
}

int gradeRank(const QString& grade)
{
    const auto grades = ::ClassMngr::Domain::ClassDetailsCatalog::grades();
    const std::string candidate = grade.trimmed().toStdString();
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

int minuteOfDay(const QString& text)
{
    const auto parsed = ScheduleValueParser::parseDisplayTime(text.trimmed());
    return parsed
        ? parsed->hour() * 60 + parsed->minute()
        : UnknownOrder;
}

std::pair<int, int> firstScheduleOrder(
    const Application::MyClassesPageClassSummary& summary
    )
{
    const auto& schedule = summary.regularSchedule.empty()
        ? summary.intensiveSchedule
        : summary.regularSchedule;
    int day = UnknownOrder;
    for (const auto& entry : schedule)
    {
        day = std::min(day, weekdayRank(value(entry.day)));
    }
    int time = UnknownOrder;
    for (const auto& entry : schedule)
    {
        if (weekdayRank(value(entry.day)) == day)
        {
            time = std::min(time, minuteOfDay(value(entry.startTime)));
        }
    }
    return {day, time};
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
    case ScheduleValueParser::Weekday::Saturday: return QStringLiteral("Sat");
    case ScheduleValueParser::Weekday::Sunday: return QStringLiteral("Sun");
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
    if (codes == QStringList{QStringLiteral("W"), QStringLiteral("F")})
    {
        return QStringLiteral("W/F");
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

QString compactTime(const QString& source)
{
    const QString trimmed = source.trimmed();
    const auto parsed = ScheduleValueParser::parseDisplayTime(trimmed);
    if (!parsed)
    {
        QString result = trimmed;
        result.remove(QStringLiteral(" AM"), Qt::CaseInsensitive);
        result.remove(QStringLiteral(" PM"), Qt::CaseInsensitive);
        return result.trimmed();
    }
    const bool hasMeridiem = trimmed.contains(QStringLiteral("AM"), Qt::CaseInsensitive)
        || trimmed.contains(QStringLiteral("PM"), Qt::CaseInsensitive);
    if (!hasMeridiem)
    {
        return parsed->toString(QStringLiteral("H:mm"));
    }
    QString result = parsed->toString(QStringLiteral("h:mm AP"));
    result.remove(QStringLiteral(" AM"));
    result.remove(QStringLiteral(" PM"));
    return result;
}

QString preferredScheduleText(
    const Application::MyClassesPageClassSummary& summary
    )
{
    const bool intensive = summary.regularSchedule.empty();
    const auto& schedule = intensive
        ? summary.intensiveSchedule
        : summary.regularSchedule;
    if (schedule.empty())
    {
        return translated("No time");
    }

    struct TimeGroup final
    {
        QString start;
        QStringList days;
    };
    QList<TimeGroup> groups;
    for (const auto& entry : schedule)
    {
        const QString start = compactTime(value(entry.startTime));
        if (start.isEmpty())
        {
            continue;
        }
        auto found = std::find_if(groups.begin(), groups.end(),
            [&start](const TimeGroup& group) { return group.start == start; });
        if (found == groups.end())
        {
            groups.push_back({start, {value(entry.day)}});
        }
        else
        {
            const QString day = value(entry.day);
            if (!found->days.contains(day))
            {
                found->days.push_back(day);
            }
        }
    }
    QStringList labels;
    for (const TimeGroup& group : groups)
    {
        labels.push_back(QStringLiteral("%1 %2")
            .arg(compressedDays(group.days), group.start));
    }
    const QString scheduleText = labels.isEmpty()
        ? translated("No time")
        : labels.join(QStringLiteral("; "));
    return intensive
        ? QStringLiteral("%1 %2").arg(translated("Int"), scheduleText)
        : scheduleText;
}

QString classBaseLabel(
    const Application::MyClassesPageClassSummary& summary,
    const bool includeGrade
    )
{
    const QString grade = value(summary.grade);
    const QString level = value(summary.level);
    QString classLabel;
    if (includeGrade && !grade.isEmpty() && !level.isEmpty())
    {
        classLabel = QStringLiteral("%1 %2").arg(grade, level);
    }
    else if (!level.isEmpty())
    {
        classLabel = level;
    }
    else if (!grade.isEmpty())
    {
        classLabel = grade;
    }
    else
    {
        classLabel = value(summary.className);
        if (classLabel.isEmpty())
        {
            classLabel = translated("Class %1").arg(
                QString::fromStdString(summary.classId.value())
                );
        }
    }
    return QStringLiteral("%1 • %2")
        .arg(classLabel, preferredScheduleText(summary));
}

QString teacherLabel(const Application::MyClassesPageClassSummary& summary)
{
    const QString english = value(summary.teacherEnglishName);
    return english.isEmpty() ? value(summary.teacherKoreanName) : english;
}

Application::ClassTabNavigationOrderingFact orderingFact(
    const Application::MyClassesPageClassSummary& summary
    )
{
    const auto [day, time] = firstScheduleOrder(summary);
    return {
        .gradeRank = Application::ClassTabNavigationOrderRank{
            gradeRank(value(summary.grade))
        },
        .levelRank = Application::ClassTabNavigationOrderRank{
            levelRank(value(summary.grade), value(summary.level))
        },
        .earliestScheduleDayRank = Application::ClassTabNavigationOrderRank{day},
        .earliestStartMinuteRank = Application::ClassTabNavigationOrderRank{time},
        .baseLabel = classBaseLabel(summary, true).toStdU16String(),
        .classId = summary.classId
    };
}

} // namespace

MyClassesSummaryModel::MyClassesSummaryModel(QObject* parent)
    : QAbstractItemModel(parent)
{
}

QModelIndex MyClassesSummaryModel::index(
    const int row,
    const int column,
    const QModelIndex& parentIndex
    ) const
{
    if (row < 0 || column != 0 || columnCount(parentIndex) == 0)
    {
        return {};
    }
    if (!parentIndex.isValid())
    {
        if (m_grouped)
        {
            if (row >= static_cast<int>(m_groupItems.size()))
            {
                return {};
            }
            return createIndex(row, column,
                const_cast<Item*>(&m_groupItems[static_cast<std::size_t>(row)]));
        }
        if (row >= static_cast<int>(m_classItems.size()))
        {
            return {};
        }
        return createIndex(row, column,
            const_cast<Item*>(&m_classItems[static_cast<std::size_t>(row)]));
    }

    const auto* const parentItem = static_cast<const Item*>(
        parentIndex.internalPointer()
        );
    if (!parentItem || !parentItem->gradeGroup || !m_grouped
        || parentItem->groupIndex < 0
        || parentItem->groupIndex >= static_cast<int>(m_groupChildren.size()))
    {
        return {};
    }
    const auto& children = m_groupChildren[
        static_cast<std::size_t>(parentItem->groupIndex)
        ];
    if (row >= static_cast<int>(children.size()))
    {
        return {};
    }
    const int summaryRow = children[static_cast<std::size_t>(row)];
    return createIndex(row, column,
        const_cast<Item*>(&m_classItems[static_cast<std::size_t>(summaryRow)]));
}

QModelIndex MyClassesSummaryModel::parent(const QModelIndex& child) const
{
    if (!child.isValid())
    {
        return {};
    }
    const auto* const item = static_cast<const Item*>(child.internalPointer());
    if (!item || item->gradeGroup || item->groupIndex < 0 || !m_grouped)
    {
        return {};
    }
    return createIndex(item->groupIndex, 0,
        const_cast<Item*>(&m_groupItems[
            static_cast<std::size_t>(item->groupIndex)]));
}

int MyClassesSummaryModel::rowCount(const QModelIndex& parentIndex) const
{
    if (!parentIndex.isValid())
    {
        return m_grouped
            ? static_cast<int>(m_groupItems.size())
            : static_cast<int>(m_classItems.size());
    }
    if (parentIndex.column() != 0)
    {
        return 0;
    }
    const auto* const item = static_cast<const Item*>(parentIndex.internalPointer());
    if (!item || !item->gradeGroup || !m_grouped || item->groupIndex < 0
        || item->groupIndex >= static_cast<int>(m_groupChildren.size()))
    {
        return 0;
    }
    return static_cast<int>(m_groupChildren[
        static_cast<std::size_t>(item->groupIndex)].size());
}

int MyClassesSummaryModel::columnCount(const QModelIndex&) const
{
    return 1;
}

QVariant MyClassesSummaryModel::data(const QModelIndex& modelIndex, const int role) const
{
    if (!modelIndex.isValid())
    {
        return {};
    }
    const auto* const item = static_cast<const Item*>(modelIndex.internalPointer());
    if (!item)
    {
        return {};
    }
    if (item->gradeGroup)
    {
        if (role == Qt::DisplayRole)
        {
            return m_groupLabels[static_cast<std::size_t>(item->groupIndex)];
        }
        if (role == IsGradeGroupRole)
        {
            return true;
        }
        if (role == Qt::FontRole)
        {
            QFont font;
            font.setWeight(QFont::DemiBold);
            return font;
        }
        return {};
    }

    if (item->summaryIndex < 0
        || item->summaryIndex >= static_cast<int>(m_summaries.size()))
    {
        return {};
    }
    const auto& summary = m_summaries[
        static_cast<std::size_t>(item->summaryIndex)
        ];
    if (role == Qt::DisplayRole)
    {
        return classLabel(item->summaryIndex);
    }
    if (role == Qt::ToolTipRole)
    {
        return value(summary.className);
    }
    if (role == ClassIdRole)
    {
        return QString::fromStdString(summary.classId.value());
    }
    if (role == IsGradeGroupRole)
    {
        return false;
    }
    return {};
}

Qt::ItemFlags MyClassesSummaryModel::flags(const QModelIndex& modelIndex) const
{
    if (!modelIndex.isValid())
    {
        return Qt::NoItemFlags;
    }
    const auto* const item = static_cast<const Item*>(modelIndex.internalPointer());
    if (item && item->gradeGroup)
    {
        return Qt::ItemIsEnabled;
    }
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

void MyClassesSummaryModel::setSummaries(
    Application::MyClassesPageSummarySnapshot summaries
    )
{
    beginResetModel();
    m_summaries = std::move(summaries);
    rebuildHierarchy();
    endResetModel();
}

void MyClassesSummaryModel::clear()
{
    setSummaries({});
}

void MyClassesSummaryModel::retranslate()
{
    beginResetModel();
    rebuildHierarchy();
    endResetModel();
}

const Application::MyClassesPageClassSummary*
MyClassesSummaryModel::summaryForIndex(const QModelIndex& modelIndex) const noexcept
{
    const int index = summaryIndex(modelIndex);
    return index >= 0 && index < static_cast<int>(m_summaries.size())
        ? &m_summaries[static_cast<std::size_t>(index)]
        : nullptr;
}

QModelIndex MyClassesSummaryModel::indexForClassId(
    const Domain::ClassId& classId
    ) const
{
    const auto found = std::find_if(
        m_summaries.cbegin(), m_summaries.cend(),
        [&classId](const auto& summary) { return summary.classId == classId; }
        );
    if (found == m_summaries.cend())
    {
        return {};
    }
    const int row = static_cast<int>(std::distance(m_summaries.cbegin(), found));
    if (!m_grouped)
    {
        return index(row, 0);
    }
    const int group = m_classItems[static_cast<std::size_t>(row)].groupIndex;
    if (group < 0)
    {
        return {};
    }
    const auto& children = m_groupChildren[static_cast<std::size_t>(group)];
    const auto child = std::find(children.cbegin(), children.cend(), row);
    if (child == children.cend())
    {
        return {};
    }
    return index(static_cast<int>(std::distance(children.cbegin(), child)), 0,
        index(group, 0));
}

const Application::MyClassesPageSummarySnapshot&
MyClassesSummaryModel::summaries() const noexcept
{
    return m_summaries;
}

int MyClassesSummaryModel::summaryCount() const noexcept
{
    return static_cast<int>(m_summaries.size());
}

bool MyClassesSummaryModel::isGrouped() const noexcept
{
    return m_grouped;
}

int MyClassesSummaryModel::gradeGroupCount() const noexcept
{
    return static_cast<int>(m_groupItems.size());
}

int MyClassesSummaryModel::gradeGroupChildCount(const int row) const noexcept
{
    return row >= 0 && row < static_cast<int>(m_groupChildren.size())
        ? static_cast<int>(m_groupChildren[static_cast<std::size_t>(row)].size())
        : 0;
}

QString MyClassesSummaryModel::classLabel(const int summaryIndexValue) const
{
    if (summaryIndexValue < 0
        || summaryIndexValue >= static_cast<int>(m_classLabels.size()))
    {
        return {};
    }
    return m_classLabels[static_cast<std::size_t>(summaryIndexValue)];
}

int MyClassesSummaryModel::summaryIndex(const QModelIndex& modelIndex) const noexcept
{
    if (!modelIndex.isValid())
    {
        return -1;
    }
    const auto* const item = static_cast<const Item*>(modelIndex.internalPointer());
    return item && !item->gradeGroup ? item->summaryIndex : -1;
}

int MyClassesSummaryModel::groupIndexForGrade(const QString& grade) const noexcept
{
    const QString key = gradeRank(grade) == UnknownOrder ? QString{} : grade.trimmed();
    const auto found = std::find_if(m_groupLabels.cbegin(), m_groupLabels.cend(),
        [&key](const QString& candidate) {
            return candidate == (key.isEmpty() ? translated("Other") : key);
        });
    return found == m_groupLabels.cend()
        ? -1
        : static_cast<int>(std::distance(m_groupLabels.cbegin(), found));
}

void MyClassesSummaryModel::rebuildHierarchy()
{
    std::stable_sort(
        m_summaries.begin(), m_summaries.end(),
        [](const auto& left, const auto& right) {
            const auto leftFact = orderingFact(left);
            const auto rightFact = orderingFact(right);
            return Application::classTabNavigationFactLessThan(
                leftFact,
                rightFact,
                [](const std::u16string_view leftLabel,
                   const std::u16string_view rightLabel) {
                    return QString::localeAwareCompare(
                        QString::fromStdU16String(std::u16string(leftLabel)),
                        QString::fromStdU16String(std::u16string(rightLabel))
                        );
                }
                );
        }
        );

    m_grouped = m_summaries.size() > 6;
    m_classItems.clear();
    m_groupItems.clear();
    m_groupLabels.clear();
    m_groupChildren.clear();
    m_classLabels.clear();
    m_classItems.resize(m_summaries.size());
    m_classLabels.reserve(m_summaries.size());

    QHash<QString, QVector<int>> rowsByBaseLabel;
    for (int row = 0; row < static_cast<int>(m_summaries.size()); ++row)
    {
        m_classLabels.push_back(classBaseLabel(
            m_summaries[static_cast<std::size_t>(row)], !m_grouped
            ));
        rowsByBaseLabel[m_classLabels.back()].push_back(row);
    }
    for (auto it = rowsByBaseLabel.cbegin(); it != rowsByBaseLabel.cend(); ++it)
    {
        if (it.value().size() < 2)
        {
            continue;
        }
        QHash<QString, int> expandedCounts;
        for (const int row : it.value())
        {
            const auto& summary = m_summaries[static_cast<std::size_t>(row)];
            const QString teacher = teacherLabel(summary);
            const QString expanded = teacher.isEmpty()
                ? it.key()
                : QStringLiteral("%1 • %2").arg(it.key(), teacher);
            ++expandedCounts[expanded];
            m_classLabels[static_cast<std::size_t>(row)] = expanded;
        }
        for (const int row : it.value())
        {
            QString& label = m_classLabels[static_cast<std::size_t>(row)];
            if (expandedCounts.value(label) > 1)
            {
                label += QStringLiteral(" #%1").arg(QString::fromStdString(
                    m_summaries[static_cast<std::size_t>(row)].classId.value()
                    ));
            }
        }
    }

    if (!m_grouped)
    {
        for (int row = 0; row < static_cast<int>(m_classItems.size()); ++row)
        {
            m_classItems[static_cast<std::size_t>(row)] = {
                .gradeGroup = false,
                .summaryIndex = row,
                .groupIndex = -1
            };
        }
        return;
    }

    for (const auto& summary : m_summaries)
    {
        const QString key = gradeRank(value(summary.grade)) == UnknownOrder
            ? QString{}
            : value(summary.grade);
        const QString label = key.isEmpty() ? translated("Other") : key;
        if (std::find(m_groupLabels.cbegin(), m_groupLabels.cend(), label)
            == m_groupLabels.cend())
        {
            m_groupLabels.push_back(label);
            m_groupChildren.emplace_back();
        }
    }
    m_groupItems.resize(m_groupLabels.size());
    for (int group = 0; group < static_cast<int>(m_groupItems.size()); ++group)
    {
        m_groupItems[static_cast<std::size_t>(group)] = {
            .gradeGroup = true,
            .summaryIndex = -1,
            .groupIndex = group
        };
    }
    for (int row = 0; row < static_cast<int>(m_summaries.size()); ++row)
    {
        const int group = groupIndexForGrade(
            value(m_summaries[static_cast<std::size_t>(row)].grade)
            );
        m_classItems[static_cast<std::size_t>(row)] = {
            .gradeGroup = false,
            .summaryIndex = row,
            .groupIndex = group
        };
        if (group >= 0)
        {
            m_groupChildren[static_cast<std::size_t>(group)].push_back(row);
        }
    }
}

} // namespace ClassMngr::Next::Ui
