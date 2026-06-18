#include "campus_dashboard_page.h"

#include "campus_dashboard_page_detail.h"

#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>

namespace Detail = CampusDashboardPageDetail;

void CampusDashboardPage::populateHousingSections(
    const QJsonArray& housingLocations
    )
{
    clearHousingSections();

    if (housingLocations.isEmpty())
    {
        if (m_housingEmptyLabel)
        {
            m_housingEmptyLabel->setVisible(!m_adminMode);
        }

        if (m_adminMode)
        {
            addHousingSectionFromJson(Detail::emptyHousingLocation());
        }

        return;
    }

    if (m_housingEmptyLabel)
    {
        m_housingEmptyLabel->setVisible(false);
    }

    for (const QJsonValue& value : housingLocations)
    {
        if (value.isObject())
        {
            addHousingSectionFromJson(value.toObject());
        }
    }

    if (m_housingSections.isEmpty())
    {
        if (m_adminMode)
        {
            addHousingSectionFromJson(Detail::emptyHousingLocation());
        }
        else if (m_housingEmptyLabel)
        {
            m_housingEmptyLabel->setVisible(true);
        }
    }
}

void CampusDashboardPage::clearHousingSections()
{
    auto unregisterLineEdit =
        [this](QLineEdit* edit)
    {
        m_lineEdits.removeAll(edit);
        m_alwaysReadOnlyLineEdits.removeAll(edit);
    };

    auto unregisterTextEdit =
        [this](QPlainTextEdit* edit)
    {
        m_textEdits.removeAll(edit);
        m_alwaysReadOnlyTextEdits.removeAll(edit);
    };

    for (const HousingSectionWidgets& section : m_housingSections)
    {
        unregisterLineEdit(section.name);
        unregisterTextEdit(section.english.complete);
        unregisterLineEdit(section.english.buildingName);
        unregisterLineEdit(section.english.province);
        unregisterLineEdit(section.english.city);
        unregisterLineEdit(section.english.district);
        unregisterLineEdit(section.english.line1);
        unregisterLineEdit(section.english.line2);
        unregisterLineEdit(section.english.postalCode);
        unregisterLineEdit(section.english.note);
        unregisterTextEdit(section.korean.complete);
        unregisterLineEdit(section.korean.buildingName);
        unregisterLineEdit(section.korean.province);
        unregisterLineEdit(section.korean.city);
        unregisterLineEdit(section.korean.district);
        unregisterLineEdit(section.korean.line1);
        unregisterLineEdit(section.korean.line2);
        unregisterLineEdit(section.korean.postalCode);
        unregisterLineEdit(section.korean.note);

        if (section.container)
        {
            section.container->deleteLater();
        }
    }

    m_housingSections.clear();
}

void CampusDashboardPage::addHousingSectionFromJson(
    const QJsonObject& housing
    )
{
    if (!m_housingSectionsLayout)
    {
        return;
    }

    if (m_housingEmptyLabel)
    {
        m_housingEmptyLabel->setVisible(false);
    }

    HousingSectionWidgets section;

    auto* container =
        new QFrame(m_housingTab);

    container->setFrameShape(QFrame::StyledPanel);

    auto* sectionLayout =
        new QVBoxLayout(container);

    sectionLayout->setContentsMargins(
        12,
        12,
        12,
        12
        );

    sectionLayout->setSpacing(10);

    section.name =
        new QLineEdit(container);

    m_lineEdits.append(section.name);

    auto* nameRow =
        new QWidget(container);

    auto* nameLayout =
        new QHBoxLayout(nameRow);

    nameLayout->setContentsMargins(
        0,
        0,
        0,
        0
        );

    nameLayout->setSpacing(8);

    section.toggleLanguageButton =
        new QPushButton(
            tr("Show Korean"),
            nameRow
            );

    Detail::setStaticToggleButtonWidth(section.toggleLanguageButton);

    section.removeButton =
        new QPushButton(
            tr("Remove"),
            nameRow
            );

    nameLayout->addWidget(section.name, 1);
    nameLayout->addWidget(section.toggleLanguageButton);
    nameLayout->addWidget(section.removeButton);

    auto* nameForm =
        new QFormLayout;

    nameForm->setSpacing(8);
    nameForm->setFieldGrowthPolicy(
        QFormLayout::ExpandingFieldsGrow
        );

    addFormRow(
        nameForm,
        QT_TR_NOOP("Housing Name:"),
        nameRow
        );

    sectionLayout->addLayout(nameForm);

    section.english =
        createAddressSection(
            container,
            false,
            true
            );

    section.korean =
        createAddressSection(
            container,
            true,
            true
            );

    sectionLayout->addWidget(section.english.container);
    sectionLayout->addWidget(section.korean.container);

    section.korean.container->setVisible(false);

    section.container =
        container;

    const int insertIndex =
        qMax(
            0,
            m_housingSectionsLayout->count() - 1
            );

    m_housingSectionsLayout->insertWidget(
        insertIndex,
        container
        );

    section.name->setText(
        Detail::jsonString(
            housing,
            QStringLiteral("name")
            )
        );

    populateAddressSection(
        &section.english,
        Detail::jsonObject(
            housing,
            QStringLiteral("en")
            )
        );

    populateAddressSection(
        &section.korean,
        Detail::jsonObject(
            housing,
            QStringLiteral("kr")
            )
        );

    connect(
        section.name,
        &QLineEdit::textEdited,
        this,
        [this]()
        {
            handleFieldEdited();
        }
        );

    connect(
        section.removeButton,
        &QPushButton::clicked,
        this,
        [this, container]()
        {
            for (int i = 0; i < m_housingSections.size(); ++i)
            {
                if (m_housingSections.at(i).container != container)
                {
                    continue;
                }

                const HousingSectionWidgets section =
                    m_housingSections.takeAt(i);

                auto unregisterLineEdit =
                    [this](QLineEdit* edit)
                {
                    m_lineEdits.removeAll(edit);
                    m_alwaysReadOnlyLineEdits.removeAll(edit);
                };

                auto unregisterTextEdit =
                    [this](QPlainTextEdit* edit)
                {
                    m_textEdits.removeAll(edit);
                    m_alwaysReadOnlyTextEdits.removeAll(edit);
                };

                unregisterLineEdit(section.name);
                unregisterTextEdit(section.english.complete);
                unregisterLineEdit(section.english.buildingName);
                unregisterLineEdit(section.english.province);
                unregisterLineEdit(section.english.city);
                unregisterLineEdit(section.english.district);
                unregisterLineEdit(section.english.line1);
                unregisterLineEdit(section.english.line2);
                unregisterLineEdit(section.english.postalCode);
                unregisterLineEdit(section.english.note);
                unregisterTextEdit(section.korean.complete);
                unregisterLineEdit(section.korean.buildingName);
                unregisterLineEdit(section.korean.province);
                unregisterLineEdit(section.korean.city);
                unregisterLineEdit(section.korean.district);
                unregisterLineEdit(section.korean.line1);
                unregisterLineEdit(section.korean.line2);
                unregisterLineEdit(section.korean.postalCode);
                unregisterLineEdit(section.korean.note);

                if (section.container)
                {
                    section.container->deleteLater();
                }

                break;
            }

            if (m_housingSections.isEmpty() && m_adminMode)
            {
                addHousingSectionFromJson(Detail::emptyHousingLocation());
            }

            updateHousingCompleteAddresses();
            updateHousingRemoveButtonVisibility();

            if (m_adminMode && !m_loading)
            {
                m_dirty = true;
                scheduleSave();
            }
        }
        );

    m_housingSections.append(section);

    connect(
        m_housingSections.last().toggleLanguageButton,
        &QPushButton::clicked,
        this,
        [this, container]()
        {
            for (HousingSectionWidgets& section : m_housingSections)
            {
                if (section.container != container)
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

                section.toggleLanguageButton->setText(
                    section.showingEnglish
                        ? tr("Show Korean")
                        : tr("Show English")
                    );

                break;
            }
        }
        );

    applyAdminMode();
    updateHousingRemoveButtonVisibility();
    updateCompleteAddress(&m_housingSections.last().english);
    updateCompleteAddress(&m_housingSections.last().korean);
}

QJsonArray CampusDashboardPage::housingSectionsToJson() const
{
    QJsonArray housingLocations;

    for (const HousingSectionWidgets& section : m_housingSections)
    {
        QJsonObject housing;

        housing.insert(
            QStringLiteral("name"),
            section.name
                ? section.name->text()
                : QString()
            );

        housing.insert(
            QStringLiteral("en"),
            addressSectionToJson(section.english)
            );

        housing.insert(
            QStringLiteral("kr"),
            addressSectionToJson(section.korean)
            );

        housingLocations.append(housing);
    }

    return housingLocations;
}

void CampusDashboardPage::updateHousingRemoveButtonVisibility()
{
    for (int i = 0; i < m_housingSections.size(); ++i)
    {
        QPushButton* removeButton =
            m_housingSections.at(i).removeButton;

        if (!removeButton)
        {
            continue;
        }

        removeButton->setVisible(
            m_adminMode && i > 0
            );
    }
}

void CampusDashboardPage::updateHousingCompleteAddresses()
{
    for (HousingSectionWidgets& section : m_housingSections)
    {
        updateCompleteAddress(&section.english);
        updateCompleteAddress(&section.korean);
    }
}
