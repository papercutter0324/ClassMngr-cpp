#include "sub_prep_class_information_list_model.h"

#include "features/classes/config/class_info_config.h"

#include <QHash>
#include <QVariant>

#include <algorithm>
#include <string>
#include <utility>

namespace
{
constexpr int UnknownOrder = 1'000;

QString fromUtf8(
    const std::string& value
    )
{
    return QString::fromUtf8(
        value.data(),
        static_cast<qsizetype>(value.size())
        );
}

int gradeOrder(
    const QString& grade
    )
{
    const int index = ClassInfoConfig::Grades.indexOf(grade);
    return index >= 0 ? index : UnknownOrder;
}

int levelOrder(
    const QString& grade,
    const QString& level
    )
{
    const int index = ClassInfoConfig::levelsForGrade(grade).indexOf(level);
    return index >= 0 ? index : UnknownOrder;
}

QString classLabel(
    const ClassMngr::Next::Application::ClassSummary& summary
    )
{
    const QString level = fromUtf8(summary.level).trimmed();
    if (!level.isEmpty())
    {
        return level;
    }

    return fromUtf8(summary.displayLabel).trimmed();
}

QString baseClassLabel(
    const ClassMngr::Next::Application::ClassSummary& summary
    )
{
    const QString name = classLabel(summary);
    const QString meeting = fromUtf8(summary.meetingText).trimmed();
    if (name.isEmpty())
    {
        return meeting;
    }
    if (meeting.isEmpty())
    {
        return name;
    }

    return QStringLiteral("%1 • %2").arg(name, meeting);
}
}

SubPrepClassInformationListModel::SubPrepClassInformationListModel(
    QObject* parent
    )
    : QAbstractListModel(parent)
{
}

int SubPrepClassInformationListModel::rowCount(
    const QModelIndex& parent
    ) const
{
    return parent.isValid()
        ? 0
        : static_cast<int>(m_visibleRows.size());
}

QVariant SubPrepClassInformationListModel::data(
    const QModelIndex& index,
    int role
    ) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= rowCount())
    {
        return {};
    }

    const auto* summary = summaryAt(index.row());
    if (!summary)
    {
        return {};
    }

    switch (role)
    {
    case Qt::DisplayRole:
        return m_displayLabels.value(index.row());
    case Qt::ToolTipRole:
        return QStringLiteral("%1 (%2)")
            .arg(
                fromUtf8(summary->displayLabel),
                fromUtf8(summary->id.value())
                );
    case ClassIdRole:
        return fromUtf8(summary->id.value());
    case GradeRole:
        return fromUtf8(summary->grade);
    case LevelRole:
        return fromUtf8(summary->level);
    case MeetingTextRole:
        return fromUtf8(summary->meetingText);
    case TeacherDisplayNameRole:
        return teacherDisplayName(*summary);
    case StudentCountRole:
        return static_cast<qulonglong>(summary->studentCount);
    default:
        return {};
    }
}

QHash<int, QByteArray> SubPrepClassInformationListModel::roleNames() const
{
    return {
        {ClassIdRole, "classId"},
        {GradeRole, "grade"},
        {LevelRole, "level"},
        {MeetingTextRole, "meetingText"},
        {TeacherDisplayNameRole, "teacherDisplayName"},
        {StudentCountRole, "studentCount"}
    };
}

void SubPrepClassInformationListModel::setProjection(
    ClassMngr::Next::Application::ClassSummaryProjection projection
    )
{
    const QString previousGrade = m_currentGrade;
    beginResetModel();
    m_projection = std::move(projection);

    m_grades.clear();
    for (const auto& summary : m_projection.classes())
    {
        const QString grade = fromUtf8(summary.grade).trimmed();
        if (!m_grades.contains(grade))
        {
            m_grades.append(grade);
        }
    }

    std::sort(
        m_grades.begin(),
        m_grades.end(),
        [](const QString& left, const QString& right)
        {
            const int leftOrder = gradeOrder(left);
            const int rightOrder = gradeOrder(right);
            if (leftOrder != rightOrder)
            {
                return leftOrder < rightOrder;
            }
            if (leftOrder != UnknownOrder)
            {
                return false;
            }
            return QString::localeAwareCompare(left, right) < 0;
        }
        );

    m_currentGrade = m_grades.contains(previousGrade)
        ? previousGrade
        : m_grades.value(0);
    rebuildVisibleRows();
    endResetModel();
}

void SubPrepClassInformationListModel::setCurrentGrade(
    const QString& grade
    )
{
    if (grade == m_currentGrade || !m_grades.contains(grade))
    {
        return;
    }

    beginResetModel();
    m_currentGrade = grade;
    rebuildVisibleRows();
    endResetModel();
}

const QStringList& SubPrepClassInformationListModel::grades() const noexcept
{
    return m_grades;
}

const QString& SubPrepClassInformationListModel::currentGrade() const noexcept
{
    return m_currentGrade;
}

const ClassMngr::Next::Application::ClassSummaryProjection&
SubPrepClassInformationListModel::projection() const noexcept
{
    return m_projection;
}

const ClassMngr::Next::Application::ClassSummary*
SubPrepClassInformationListModel::summaryAt(int row) const noexcept
{
    if (row < 0 || static_cast<std::size_t>(row) >= m_visibleRows.size())
    {
        return nullptr;
    }

    const std::size_t projectionIndex =
        m_visibleRows[static_cast<std::size_t>(row)];
    const auto& summaries = m_projection.classes();
    return projectionIndex < summaries.size()
        ? &summaries[projectionIndex]
        : nullptr;
}

std::optional<ClassMngr::Next::Domain::ClassId>
SubPrepClassInformationListModel::classIdAt(int row) const
{
    const auto* summary = summaryAt(row);
    return summary
        ? std::optional<ClassMngr::Next::Domain::ClassId>(summary->id)
        : std::nullopt;
}

int SubPrepClassInformationListModel::rowForClassId(
    const ClassMngr::Next::Domain::ClassId& classId
    ) const noexcept
{
    for (std::size_t row = 0; row < m_visibleRows.size(); ++row)
    {
        const auto* summary = summaryAt(static_cast<int>(row));
        if (summary && summary->id == classId)
        {
            return static_cast<int>(row);
        }
    }

    return -1;
}

void SubPrepClassInformationListModel::rebuildVisibleRows()
{
    m_visibleRows.clear();
    const auto& summaries = m_projection.classes();
    for (std::size_t index = 0; index < summaries.size(); ++index)
    {
        if (fromUtf8(summaries[index].grade).trimmed() == m_currentGrade)
        {
            m_visibleRows.push_back(index);
        }
    }

    std::stable_sort(
        m_visibleRows.begin(),
        m_visibleRows.end(),
        [&summaries](std::size_t leftIndex, std::size_t rightIndex)
        {
            const auto& left = summaries[leftIndex];
            const auto& right = summaries[rightIndex];
            const QString grade = fromUtf8(left.grade).trimmed();
            const int leftLevel = levelOrder(grade, fromUtf8(left.level).trimmed());
            const int rightLevel = levelOrder(grade, fromUtf8(right.level).trimmed());
            if (leftLevel != rightLevel)
            {
                return leftLevel < rightLevel;
            }
            if (left.order != right.order)
            {
                return left.order < right.order;
            }
            return left.id < right.id;
        }
        );

    QStringList baseLabels;
    baseLabels.reserve(static_cast<qsizetype>(m_visibleRows.size()));
    QHash<QString, int> counts;
    for (const std::size_t index : m_visibleRows)
    {
        const QString label = baseClassLabel(summaries[index]);
        baseLabels.append(label);
        ++counts[label];
    }

    m_displayLabels.clear();
    m_displayLabels.reserve(baseLabels.size());
    QHash<QString, int> expandedCounts;
    for (std::size_t row = 0; row < m_visibleRows.size(); ++row)
    {
        const auto& summary = summaries[m_visibleRows[row]];
        QString label = baseLabels.at(static_cast<qsizetype>(row));
        if (counts.value(label) > 1)
        {
            const QString teacher = teacherDisplayName(summary).trimmed();
            if (!teacher.isEmpty())
            {
                label = QStringLiteral("%1 — %2").arg(label, teacher);
            }
            ++expandedCounts[label];
        }
        m_displayLabels.append(label);
    }

    for (std::size_t row = 0; row < m_visibleRows.size(); ++row)
    {
        const auto& summary = summaries[m_visibleRows[row]];
        QString& label = m_displayLabels[static_cast<qsizetype>(row)];
        if (expandedCounts.value(label) > 1)
        {
            label += QStringLiteral(" #%1").arg(fromUtf8(summary.id.value()));
        }
    }
}

QString SubPrepClassInformationListModel::teacherDisplayName(
    const ClassMngr::Next::Application::ClassSummary& summary
    ) const
{
    if (!summary.teacherId.has_value())
    {
        return {};
    }

    const auto teacher = m_projection.findTeacher(*summary.teacherId);
    return teacher.has_value() ? fromUtf8(teacher->displayName) : QString();
}
