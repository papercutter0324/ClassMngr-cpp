#ifndef CAMPUS_INFO_H
#define CAMPUS_INFO_H

#include <QJsonArray>
#include <QString>
#include <QStringList>

struct CampusInfo
{
    QString id;

    QString campusName;
    QString buildingName;
    QString address;
    QString phoneNumber;
    QString officeNumber;

    QStringList transitSteps;
    QString arrivalInfo;
    QString imageMain;

    QString officeWifi;
    QString officeWifiPassword;
    QString printerName;
    QString printerSteps;
    QString photocopierCode;

    QJsonArray housingLocations;
};

#endif // CAMPUS_INFO_H
