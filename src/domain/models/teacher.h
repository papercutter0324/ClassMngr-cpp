#ifndef TEACHER_H
#define TEACHER_H

#include <QString>

struct Teacher
{
    int id = -1;

    QString teacherKr;
    QString teacherEn;
    QString preferredRomanization;

    QString roomNumber;
    QString birthday;
    QString phoneNumber;

    QString wifiName;
    QString wifiPassword;
    QString internetType = QStringLiteral("WiFi");

    QString zoomId;
    QString zoomPassword;
    QString projectionType = QStringLiteral("HDMI");

    QString notes;
};

#endif // TEACHER_H
