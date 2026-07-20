#include "teacher_model.h"

TeacherModel::TeacherModel(QObject* parent)
    : QAbstractListModel(parent)
{
}

int TeacherModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid())
        return 0;
    return m_teachers.size();
}

QVariant TeacherModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid())
        return {};

    const auto& t = m_teachers[index.row()];

    switch (role)
    {
    case IdRole: return t.id;
    case KrRole: return t.teacherKr;
    case EnRole: return t.teacherEn;
    case PreferredRomanizationRole: return t.preferredRomanization;
    case RoomRole: return t.roomNumber;
    case BirthdayRole: return t.birthday;
    case PhoneNumberRole: return t.phoneNumber;
    case WifiNameRole: return t.wifiName;
    case WifiPasswordRole: return t.wifiPassword;
    case ZoomIdRole: return t.zoomId;
    case ZoomPasswordRole: return t.zoomPassword;
    case InternetTypeRole: return t.internetType;
    case ProjectionTypeRole: return t.projectionType;
    default: return {};
    }
}

QHash<int, QByteArray> TeacherModel::roleNames() const
{
    return {
        { IdRole, "id" },
        { KrRole, "kr" },
        { EnRole, "en" },
        { PreferredRomanizationRole, "preferredRomanization" },
        { RoomRole, "room" },
        { BirthdayRole, "birthday" },
        { PhoneNumberRole, "phoneNumber" },
        { WifiNameRole, "wifiName" },
        { WifiPasswordRole, "wifiPassword" },
        { ZoomIdRole, "zoomId" },
        { ZoomPasswordRole, "zoomPassword" },
        { InternetTypeRole, "internetType" },
        { ProjectionTypeRole, "projectionType" }
    };
}

void TeacherModel::setTeachers(const QVector<Teacher>& teachers)
{
    beginResetModel();
    m_teachers = teachers;
    endResetModel();
}

const Teacher& TeacherModel::teacherAt(int row) const
{
    static Teacher empty;
    if (row < 0 || row >= m_teachers.size())
        return empty;

    return m_teachers[row];
}
