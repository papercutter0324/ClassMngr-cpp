#include "next/ui/testing_classes_model.h"

#include "domain/models/testing_class.h"
#include "next/application/testing_classes_ordering_policy.h"

#include <QStringList>

#include <utility>

namespace ClassMngr::Next::Ui
{
namespace
{

[[nodiscard]] int gradeOrder(const QString& grade)
{
    const int index = testingClassGrades().indexOf(grade);
    return index >= 0 ? index : 1'000;
}

[[nodiscard]] int levelOrder(const QString& grade, const QString& level)
{
    const int index = testingClassLevelsForGrade(grade).indexOf(level);
    return index >= 0 ? index : 1'000;
}

[[nodiscard]] QString label(const QString& name,
    const QString& grade, const QString& level)
{
    return QStringLiteral("%1 — %2 — %3").arg(name, grade, level);
}

} // namespace

TestingClassesModel::TestingClassesModel(QObject* parent)
    : QAbstractListModel(parent)
{
}

int TestingClassesModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_rows.size());
}

QVariant TestingClassesModel::data(const QModelIndex& index, const int role) const
{
    if (!index.isValid() || index.row() < 0
        || index.row() >= static_cast<int>(m_rows.size()))
    {
        return {};
    }
    const Row& row = m_rows[static_cast<std::size_t>(index.row())];
    switch (role)
    {
    case Qt::DisplayRole:
        return label(row.name, row.grade, row.level);
    case ClassIdRole:
        return QString::fromStdString(row.classId.value());
    case ClassNameRole:
        return row.name;
    case GradeRole:
        return row.grade;
    case LevelRole:
        return row.level;
    case RoomRole:
        return row.room;
    default:
        return {};
    }
}

void TestingClassesModel::setSnapshot(
    const Application::ScheduleTestingClassChoicesSnapshot& snapshot
    )
{
    std::vector<Row> rows;
    rows.reserve(snapshot.choices.size());
    std::vector<Application::TestingClassesOrderingFact> facts;
    facts.reserve(snapshot.choices.size());
    for (const Application::ScheduleTestingClassChoice& choice : snapshot.choices)
    {
        const QString name = QString::fromStdU16String(choice.name);
        const QString grade = QString::fromStdU16String(choice.grade);
        const QString level = QString::fromStdU16String(choice.level);
        rows.push_back({
            .classId = choice.classId,
            .name = name,
            .grade = grade,
            .level = level,
            .room = QString::fromStdU16String(choice.room)
        });
        facts.push_back({
            .gradeRank = Application::TestingClassesOrderingRank{
                gradeOrder(grade)},
            .levelRank = Application::TestingClassesOrderingRank{
                levelOrder(grade, level)},
            .levelText = level.toStdU16String(),
            .className = name.toStdU16String()
        });
    }
    const auto order = Application::orderTestingClassesRows(
        facts,
        [](const std::u16string_view left, const std::u16string_view right) {
            return QString::localeAwareCompare(
                QString::fromStdU16String(std::u16string(left)),
                QString::fromStdU16String(std::u16string(right)));
        });
    std::vector<Row> sorted;
    sorted.reserve(rows.size());
    for (const auto rowIndex : order)
    {
        sorted.push_back(std::move(rows.at(
            static_cast<std::size_t>(rowIndex.rawValue()))));
    }

    beginResetModel();
    m_rows = std::move(sorted);
    endResetModel();
}

void TestingClassesModel::clear()
{
    beginResetModel();
    m_rows.clear();
    endResetModel();
}

int TestingClassesModel::classCount() const noexcept
{
    return static_cast<int>(m_rows.size());
}

std::optional<Domain::ClassId> TestingClassesModel::classIdAt(
    const QModelIndex& index
    ) const
{
    if (!index.isValid() || index.row() < 0
        || index.row() >= static_cast<int>(m_rows.size()))
    {
        return std::nullopt;
    }
    return m_rows[static_cast<std::size_t>(index.row())].classId;
}

QModelIndex TestingClassesModel::indexForClassId(
    const Domain::ClassId& classId
    ) const
{
    for (int row = 0; row < static_cast<int>(m_rows.size()); ++row)
    {
        if (m_rows[static_cast<std::size_t>(row)].classId == classId)
        {
            return index(row, 0);
        }
    }
    return {};
}

} // namespace ClassMngr::Next::Ui
