#include "next/ui/schedule_import_review_models.h"

#include <QComboBox>
#include <QCoreApplication>
#include <QVariantMap>

#include <algorithm>
#include <optional>
#include <utility>

namespace ClassMngr::Next::Ui
{
using namespace Application;

namespace
{

constexpr int ChoiceItemsRole = Qt::UserRole + 1;

QString reviewText(const char* source)
{
    return QCoreApplication::translate("ScheduleImportReview", source);
}

QString text(const std::u16string& value)
{
    return QString::fromStdU16String(value);
}

QString idText(const Domain::TeacherId& value)
{
    return QString::fromStdString(value.value());
}

QString idText(const Domain::ClassId& value)
{
    return QString::fromStdString(value.value());
}

QVariant choice(const QString& label, const QVariant& value)
{
    return QVariantMap{{QStringLiteral("label"), label},
                       {QStringLiteral("value"), value}};
}

QVariantList teacherActionChoices(
    const ScheduleImportReviewTeacherAction action,
    const bool hasMatches
    )
{
    using Action = ScheduleImportReviewTeacherAction;
    QVariantList result;
    if (action == Action::Unselected)
    {
        result.append(choice(reviewText("Choose an action…"),
                            static_cast<int>(Action::Unselected)));
    }
    if (hasMatches)
    {
        result.append(choice(reviewText("Reuse"), static_cast<int>(Action::Reuse)));
        result.append(choice(reviewText("Update room"),
                            static_cast<int>(Action::UpdateRoom)));
    }
    result.append(choice(reviewText("Create"), static_cast<int>(Action::Create)));
    result.append(choice(reviewText("Skip"), static_cast<int>(Action::Skip)));
    return result;
}

QVariantList classActionChoices(
    const ScheduleImportReviewClassAction action,
    const bool hasMatches
    )
{
    using Action = ScheduleImportReviewClassAction;
    QVariantList result;
    if (action == Action::Unselected)
    {
        result.append(choice(reviewText("Choose an action…"),
                            static_cast<int>(Action::Unselected)));
    }
    if (hasMatches)
    {
        result.append(choice(reviewText("Update existing"),
            static_cast<int>(Action::UpdateExisting)));
    }
    result.append(choice(reviewText("Create new"),
                        static_cast<int>(Action::CreateNew)));
    result.append(choice(reviewText("Skip"), static_cast<int>(Action::Skip)));
    return result;
}

QString teacherActionLabel(const ScheduleImportReviewTeacherAction action)
{
    using Action = ScheduleImportReviewTeacherAction;
    switch (action)
    {
    case Action::Unselected: return reviewText("Choose an action…");
    case Action::Reuse: return reviewText("Reuse");
    case Action::UpdateRoom: return reviewText("Update room");
    case Action::Create: return reviewText("Create");
    case Action::Skip: return reviewText("Skip");
    case Action::Invalid: return reviewText("Invalid");
    }
    return {};
}

QString classActionLabel(const ScheduleImportReviewClassAction action)
{
    using Action = ScheduleImportReviewClassAction;
    switch (action)
    {
    case Action::Unselected: return reviewText("Choose an action…");
    case Action::UpdateExisting: return reviewText("Update existing");
    case Action::CreateNew: return reviewText("Create new");
    case Action::Skip: return reviewText("Skip");
    case Action::Invalid: return reviewText("Invalid");
    }
    return {};
}

QString teacherNameForId(
    const ScheduleImportReviewMapping* mapping,
    const Domain::TeacherId& id
    )
{
    if (!mapping)
    {
        return idText(id);
    }
    const auto found = std::find_if(mapping->snapshot.teachers.cbegin(),
        mapping->snapshot.teachers.cend(), [&id](const auto& teacher) {
            return teacher.id == id;
        });
    if (found == mapping->snapshot.teachers.cend())
    {
        return idText(id);
    }
    return QStringLiteral("%1 (#%2)").arg(text(found->koreanName), idText(id));
}

QString classNameForId(
    const ScheduleImportReviewMapping* mapping,
    const Domain::ClassId& id
    )
{
    if (!mapping)
    {
        return idText(id);
    }
    const auto found = std::find_if(mapping->snapshot.classes.cbegin(),
        mapping->snapshot.classes.cend(), [&id](const auto& classroom) {
            return classroom.id == id;
        });
    if (found == mapping->snapshot.classes.cend())
    {
        return idText(id);
    }
    return QStringLiteral("%1 %2 · #%3")
        .arg(text(found->grade), text(found->level), idText(id));
}

QString teacherMatchesText(
    const ScheduleImportReviewMapping* mapping,
    const ScheduleImportMatchingTeacherProjection& projection
    )
{
    if (projection.matchingTeacherIds.empty())
    {
        return reviewText("No existing match");
    }
    QStringList names;
    for (const Domain::TeacherId& id : projection.matchingTeacherIds)
    {
        names.append(teacherNameForId(mapping, id));
    }
    return names.join(QStringLiteral("; "));
}

QString classMatchesText(
    const ScheduleImportReviewMapping* mapping,
    const ScheduleImportMatchingClassProjection& projection
    )
{
    if (projection.matchingClassIds.empty())
    {
        return reviewText("No existing match");
    }
    QStringList names;
    for (const Domain::ClassId& id : projection.matchingClassIds)
    {
        names.append(classNameForId(mapping, id));
    }
    return names.join(QStringLiteral("; "));
}

QVariantList teacherTargetChoices(
    const ScheduleImportReviewMapping* mapping,
    const ScheduleImportMatchingTeacherProjection& projection
    )
{
    QVariantList result;
    result.append(choice(reviewText("Choose a teacher…"), QString{}));
    for (const Domain::TeacherId& id : projection.matchingTeacherIds)
    {
        result.append(choice(teacherNameForId(mapping, id), idText(id)));
    }
    return result;
}

QVariantList classTargetChoices(
    const ScheduleImportReviewMapping* mapping,
    const ScheduleImportMatchingClassProjection& projection
    )
{
    QVariantList result;
    result.append(choice(reviewText("Choose a class…"), QString{}));
    for (const Domain::ClassId& id : projection.matchingClassIds)
    {
        result.append(choice(classNameForId(mapping, id), idText(id)));
    }
    return result;
}

std::optional<Domain::TeacherId> parseTeacherId(const QString& value)
{
    if (value.isEmpty())
    {
        return std::nullopt;
    }
    return Domain::TeacherId::fromString(value.toStdString());
}

std::optional<Domain::ClassId> parseClassId(const QString& value)
{
    if (value.isEmpty())
    {
        return std::nullopt;
    }
    return Domain::ClassId::fromString(value.toStdString());
}

QVariantList roomChoices(const ScheduleImportMatchingTeacherProjection& projection)
{
    QVariantList result;
    result.append(choice(reviewText("Choose a room…"), QString{}));
    for (const std::u16string& room : projection.importedRooms)
    {
        const QString roomText = text(room);
        result.append(choice(roomText, roomText));
    }
    return result;
}

QString candidateTimes(const ScheduleImportClassCandidate& candidate)
{
    QStringList lines;
    for (const ClassTime& time : candidate.times)
    {
        lines.append(QStringLiteral("%1 %2–%3")
            .arg(time.day, time.startTime, time.endTime));
    }
    return lines.join(QStringLiteral(", "));
}

} // namespace

ScheduleImportTeacherReviewModel::ScheduleImportTeacherReviewModel(
    QObject* parent
    )
    : QAbstractTableModel(parent)
{
}

void ScheduleImportTeacherReviewModel::retranslateHeaders()
{
    emit headerDataChanged(Qt::Horizontal, 0, ColumnCount - 1);
}

void ScheduleImportTeacherReviewModel::setMapping(
    ScheduleImportReviewMapping* mapping
    )
{
    beginResetModel();
    m_mapping = mapping;
    endResetModel();
}

int ScheduleImportTeacherReviewModel::rowCount(const QModelIndex& parent) const
{
    return !parent.isValid() && m_mapping
        ? static_cast<int>(m_mapping->matching.teachers.size()) : 0;
}

int ScheduleImportTeacherReviewModel::columnCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : ColumnCount;
}

QVariant ScheduleImportTeacherReviewModel::data(
    const QModelIndex& index,
    const int role
    ) const
{
    if (!m_mapping || !index.isValid() || index.row() < 0
        || index.row() >= rowCount())
    {
        return {};
    }
    const std::size_t row = static_cast<std::size_t>(index.row());
    const auto& projection = m_mapping->matching.teachers.at(row);
    const auto& decision = m_mapping->decisions.teachers.at(row);
    if (role == ChoiceItemsRole)
    {
        switch (index.column())
        {
        case ActionColumn:
            return teacherActionChoices(decision.action,
                !projection.matchingTeacherIds.empty());
        case TargetColumn:
            return teacherTargetChoices(m_mapping, projection);
        case RoomColumn:
            return roomChoices(projection);
        default:
            return {};
        }
    }
    if (role == Qt::EditRole)
    {
        switch (index.column())
        {
        case ActionColumn:
            return static_cast<int>(decision.action);
        case TargetColumn:
            return row < m_mapping->selectedTeacherIds.size()
                && m_mapping->selectedTeacherIds[row]
                ? idText(*m_mapping->selectedTeacherIds[row]) : QString{};
        case RoomColumn:
            return QString::fromStdString(decision.selectedRoom.value());
        default:
            return {};
        }
    }
    if (role != Qt::DisplayRole)
    {
        return {};
    }
    switch (index.column())
    {
    case TeacherColumn:
        return text(projection.teacherName);
    case MatchesColumn:
        return teacherMatchesText(m_mapping, projection);
    case ActionColumn:
        return teacherActionLabel(decision.action);
    case TargetColumn:
        return row < m_mapping->selectedTeacherIds.size()
            && m_mapping->selectedTeacherIds[row]
            ? teacherNameForId(m_mapping,
                *m_mapping->selectedTeacherIds[row]) : QString{};
    case RoomColumn:
        return QString::fromStdString(decision.selectedRoom.value());
    default:
        return {};
    }
}

QVariant ScheduleImportTeacherReviewModel::headerData(
    const int section,
    const Qt::Orientation orientation,
    const int role
    ) const
{
    if (role != Qt::DisplayRole || orientation != Qt::Horizontal)
    {
        return {};
    }
    switch (section)
    {
    case TeacherColumn: return reviewText("Teacher");
    case MatchesColumn: return reviewText("Existing matches");
    case ActionColumn: return reviewText("Action");
    case TargetColumn: return reviewText("Target teacher");
    case RoomColumn: return reviewText("Imported room");
    default: return {};
    }
}

Qt::ItemFlags ScheduleImportTeacherReviewModel::flags(
    const QModelIndex& index
    ) const
{
    if (!index.isValid())
    {
        return Qt::NoItemFlags;
    }
    Qt::ItemFlags result = QAbstractTableModel::flags(index);
    if (index.column() == ActionColumn)
    {
        result |= Qt::ItemIsEditable;
    }
    else if (m_mapping && index.row() >= 0
        && static_cast<std::size_t>(index.row()) < m_mapping->matching.teachers.size())
    {
        const auto& projection = m_mapping->matching.teachers.at(
            static_cast<std::size_t>(index.row()));
        if ((index.column() == TargetColumn
             && !projection.matchingTeacherIds.empty())
            || (index.column() == RoomColumn
                && !projection.importedRooms.empty()))
        {
            result |= Qt::ItemIsEditable;
        }
    }
    return result;
}

bool ScheduleImportTeacherReviewModel::setData(
    const QModelIndex& index,
    const QVariant& value,
    const int role
    )
{
    if (!m_mapping || role != Qt::EditRole || !index.isValid()
        || index.row() < 0 || index.row() >= rowCount())
    {
        return false;
    }
    const std::size_t row = static_cast<std::size_t>(index.row());
    auto& decision = m_mapping->decisions.teachers.at(row);
    if (index.column() == ActionColumn)
    {
        decision.action = static_cast<ScheduleImportReviewTeacherAction>(
            value.toInt());
        if (decision.action == ScheduleImportReviewTeacherAction::Create
            || decision.action == ScheduleImportReviewTeacherAction::Skip)
        {
            m_mapping->selectedTeacherIds[row].reset();
        }
    }
    else if (index.column() == TargetColumn)
    {
        if (row >= m_mapping->selectedTeacherIds.size())
        {
            return false;
        }
        m_mapping->selectedTeacherIds[row] = parseTeacherId(value.toString());
    }
    else if (index.column() == RoomColumn)
    {
        decision.selectedRoom = Application::ScheduleImportReviewRoomValue(
            value.toString().toStdString());
    }
    else
    {
        return false;
    }
    emit dataChanged(index, index, {Qt::DisplayRole, Qt::EditRole});
    if (index.column() == ActionColumn || index.column() == TargetColumn)
    {
        emit dataChanged(this->index(index.row(), TargetColumn),
                         this->index(index.row(), TargetColumn),
                         {Qt::DisplayRole, Qt::EditRole});
    }
    return true;
}

Application::ScheduleImportReviewDecisionRequest
ScheduleImportTeacherReviewModel::decisions() const
{
    if (!m_mapping)
    {
        return {};
    }
    Application::ScheduleImportReviewDecisionRequest result =
        m_mapping->decisions;
    return result;
}

std::vector<std::optional<Domain::TeacherId>>
ScheduleImportTeacherReviewModel::teacherIds() const
{
    return m_mapping ? m_mapping->selectedTeacherIds
                     : std::vector<std::optional<Domain::TeacherId>>{};
}

ScheduleImportClassReviewModel::ScheduleImportClassReviewModel(QObject* parent)
    : QAbstractTableModel(parent)
{
}

void ScheduleImportClassReviewModel::retranslateHeaders()
{
    emit headerDataChanged(Qt::Horizontal, 0, ColumnCount - 1);
}

void ScheduleImportClassReviewModel::setMapping(
    ScheduleImportReviewMapping* mapping
    )
{
    beginResetModel();
    m_mapping = mapping;
    endResetModel();
}

int ScheduleImportClassReviewModel::rowCount(const QModelIndex& parent) const
{
    return !parent.isValid() && m_mapping
        ? static_cast<int>(m_mapping->matching.classes.size()) : 0;
}

int ScheduleImportClassReviewModel::columnCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : ColumnCount;
}

QVariant ScheduleImportClassReviewModel::data(
    const QModelIndex& index,
    const int role
    ) const
{
    if (!m_mapping || !index.isValid() || index.row() < 0
        || index.row() >= rowCount())
    {
        return {};
    }
    const std::size_t row = static_cast<std::size_t>(index.row());
    const auto& projection = m_mapping->matching.classes.at(row);
    const auto& decision = m_mapping->decisions.classes.at(row);
    if (role == ChoiceItemsRole)
    {
        if (index.column() == ActionColumn)
        {
            return classActionChoices(decision.action,
                !projection.matchingClassIds.empty());
        }
        if (index.column() == TargetColumn)
        {
            return classTargetChoices(m_mapping, projection);
        }
        return {};
    }
    if (role == Qt::EditRole)
    {
        if (index.column() == ActionColumn)
        {
            return static_cast<int>(decision.action);
        }
        if (index.column() == TargetColumn)
        {
            return decision.targetClassId
                ? idText(*decision.targetClassId) : QString{};
        }
        return {};
    }
    if (role != Qt::DisplayRole)
    {
        return {};
    }
    switch (index.column())
    {
    case ClassColumn:
    {
        const auto& candidate = m_mapping->user.classes.at(
            static_cast<qsizetype>(projection.candidateIndex.rawValue()));
        return QStringLiteral("%1 · %2 %3")
            .arg(candidate.teacherKr, candidate.classGrade,
                 candidate.classLevel);
    }
    case MatchesColumn:
        return classMatchesText(m_mapping, projection);
    case ActionColumn:
        return classActionLabel(decision.action);
    case TargetColumn:
        return decision.targetClassId
            ? classNameForId(m_mapping, *decision.targetClassId) : QString{};
    default:
        return {};
    }
}

QVariant ScheduleImportClassReviewModel::headerData(
    const int section,
    const Qt::Orientation orientation,
    const int role
    ) const
{
    if (role != Qt::DisplayRole || orientation != Qt::Horizontal)
    {
        return {};
    }
    switch (section)
    {
    case ClassColumn: return reviewText("Imported class");
    case MatchesColumn: return reviewText("Existing matches");
    case ActionColumn: return reviewText("Action");
    case TargetColumn: return reviewText("Target class");
    default: return {};
    }
}

Qt::ItemFlags ScheduleImportClassReviewModel::flags(
    const QModelIndex& index
    ) const
{
    if (!index.isValid())
    {
        return Qt::NoItemFlags;
    }
    Qt::ItemFlags result = QAbstractTableModel::flags(index);
    if (index.column() == ActionColumn)
    {
        result |= Qt::ItemIsEditable;
    }
    else if (index.column() == TargetColumn && m_mapping
        && index.row() >= 0
        && static_cast<std::size_t>(index.row()) < m_mapping->matching.classes.size()
        && !m_mapping->matching.classes.at(
            static_cast<std::size_t>(index.row())).matchingClassIds.empty())
    {
        result |= Qt::ItemIsEditable;
    }
    return result;
}

bool ScheduleImportClassReviewModel::setData(
    const QModelIndex& index,
    const QVariant& value,
    const int role
    )
{
    if (!m_mapping || role != Qt::EditRole || !index.isValid()
        || index.row() < 0 || index.row() >= rowCount())
    {
        return false;
    }
    auto& decision = m_mapping->decisions.classes.at(
        static_cast<std::size_t>(index.row()));
    if (index.column() == ActionColumn)
    {
        decision.action = static_cast<ScheduleImportReviewClassAction>(
            value.toInt());
        if (decision.action == ScheduleImportReviewClassAction::CreateNew
            || decision.action == ScheduleImportReviewClassAction::Skip)
        {
            decision.targetClassId.reset();
        }
    }
    else if (index.column() == TargetColumn)
    {
        decision.targetClassId = parseClassId(value.toString());
    }
    else
    {
        return false;
    }
    emit dataChanged(index, index, {Qt::DisplayRole, Qt::EditRole});
    if (index.column() == ActionColumn || index.column() == TargetColumn)
    {
        emit dataChanged(this->index(index.row(), TargetColumn),
                         this->index(index.row(), TargetColumn),
                         {Qt::DisplayRole, Qt::EditRole});
    }
    return true;
}

Application::ScheduleImportReviewDecisionRequest
ScheduleImportClassReviewModel::decisions() const
{
    if (!m_mapping)
    {
        return {};
    }
    return m_mapping->decisions;
}

ScheduleImportCandidatePreviewModel::ScheduleImportCandidatePreviewModel(
    QObject* parent
    )
    : QAbstractTableModel(parent)
{
}

void ScheduleImportCandidatePreviewModel::setUser(
    const ScheduleImportUserBlock* user
    )
{
    beginResetModel();
    m_user = user;
    endResetModel();
}

int ScheduleImportCandidatePreviewModel::rowCount(const QModelIndex& parent) const
{
    return !parent.isValid() && m_user ? m_user->classes.size() : 0;
}

int ScheduleImportCandidatePreviewModel::columnCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : ColumnCount;
}

QVariant ScheduleImportCandidatePreviewModel::data(
    const QModelIndex& index,
    const int role
    ) const
{
    if (!m_user || !index.isValid() || role != Qt::DisplayRole
        || index.row() < 0 || index.row() >= rowCount())
    {
        return {};
    }
    const auto& candidate = m_user->classes.at(index.row());
    switch (index.column())
    {
    case TeacherColumn: return candidate.teacherKr;
    case ClassColumn:
        return QStringLiteral("%1 %2")
            .arg(candidate.classGrade, candidate.classLevel).simplified();
    case TimesColumn: return candidateTimes(candidate);
    default: return {};
    }
}

QVariant ScheduleImportCandidatePreviewModel::headerData(
    const int section,
    const Qt::Orientation orientation,
    const int role
    ) const
{
    if (role != Qt::DisplayRole || orientation != Qt::Horizontal)
    {
        return {};
    }
    switch (section)
    {
    case TeacherColumn: return reviewText("Teacher");
    case ClassColumn: return reviewText("Class");
    case TimesColumn: return reviewText("Meeting times");
    default: return {};
    }
}

QWidget* ScheduleImportChoiceDelegate::createEditor(
    QWidget* parent,
    const QStyleOptionViewItem&,
    const QModelIndex& index
    ) const
{
    const QVariant choicesValue = index.data(ChoiceItemsRole);
    if (!choicesValue.isValid())
    {
        return nullptr;
    }
    auto* const combo = new QComboBox(parent);
    combo->setObjectName(QStringLiteral("scheduleImportChoiceEditor"));
    combo->setSizeAdjustPolicy(QComboBox::AdjustToContentsOnFirstShow);
    const QVariantList choices = choicesValue.toList();
    for (const QVariant& value : choices)
    {
        const QVariantMap item = value.toMap();
        combo->addItem(item.value(QStringLiteral("label")).toString(),
                       item.value(QStringLiteral("value")));
    }
    return combo;
}

void ScheduleImportChoiceDelegate::setEditorData(
    QWidget* editor,
    const QModelIndex& index
    ) const
{
    auto* const combo = qobject_cast<QComboBox*>(editor);
    if (!combo)
    {
        return;
    }
    const QVariant current = index.data(Qt::EditRole);
    const int match = combo->findData(current);
    combo->setCurrentIndex(match >= 0 ? match : 0);
}

void ScheduleImportChoiceDelegate::setModelData(
    QWidget* editor,
    QAbstractItemModel* model,
    const QModelIndex& index
    ) const
{
    auto* const combo = qobject_cast<QComboBox*>(editor);
    if (combo && model)
    {
        static_cast<void>(model->setData(index, combo->currentData(), Qt::EditRole));
    }
}

} // namespace ClassMngr::Next::Ui
