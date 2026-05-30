#ifndef TEACHER_H
#define TEACHER_H

#include <QString>

struct Teacher
{
    int id;

    QString teacherKr;
    QString teacherEn;

    QString roomNumber;

    QString wifiName;
    QString wifiPassword;

    QString zoomId;
    QString zoomPassword;

    QString notes;
};

#endif // TEACHER_H
