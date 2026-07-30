#pragma once

#include <QAbstractListModel>
#include <QVector>
#include "domain/models/teacher.h"

class TeacherModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Roles {
        IdRole = Qt::UserRole + 1,
        KrRole,
        EnRole,
        PreferredRomanizationRole,
        PreferredNameRole,
        RoomRole,
        BirthdayRole,
        PhoneNumberRole,
        WifiNameRole,
        WifiPasswordRole,
        ZoomIdRole,
        ZoomPasswordRole,
        InternetTypeRole,
        ProjectionTypeRole
    };

    explicit TeacherModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setTeachers(const QVector<Teacher>& teachers);
    const Teacher& teacherAt(int row) const;

private:
    QVector<Teacher> m_teachers;
};
