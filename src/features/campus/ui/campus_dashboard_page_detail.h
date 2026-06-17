#ifndef CAMPUS_DASHBOARD_PAGE_DETAIL_H
#define CAMPUS_DASHBOARD_PAGE_DETAIL_H

#include <QJsonObject>
#include <QString>
#include <QStringList>

class QFormLayout;
class QPushButton;
class QWidget;

namespace CampusDashboardPageDetail
{
inline constexpr int AutosaveDebounceMs = 800;
inline constexpr auto NotApplicableText = "N/A";

void setStaticToggleButtonWidth(
    QPushButton* button
    );

QString jsonString(
    const QJsonObject& object,
    const QString& key
    );

QStringList transitStepsFromText(
    const QString& text
    );

QJsonObject jsonObject(
    const QJsonObject& object,
    const QString& key
    );

QString combinedCityDistrict(
    const QString& city,
    const QString& district
    );

QJsonObject normalizedAddressForUi(
    const QJsonObject& address
    );

QString sharedAddressNote(
    const QJsonObject& address,
    const QJsonObject& modernAddress,
    const QJsonObject& classicAddress
    );

QJsonObject emptyHousingLocation();

QWidget* createScrollContainer(
    QWidget* parent,
    QFormLayout** form
    );
}

#endif // CAMPUS_DASHBOARD_PAGE_DETAIL_H
