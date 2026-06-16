#ifndef CAMPUS_H
#define CAMPUS_H

#include <QString>

struct CampusRecord
{
    int id = -1;

    QString name;

    QString buildingName;
    QString address;
    QString phoneNumber;

    QString officeNumber;

    QString transitSteps;
    QString arrivalInfo;

    QString imagePath;

    QString officeWifi;
    QString officeWifiPassword;

    QString printerName;
    QString printerSteps;

    QString photocopierCode;

    QString housingLocations;
};

#endif // CAMPUS_H
