#include "campus_dashboard_page_detail.h"

#include <QFontMetrics>
#include <QFormLayout>
#include <QFrame>
#include <QJsonValue>
#include <QPushButton>
#include <QScrollArea>
#include <QSizePolicy>
#include <QVBoxLayout>

namespace CampusDashboardPageDetail
{

void setStaticToggleButtonWidth(
    QPushButton* button
    )
{
    if (!button)
    {
        return;
    }

    const QFontMetrics metrics(button->font());

    const int width =
        qMax(
            qMax(
                metrics.horizontalAdvance(QStringLiteral("Show Modern")),
                metrics.horizontalAdvance(QStringLiteral("Show Classic"))
                ),
            qMax(
                metrics.horizontalAdvance(QStringLiteral("Show Korean")),
                metrics.horizontalAdvance(QStringLiteral("Show English"))
                )
            ) + 32;

    button->setMinimumWidth(width);
    button->setSizePolicy(
        QSizePolicy::Fixed,
        QSizePolicy::Fixed
        );
}

QString jsonString(
    const QJsonObject& object,
    const QString& key
    )
{
    return object.value(key).toString();
}

QStringList transitStepsFromText(
    const QString& text
    )
{
    QStringList steps;

    const QStringList lines =
        text.split(
            u'\n',
            Qt::SkipEmptyParts
            );

    for (const QString& line : lines)
    {
        const QString step =
            line.trimmed();

        if (!step.isEmpty())
        {
            steps.append(step);
        }
    }

    return steps;
}

QJsonObject jsonObject(
    const QJsonObject& object,
    const QString& key
    )
{
    return object.value(key).toObject();
}

QString combinedCityDistrict(
    const QString& city,
    const QString& district
    )
{
    return QStringList{
        city.trimmed(),
        district.trimmed()
        }
        .join(QStringLiteral(" "))
        .simplified();
}

void splitLegacyCityDistrict(
    const QString& cityDistrict,
    QString* city,
    QString* district
    )
{
    if (!city || !district)
    {
        return;
    }

    const QString trimmedCityDistrict =
        cityDistrict.trimmed();

    if (trimmedCityDistrict.isEmpty())
    {
        return;
    }

    const int splitIndex =
        trimmedCityDistrict.lastIndexOf(u' ');

    if (splitIndex > 0)
    {
        *city =
            trimmedCityDistrict.left(splitIndex).trimmed();
        *district =
            trimmedCityDistrict.mid(splitIndex + 1).trimmed();
    }
    else
    {
        *district =
            trimmedCityDistrict;
    }
}

QJsonObject normalizedAddressForUi(
    const QJsonObject& address
    )
{
    QString city =
        jsonString(
            address,
            QStringLiteral("city")
            );

    QString district =
        jsonString(
            address,
            QStringLiteral("district")
            );

    if (city.trimmed().isEmpty() && district.trimmed().isEmpty())
    {
        splitLegacyCityDistrict(
            jsonString(
                address,
                QStringLiteral("city_district")
                ),
            &city,
            &district
            );
    }

    QJsonObject normalized;

    normalized.insert(
        QStringLiteral("building_name"),
        jsonString(
            address,
            QStringLiteral("building_name")
            )
        );

    normalized.insert(
        QStringLiteral("province"),
        jsonString(
            address,
            QStringLiteral("province")
            )
        );

    normalized.insert(
        QStringLiteral("city"),
        city
        );

    normalized.insert(
        QStringLiteral("district"),
        district
        );

    normalized.insert(
        QStringLiteral("city_district"),
        combinedCityDistrict(city, district)
        );

    normalized.insert(
        QStringLiteral("line1"),
        jsonString(
            address,
            QStringLiteral("line1")
            )
        );

    normalized.insert(
        QStringLiteral("line2"),
        jsonString(
            address,
            QStringLiteral("line2")
            )
        );

    normalized.insert(
        QStringLiteral("postal_code"),
        jsonString(
            address,
            QStringLiteral("postal_code")
            )
        );

    normalized.insert(
        QStringLiteral("addr_note"),
        jsonString(
            address,
            QStringLiteral("addr_note")
            )
        );

    return normalized;
}

QString sharedAddressNote(
    const QJsonObject& address,
    const QJsonObject& modernAddress,
    const QJsonObject& classicAddress
    )
{
    const QString topLevelNote =
        jsonString(
            address,
            QStringLiteral("addr_note")
            );

    if (!topLevelNote.trimmed().isEmpty())
    {
        return topLevelNote;
    }

    const QString modernNote =
        jsonString(
            modernAddress,
            QStringLiteral("addr_note")
            );

    if (!modernNote.trimmed().isEmpty())
    {
        return modernNote;
    }

    return jsonString(
        classicAddress,
        QStringLiteral("addr_note")
        );
}

QJsonObject emptyHousingLocation()
{
    QJsonObject housing;

    housing.insert(
        QStringLiteral("name"),
        QString()
        );

    housing.insert(
        QStringLiteral("en"),
        QJsonObject()
        );

    housing.insert(
        QStringLiteral("kr"),
        QJsonObject()
        );

    return housing;
}

QWidget* createScrollContainer(
    QWidget* parent,
    QFormLayout** form
    )
{
    auto* tab =
        new QWidget(parent);

    auto* root =
        new QVBoxLayout(tab);

    auto* scroll =
        new QScrollArea(tab);

    scroll->setWidgetResizable(true);
    scroll->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    scroll->setFrameShape(QFrame::NoFrame);

    auto* container =
        new QFrame(scroll);

    container->setFrameShape(QFrame::StyledPanel);
    container->setFrameShadow(QFrame::Plain);

    auto* containerLayout =
        new QVBoxLayout(container);

    containerLayout->setContentsMargins(
        12,
        12,
        12,
        12
        );

    auto* formLayout =
        new QFormLayout;

    formLayout->setContentsMargins(
        0,
        0,
        0,
        0
        );

    formLayout->setSpacing(10);
    formLayout->setFieldGrowthPolicy(
        QFormLayout::ExpandingFieldsGrow
        );

    containerLayout->addLayout(formLayout);
    containerLayout->addStretch();
    containerLayout->setStretch(0, 0);
    containerLayout->setStretch(1, 1);
    containerLayout->setAlignment(formLayout, Qt::AlignTop);

    scroll->setWidget(container);
    root->addWidget(scroll);

    *form = formLayout;

    return tab;
}
} // namespace CampusDashboardPageDetail
