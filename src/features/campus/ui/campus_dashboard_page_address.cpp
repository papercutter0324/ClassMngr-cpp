#include "campus_dashboard_page.h"

#include "campus_dashboard_page_detail.h"

#include <QFormLayout>
#include <QJsonObject>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QWidget>

#include <utility>

namespace Detail = CampusDashboardPageDetail;

void CampusDashboardPage::populateAddressSection(
    AddressSectionWidgets* section,
    const QJsonObject& address
    )
{
    if (!section)
    {
        return;
    }

    const QJsonObject modernAddress =
        address
            .value(QStringLiteral("modern"))
            .toObject();

    const QJsonObject classicAddress =
        address
            .value(QStringLiteral("classic"))
            .toObject();

    section->modernAddress =
        Detail::normalizedAddressForUi(
            modernAddress.isEmpty()
                ? address
                : modernAddress
            );

    section->classicAddress =
        Detail::normalizedAddressForUi(
            classicAddress.isEmpty()
                ? section->modernAddress
                : classicAddress
            );

    section->showingModernAddress =
        Detail::jsonString(
            address,
            QStringLiteral("address_system")
            ).compare(QStringLiteral("classic"), Qt::CaseInsensitive) != 0;

    loadAddressFields(
        section,
        section->showingModernAddress
            ? section->modernAddress
            : section->classicAddress
        );

    updateAddressSystemButton(section);
    updateCompleteAddressPairFor(section);
}

void CampusDashboardPage::loadAddressFields(
    AddressSectionWidgets* section,
    const QJsonObject& address
    ) const
{
    if (!section)
    {
        return;
    }

    if (section->buildingName)
    {
        section->buildingName->setText(
            Detail::jsonString(
                address,
                QStringLiteral("building_name")
                )
            );
    }

    section->province->setText(
        Detail::jsonString(
            address,
            QStringLiteral("province")
            )
        );

    section->city->setText(
        Detail::jsonString(
            address,
            QStringLiteral("city")
            )
        );

    section->district->setText(
        Detail::jsonString(
            address,
            QStringLiteral("district")
            )
        );

    section->line1->setText(
        Detail::jsonString(
            address,
            QStringLiteral("line1")
            )
        );

    section->line2->setText(
        Detail::jsonString(
            address,
            QStringLiteral("line2")
            )
        );

    section->postalCode->setText(
        Detail::jsonString(
            address,
            QStringLiteral("postal_code")
            )
        );
}

QJsonObject CampusDashboardPage::addressSectionToJson(
    const AddressSectionWidgets& section
    ) const
{
    QJsonObject modernAddress =
        section.modernAddress;

    QJsonObject classicAddress =
        section.classicAddress;

    if (section.showingModernAddress)
    {
        modernAddress =
            addressFieldsToJson(section);
    }
    else
    {
        classicAddress =
            addressFieldsToJson(section);
    }

    modernAddress.remove(
        QStringLiteral("addr_note")
        );

    classicAddress.remove(
        QStringLiteral("addr_note")
        );

    QJsonObject address =
        modernAddress;

    address.insert(
        QStringLiteral("modern"),
        modernAddress
        );

    address.insert(
        QStringLiteral("classic"),
        classicAddress
        );

    address.insert(
        QStringLiteral("address_system"),
        section.showingModernAddress
            ? QStringLiteral("modern")
            : QStringLiteral("classic")
        );

    return address;
}

QJsonObject CampusDashboardPage::addressFieldsToJson(
    const AddressSectionWidgets& section
    ) const
{
    QJsonObject address;

    if (section.buildingName)
    {
        address.insert(
            QStringLiteral("building_name"),
            section.buildingName->text()
            );
    }

    address.insert(
        QStringLiteral("province"),
        section.province
            ? section.province->text()
            : QString()
        );

    address.insert(
        QStringLiteral("city"),
        section.city
            ? section.city->text()
            : QString()
        );

    address.insert(
        QStringLiteral("district"),
        section.district
            ? section.district->text()
            : QString()
        );

    address.insert(
        QStringLiteral("city_district"),
        Detail::combinedCityDistrict(
            section.city
                ? section.city->text()
                : QString(),
            section.district
                ? section.district->text()
                : QString()
            )
        );

    address.insert(
        QStringLiteral("line1"),
        section.line1
            ? section.line1->text()
            : QString()
        );

    address.insert(
        QStringLiteral("line2"),
        section.line2
            ? section.line2->text()
            : QString()
        );

    address.insert(
        QStringLiteral("postal_code"),
        section.postalCode
            ? section.postalCode->text()
            : QString()
        );

    return address;
}

void CampusDashboardPage::handleAddressSystemToggle(
    QPushButton* button
    )
{
    if (!button)
    {
        return;
    }

    if (m_directionsEnglishAddress.toggleAddressSystemButton == button)
    {
        toggleAddressSystem(&m_directionsEnglishAddress);
        return;
    }

    if (m_directionsKoreanAddress.toggleAddressSystemButton == button)
    {
        toggleAddressSystem(&m_directionsKoreanAddress);
        return;
    }

    for (HousingSectionWidgets& section : m_housingSections)
    {
        if (section.english.toggleAddressSystemButton == button)
        {
            toggleAddressSystem(&section.english);
            return;
        }

        if (section.korean.toggleAddressSystemButton == button)
        {
            toggleAddressSystem(&section.korean);
            return;
        }
    }
}

void CampusDashboardPage::handleAddressComponentsToggle(
    QPushButton* button
    )
{
    if (!button)
    {
        return;
    }

    const auto togglePair =
        [this, button](
            AddressSectionWidgets* english,
            AddressSectionWidgets* korean
            )
    {
        if (!english || !korean)
        {
            return false;
        }

        AddressSectionWidgets* selected = nullptr;

        if (english->toggleAddressComponentsButton == button)
        {
            selected = english;
        }
        else if (korean->toggleAddressComponentsButton == button)
        {
            selected = korean;
        }

        if (!selected)
        {
            return false;
        }

        const bool visible =
            !selected->showingAddressComponents;

        setAddressComponentsVisible(english, visible);
        setAddressComponentsVisible(korean, visible);
        return true;
    };

    if (togglePair(
            &m_directionsEnglishAddress,
            &m_directionsKoreanAddress
            ))
    {
        return;
    }

    for (HousingSectionWidgets& section : m_housingSections)
    {
        if (togglePair(&section.english, &section.korean))
        {
            return;
        }
    }
}

void CampusDashboardPage::handleAddressLanguageToggle(
    QPushButton* button
    )
{
    if (!button)
    {
        return;
    }

    if (
        m_directionsEnglishAddress.toggleLanguageButton == button
        || m_directionsKoreanAddress.toggleLanguageButton == button
        )
    {
        showDirectionsLanguage(
            !m_directionsShowingEnglish
            );
        return;
    }

    for (HousingSectionWidgets& section : m_housingSections)
    {
        if (
            section.english.toggleLanguageButton != button
            && section.korean.toggleLanguageButton != button
            )
        {
            continue;
        }

        section.showingEnglish =
            !section.showingEnglish;

        section.english.container->setVisible(
            section.showingEnglish
            );
        section.korean.container->setVisible(
            !section.showingEnglish
            );

        section.english.toggleLanguageButton->setText(
            tr("Show Korean")
            );
        section.korean.toggleLanguageButton->setText(
            tr("Show English")
            );
        return;
    }
}

void CampusDashboardPage::setAddressComponentsVisible(
    AddressSectionWidgets* section,
    bool visible
    )
{
    if (!section || !section->form)
    {
        return;
    }

    section->showingAddressComponents = visible;

    for (QWidget* field : std::as_const(section->componentFields))
    {
        if (field)
        {
            section->form->setRowVisible(field, visible);
        }
    }

    updateAddressComponentsButton(section);
}

void CampusDashboardPage::hideAddressComponents(
    AddressSectionWidgets* english,
    AddressSectionWidgets* korean
    )
{
    setAddressComponentsVisible(
        english,
        false
        );
    setAddressComponentsVisible(
        korean,
        false
        );
}

void CampusDashboardPage::toggleAddressSystem(
    AddressSectionWidgets* section
    )
{
    if (!section)
    {
        return;
    }

    storeCurrentAddressVariant(section);

    section->showingModernAddress =
        !section->showingModernAddress;

    loadAddressFields(
        section,
        section->showingModernAddress
            ? section->modernAddress
            : section->classicAddress
        );

    updateAddressSystemButton(section);
    updateCompleteAddressPairFor(section);
}

void CampusDashboardPage::storeCurrentAddressVariant(
    AddressSectionWidgets* section
    ) const
{
    if (!section)
    {
        return;
    }

    if (section->showingModernAddress)
    {
        section->modernAddress =
            addressFieldsToJson(*section);
    }
    else
    {
        section->classicAddress =
            addressFieldsToJson(*section);
    }
}

void CampusDashboardPage::updateAddressSystemButton(
    AddressSectionWidgets* section
    ) const
{
    if (!section || !section->toggleAddressSystemButton)
    {
        return;
    }

    section->toggleAddressSystemButton->setText(
        section->showingModernAddress
            ? tr("Show Classic")
            : tr("Show Modern")
        );
}

void CampusDashboardPage::updateAddressComponentsButton(
    AddressSectionWidgets* section
    ) const
{
    if (!section || !section->toggleAddressComponentsButton)
    {
        return;
    }

    section->toggleAddressComponentsButton->setText(
        section->showingAddressComponents
            ? tr("Hide Details")
            : tr("Show Details")
        );
}

QString CampusDashboardPage::completeAddressFor(
    const AddressSectionWidgets& section
    ) const
{
    const QString province =
        section.province
            ? section.province->text().trimmed()
            : QString();

    const QString city =
        section.city
            ? section.city->text().trimmed()
            : QString();

    const QString district =
        section.district
            ? section.district->text().trimmed()
            : QString();

    const QString line1 =
        section.line1
            ? section.line1->text().trimmed()
            : QString();

    const QString line2 =
        section.line2
            ? section.line2->text().trimmed()
            : QString();

    const QString line2Suffix =
        section.line2Suffix
            ? section.line2Suffix->text().trimmed()
            : QString();

    QString renderedLine2 =
        line2;

    if (!line2Suffix.isEmpty())
    {
        renderedLine2 =
            renderedLine2.isEmpty()
                ? tr("(%1)").arg(line2Suffix)
                : tr("%1 (%2)").arg(renderedLine2, line2Suffix);
    }

    const QString postalCode =
        section.postalCode
            ? section.postalCode->text().trimmed()
            : QString();

    QStringList parts;

    if (section.koreanAddress)
    {
        const QString firstLine =
            QStringList{province, city, district, line1}
                .join(QStringLiteral(" "))
                .simplified();

        if (!firstLine.isEmpty())
        {
            parts.append(firstLine);
        }

        if (!renderedLine2.isEmpty())
        {
            parts.append(renderedLine2);
        }

        parts.append(QStringLiteral("[Recipient's Name]"));

        if (!postalCode.isEmpty())
        {
            parts.append(postalCode);
        }

        return parts.join(u'\n');
    }

    parts.append(QStringLiteral("[Recipient's Name]"));

    if (!renderedLine2.isEmpty())
    {
        parts.append(renderedLine2);
    }

    if (!line1.isEmpty())
    {
        parts.append(line1);
    }

    QStringList regionParts;

    if (!district.isEmpty())
    {
        regionParts.append(district);
    }

    if (!city.isEmpty())
    {
        regionParts.append(city);
    }

    if (!province.isEmpty())
    {
        regionParts.append(province);
    }

    const QString finalLine =
        regionParts.join(QStringLiteral(" "));

    if (!finalLine.isEmpty())
    {
        parts.append(finalLine);
    }

    if (!postalCode.isEmpty())
    {
        parts.append(postalCode);
    }

    return parts.join(u'\n');
}

void CampusDashboardPage::updateCompleteAddress(
    AddressSectionWidgets* section
    )
{
    if (!section || !section->complete)
    {
        return;
    }

    section->complete->setPlainText(
        completeAddressFor(*section)
        );

    section->complete->setFixedHeight(
        CompleteAddressMinimumHeight
        );
}

void CampusDashboardPage::updateCompleteAddressPair(
    AddressSectionWidgets* english,
    AddressSectionWidgets* korean
    )
{
    if (!english || !korean)
    {
        return;
    }

    if (!english->complete || !korean->complete)
    {
        return;
    }

    updateCompleteAddress(english);
    updateCompleteAddress(korean);
}

void CampusDashboardPage::updateCompleteAddressPairFor(
    AddressSectionWidgets* section
    )
{
    if (!section)
    {
        return;
    }

    if (
        section == &m_directionsEnglishAddress
        || section == &m_directionsKoreanAddress
        )
    {
        updateCompleteAddressPair(
            &m_directionsEnglishAddress,
            &m_directionsKoreanAddress
            );
        return;
    }

    for (HousingSectionWidgets& housing : m_housingSections)
    {
        if (
            section == &housing.english
            || section == &housing.korean
            )
        {
            updateCompleteAddressPair(
                &housing.english,
                &housing.korean
                );
            return;
        }
    }

    updateCompleteAddress(section);
}

void CampusDashboardPage::showDirectionsLanguage(
    bool showEnglish
    )
{
    m_directionsShowingEnglish =
        showEnglish;

    if (m_directionsEnglishAddress.container)
    {
        m_directionsEnglishAddress.container->setVisible(showEnglish);
    }

    if (m_directionsKoreanAddress.container)
    {
        m_directionsKoreanAddress.container->setVisible(!showEnglish);
    }

    if (m_directionsEnglishAddress.toggleLanguageButton)
    {
        m_directionsEnglishAddress.toggleLanguageButton->setText(
            tr("Show Korean")
            );
    }

    if (m_directionsKoreanAddress.toggleLanguageButton)
    {
        m_directionsKoreanAddress.toggleLanguageButton->setText(
            tr("Show English")
            );
    }
}

void CampusDashboardPage::syncPhoneFields(
    QLineEdit* source
    )
{
    if (!source || !m_phoneEdit || !m_phoneKrEdit)
    {
        return;
    }

    QLineEdit* target =
        source == m_phoneEdit
            ? m_phoneKrEdit
            : m_phoneEdit;

    const QSignalBlocker blocker(target);

    target->setText(source->text());
}

CampusDashboardPage::AddressSectionWidgets*
CampusDashboardPage::addressSectionForField(
    QLineEdit* edit
    )
{
    if (!edit)
    {
        return nullptr;
    }

    auto containsField =
        [edit](const AddressSectionWidgets& section)
    {
        return section.buildingName == edit
            || section.province == edit
            || section.city == edit
            || section.district == edit
            || section.line1 == edit
            || section.line2 == edit
            || section.postalCode == edit;
    };

    if (containsField(m_directionsEnglishAddress))
    {
        return &m_directionsEnglishAddress;
    }

    if (containsField(m_directionsKoreanAddress))
    {
        return &m_directionsKoreanAddress;
    }

    for (HousingSectionWidgets& housingSection : m_housingSections)
    {
        if (containsField(housingSection.english))
        {
            return &housingSection.english;
        }

        if (containsField(housingSection.korean))
        {
            return &housingSection.korean;
        }
    }

    return nullptr;
}

void CampusDashboardPage::handleAddressVariantFieldEdited(
    QLineEdit* edit,
    const QString& key
    )
{
    if (m_loading || !edit)
    {
        return;
    }

    AddressSectionWidgets* section =
        addressSectionForField(edit);

    if (!section)
    {
        return;
    }

    QJsonObject* targetAddress =
        section->showingModernAddress
            ? &section->classicAddress
            : &section->modernAddress;

    if (!Detail::jsonString(*targetAddress, key).trimmed().isEmpty())
    {
        return;
    }

    targetAddress->insert(
        key,
        edit->text()
        );

    if (
        key == QStringLiteral("city")
        || key == QStringLiteral("district")
        )
    {
        targetAddress->insert(
            QStringLiteral("city_district"),
            Detail::combinedCityDistrict(
                Detail::jsonString(*targetAddress, QStringLiteral("city")),
                Detail::jsonString(*targetAddress, QStringLiteral("district"))
                )
            );
    }
}

void CampusDashboardPage::updateDirectionsCompleteAddresses()
{
    updateCompleteAddressPair(
        &m_directionsEnglishAddress,
        &m_directionsKoreanAddress
        );
}
