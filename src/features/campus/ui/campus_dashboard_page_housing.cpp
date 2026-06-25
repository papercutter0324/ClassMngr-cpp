#include "campus_dashboard_page.h"

#include "ui/shared/widgets/text_fit_push_button.h"

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
        unregisterTextEdit(section.korean.complete);
        unregisterLineEdit(section.korean.buildingName);
        unregisterLineEdit(section.korean.province);
        unregisterLineEdit(section.korean.city);
        unregisterLineEdit(section.korean.district);
        unregisterLineEdit(section.korean.line1);
        unregisterLineEdit(section.korean.line2);
        unregisterLineEdit(section.korean.postalCode);
        unregisterLineEdit(section.note);

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
    section.name->hide();

    section.removeButton =
        new TextFitPushButton(
            tr("Remove"),
            container
            );

    sectionLayout->addWidget(
        section.removeButton,
        0,
        Qt::AlignRight
        );

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

    auto* noteForm =
        new QFormLayout;

    noteForm->setSpacing(8);
    noteForm->setContentsMargins(0, 0, 0, 0);
    noteForm->setFieldGrowthPolicy(
        QFormLayout::ExpandingFieldsGrow
        );

    section.note =
        addLineField(
            noteForm,
            QT_TR_NOOP("Note:")
            );

    sectionLayout->addLayout(noteForm);

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

    section.map =
        Detail::jsonObject(
            housing,
            QStringLiteral("map")
            );

    const QJsonObject englishAddress =
        Detail::jsonObject(
            housing,
            QStringLiteral("en")
            );

    const QJsonObject koreanAddress =
        Detail::jsonObject(
            housing,
            QStringLiteral("kr")
            );

    QString note =
        Detail::jsonString(
            housing,
            QStringLiteral("addr_note")
            );

    if (note.trimmed().isEmpty())
    {
        note = Detail::sharedAddressNote(
            englishAddress,
            Detail::jsonObject(
                englishAddress,
                QStringLiteral("modern")
                ),
            Detail::jsonObject(
                englishAddress,
                QStringLiteral("classic")
                )
            );
    }

    if (note.trimmed().isEmpty())
    {
        note = Detail::sharedAddressNote(
            koreanAddress,
            Detail::jsonObject(
                koreanAddress,
                QStringLiteral("modern")
                ),
            Detail::jsonObject(
                koreanAddress,
                QStringLiteral("classic")
                )
            );
    }

    section.note->setText(note);

    populateAddressSection(
        &section.english,
        englishAddress
        );

    populateAddressSection(
        &section.korean,
        koreanAddress
        );

    hideAddressComponents(
        &section.english,
        &section.korean
        );

    connect(
        section.name,
        &QLineEdit::textEdited,
        this,
        [this]()
        {
            updateMapPreview();
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
                unregisterTextEdit(section.korean.complete);
                unregisterLineEdit(section.korean.buildingName);
                unregisterLineEdit(section.korean.province);
                unregisterLineEdit(section.korean.city);
                unregisterLineEdit(section.korean.district);
                unregisterLineEdit(section.korean.line1);
                unregisterLineEdit(section.korean.line2);
                unregisterLineEdit(section.korean.postalCode);
                unregisterLineEdit(section.note);

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
            updateMapPreview();

            if (m_adminMode && !m_loading)
            {
                m_dirty = true;
                scheduleSave();
            }
        }
        );

    m_housingSections.append(section);

    applyAdminMode();
    updateHousingRemoveButtonVisibility();
    updateCompleteAddressPair(
        &m_housingSections.last().english,
        &m_housingSections.last().korean
        );
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

        housing.insert(
            QStringLiteral("addr_note"),
            section.note
                ? section.note->text()
                : QString()
            );

        housing.insert(
            QStringLiteral("map"),
            section.map
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
        updateCompleteAddressPair(
            &section.english,
            &section.korean
            );
    }
}
