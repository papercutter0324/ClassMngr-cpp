#ifndef CAMPUS_INFO_H
#define CAMPUS_INFO_H

#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QStringList>

struct CampusInfo
{
    QString id;

    QString campusName;
    QString campusCode;
    QString buildingName;
    QString buildingNameKr;
    QString address;
    QString phoneNumber;
    QString officeNumber;
    QJsonObject directionsAddressEn;
    QJsonObject directionsAddressKr;
    QString directionsNote;

    QStringList transitSteps;
    QString arrivalInfo;
    QString imageMain;
    QStringList mapImagePaths;
    QString naverMapUrl;
    QString kakaoMapUrl;

    QString officeWifi;
    QString officeWifiPassword;
    QString printerName;
    QString printerSteps;
    QString printerDriverUrl;
    bool printerDriverUrlUnavailable = true;
    QString photocopierCode;

    QJsonArray housingLocations;
};

#endif // CAMPUS_INFO_H
