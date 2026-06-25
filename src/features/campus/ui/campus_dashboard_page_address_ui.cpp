#include "campus_dashboard_page.h"

#include "campus_dashboard_page_detail.h"
#include "core/fontmanager.h"
#include "ui/shared/widgets/text_fit_push_button.h"

#include <QFont>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLayoutItem>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSizePolicy>
#include <QVBoxLayout>

#include <algorithm>

namespace Detail = CampusDashboardPageDetail;

QWidget* CampusDashboardPage::createAddressTab()
{
    auto* tab =
        new QWidget(this);

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

    auto* layout =
        new QVBoxLayout(container);

    layout->setContentsMargins(
        12,
        12,
        12,
        12
        );

    layout->setSpacing(10);

    m_nameEdit =
        new QLineEdit(container);

    m_lineEdits.append(m_nameEdit);
    m_nameEdit->hide();

    m_directionsEnglishAddress =
        createAddressSection(
            container,
            false
            );

    m_buildingEdit =
        new QLineEdit(m_directionsEnglishAddress.container);

    m_phoneEdit =
        new QLineEdit(m_directionsEnglishAddress.container);

    m_lineEdits.append(m_buildingEdit);
    m_lineEdits.append(m_phoneEdit);

    insertFormRow(
        m_directionsEnglishAddress.form,
        0,
        QT_TR_NOOP("Building Name:"),
        m_buildingEdit
        );

    insertFormRow(
        m_directionsEnglishAddress.summaryForm,
        1,
        QT_TR_NOOP("Phone Number:"),
        m_phoneEdit
        );

    m_directionsEnglishAddress.line2Suffix =
        m_buildingEdit;
    m_directionsEnglishAddress.componentFields.append(
        m_buildingEdit
        );

    m_directionsKoreanAddress =
        createAddressSection(
            container,
            true
            );

    m_buildingKrEdit =
        new QLineEdit(m_directionsKoreanAddress.container);

    m_phoneKrEdit =
        new QLineEdit(m_directionsKoreanAddress.container);

    const QFont koreanFont =
        FontManager::getKoreanFont();

    m_buildingKrEdit->setFont(koreanFont);
    m_phoneKrEdit->setFont(koreanFont);

    m_lineEdits.append(m_buildingKrEdit);
    m_lineEdits.append(m_phoneKrEdit);

    insertFormRow(
        m_directionsKoreanAddress.form,
        0,
        QT_TR_NOOP("Building Name:"),
        m_buildingKrEdit
        );

    insertFormRow(
        m_directionsKoreanAddress.summaryForm,
        1,
        QT_TR_NOOP("Phone Number:"),
        m_phoneKrEdit
        );

    m_directionsKoreanAddress.line2Suffix =
        m_buildingKrEdit;
    m_directionsKoreanAddress.componentFields.append(
        m_buildingKrEdit
        );
    alignAddressDetailsWithCompleteField(
        &m_directionsEnglishAddress
        );
    alignAddressDetailsWithCompleteField(
        &m_directionsKoreanAddress
        );
    hideAddressComponents(
        &m_directionsEnglishAddress,
        &m_directionsKoreanAddress
        );

    layout->addWidget(m_directionsEnglishAddress.container);
    layout->addWidget(m_directionsKoreanAddress.container);
    layout->addStretch();

    scroll->setWidget(container);
    root->addWidget(scroll);

    connect(
        m_nameEdit,
        &QLineEdit::textEdited,
        this,
        [this]()
        {
            handleFieldEdited();
        }
        );

    connect(
        m_nameEdit,
        &QLineEdit::editingFinished,
        this,
        &CampusDashboardPage::normalizeCampusNameField
        );

    connect(
        m_buildingEdit,
        &QLineEdit::textEdited,
        this,
        [this]()
        {
            handleFieldEdited();
        }
        );

    connect(
        m_buildingKrEdit,
        &QLineEdit::textEdited,
        this,
        [this]()
        {
            handleFieldEdited();
        }
        );

    connect(
        m_phoneEdit,
        &QLineEdit::textEdited,
        this,
        [this]()
        {
            syncPhoneFields(m_phoneEdit);
            handleFieldEdited();
        }
        );

    connect(
        m_phoneKrEdit,
        &QLineEdit::textEdited,
        this,
        [this]()
        {
            syncPhoneFields(m_phoneKrEdit);
            handleFieldEdited();
        }
        );

    showDirectionsLanguage(true);

    return tab;
}

void CampusDashboardPage::alignAddressDetailsWithCompleteField(
    AddressSectionWidgets* section
    ) const
{
    if (
        !section
        || !section->form
        || !section->summaryForm
        )
    {
        return;
    }

    int summaryLabelColumnWidth = 0;

    for (int row = 0; row < section->summaryForm->rowCount(); ++row)
    {
        QLayoutItem* item =
            section->summaryForm->itemAt(
                row,
                QFormLayout::LabelRole
                );

        if (auto* label =
                item
                    ? qobject_cast<QLabel*>(item->widget())
                    : nullptr)
        {
            summaryLabelColumnWidth =
                std::max(
                    summaryLabelColumnWidth,
                    label->sizeHint().width()
                    );
        }
    }

    if (summaryLabelColumnWidth <= 0)
    {
        return;
    }

    int horizontalSpacing =
        section->summaryForm->horizontalSpacing();

    if (horizontalSpacing < 0)
    {
        horizontalSpacing =
            section->summaryForm->spacing();
    }

    const int leftMargin =
        summaryLabelColumnWidth
        + std::max(
            0,
            horizontalSpacing
            );

    section->form->setContentsMargins(
        leftMargin,
        0,
        0,
        0
        );
}

void CampusDashboardPage::alignAllAddressDetailsWithCompleteFields()
{
    alignAddressDetailsWithCompleteField(&m_directionsEnglishAddress);
    alignAddressDetailsWithCompleteField(&m_directionsKoreanAddress);

    for (HousingSectionWidgets& section : m_housingSections)
    {
        alignAddressDetailsWithCompleteField(&section.english);
        alignAddressDetailsWithCompleteField(&section.korean);
    }
}

CampusDashboardPage::AddressSectionWidgets
CampusDashboardPage::createAddressSection(
    QWidget* parent,
    bool koreanAddress,
    bool includeBuildingName
    )
{
    AddressSectionWidgets section;

    section.koreanAddress =
        koreanAddress;

    section.container =
        new QWidget(parent);

    auto* sectionLayout =
        new QVBoxLayout(section.container);

    sectionLayout->setContentsMargins(
        0,
        0,
        0,
        0
        );

    sectionLayout->setSpacing(8);

    auto* addressContainer =
        new QWidget(section.container);

    section.form =
        new QFormLayout(addressContainer);

    section.form->setContentsMargins(
        0,
        0,
        0,
        0
        );

    section.form->setSpacing(8);
    section.form->setFieldGrowthPolicy(
        QFormLayout::ExpandingFieldsGrow
        );

    auto* completeAddressContainer =
        new QWidget(section.container);

    completeAddressContainer->setSizePolicy(
        QSizePolicy::Expanding,
        QSizePolicy::Fixed
        );

    auto* completeForm =
        new QFormLayout(completeAddressContainer);
    section.summaryForm =
        completeForm;

    completeForm->setContentsMargins(0, 0, 0, 0);
    completeForm->setSpacing(8);
    completeForm->setFieldGrowthPolicy(
        QFormLayout::ExpandingFieldsGrow
        );

    section.complete =
        new QPlainTextEdit(completeAddressContainer);

    if (koreanAddress)
    {
        section.complete->setFont(
            FontManager::getKoreanFont()
            );
    }

    section.complete->setReadOnly(true);
    section.complete->setMinimumWidth(280);
    section.complete->setLineWrapMode(QPlainTextEdit::NoWrap);
    section.complete->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    section.complete->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    configureExpandingTextField(
        section.complete,
        1,
        8
        );
    section.complete->setFixedHeight(
        CompleteAddressMinimumHeight
        );

    section.toggleLanguageButton =
        new TextFitPushButton(
            koreanAddress
                ? tr("Show English")
                : tr("Show Korean"),
            completeAddressContainer
            );

    section.toggleAddressSystemButton =
        new TextFitPushButton(
            tr("Show Classic"),
            completeAddressContainer
            );

    section.toggleAddressComponentsButton =
        new TextFitPushButton(
            tr("Show Details"),
            completeAddressContainer
            );

    Detail::setStaticToggleButtonWidths(
        section.toggleLanguageButton,
        section.toggleAddressSystemButton,
        section.toggleAddressComponentsButton,
        {
            tr("Show English"),
            tr("Show Korean"),
            tr("Show Modern"),
            tr("Show Classic"),
            tr("Show Details"),
            tr("Hide Details")
        }
        );

    auto* completeRow =
        new QWidget(completeAddressContainer);

    completeRow->setSizePolicy(
        QSizePolicy::Expanding,
        QSizePolicy::Fixed
        );

    auto* completeLayout =
        new QHBoxLayout(completeRow);

    completeLayout->setContentsMargins(
        0,
        0,
        0,
        0
        );

    completeLayout->setSpacing(8);
    section.completeControls =
        new QWidget(completeRow);

    section.completeControls->setSizePolicy(
        QSizePolicy::Fixed,
        QSizePolicy::Expanding
        );

    auto* controlsLayout =
        new QVBoxLayout(section.completeControls);

    controlsLayout->setContentsMargins(0, 0, 0, 0);
    controlsLayout->setSpacing(8);
    controlsLayout->setAlignment(Qt::AlignTop);
    controlsLayout->addWidget(
        section.toggleLanguageButton,
        0,
        Qt::AlignTop | Qt::AlignRight
        );
    controlsLayout->addWidget(
        section.toggleAddressSystemButton,
        0,
        Qt::AlignTop | Qt::AlignRight
        );
    controlsLayout->addWidget(
        section.toggleAddressComponentsButton,
        0,
        Qt::AlignTop | Qt::AlignRight
        );

    completeLayout->addWidget(section.complete, 1);
    completeLayout->addWidget(
        section.completeControls,
        0,
        Qt::AlignTop
        );

    addFormRow(
        completeForm,
        QT_TR_NOOP("Complete Address:"),
        completeRow
        );

    sectionLayout->addWidget(completeAddressContainer);

    m_alwaysReadOnlyTextEdits.append(section.complete);

    connect(
        section.toggleLanguageButton,
        &QPushButton::clicked,
        this,
        [this, button = section.toggleLanguageButton]()
        {
            handleAddressLanguageToggle(button);
        }
        );

    connect(
        section.toggleAddressSystemButton,
        &QPushButton::clicked,
        this,
        [this, button = section.toggleAddressSystemButton]()
        {
            handleAddressSystemToggle(button);
        }
        );

    connect(
        section.toggleAddressComponentsButton,
        &QPushButton::clicked,
        this,
        [this, button = section.toggleAddressComponentsButton]()
        {
            handleAddressComponentsToggle(button);
        }
        );

    if (includeBuildingName)
    {
        section.buildingName =
            addLineField(
                section.form,
                QT_TR_NOOP("Building Name:")
                );

        section.line2Suffix =
            section.buildingName;
    }

    section.province =
        addLineField(
            section.form,
            QT_TR_NOOP("Province:")
            );

    section.city =
        addLineField(
            section.form,
            QT_TR_NOOP("City:")
            );

    section.district =
        addLineField(
            section.form,
            QT_TR_NOOP("District:")
            );

    section.line1 =
        addLineField(
            section.form,
            QT_TR_NOOP("Address Line 1:")
            );

    section.line2 =
        addLineField(
            section.form,
            QT_TR_NOOP("Address Line 2:")
            );

    section.postalCode =
        addLineField(
            section.form,
            QT_TR_NOOP("Postal Code:")
            );

    section.componentFields = {
        section.buildingName,
        section.province,
        section.city,
        section.district,
        section.line1,
        section.line2,
        section.postalCode
    };

    section.componentFields.removeAll(nullptr);

    alignAddressDetailsWithCompleteField(
        &section
        );

    sectionLayout->addWidget(addressContainer);

    auto connectMirroredField =
        [this](QLineEdit* edit, const QString& key)
    {
        if (!edit)
        {
            return;
        }

        connect(
            edit,
            &QLineEdit::textEdited,
            this,
            [this, edit, key](const QString&)
            {
                handleAddressVariantFieldEdited(
                    edit,
                    key
                    );
            }
            );
    };

    connectMirroredField(
        section.buildingName,
        QStringLiteral("building_name")
        );

    connectMirroredField(
        section.province,
        QStringLiteral("province")
        );

    connectMirroredField(
        section.city,
        QStringLiteral("city")
        );

    connectMirroredField(
        section.district,
        QStringLiteral("district")
        );

    connectMirroredField(
        section.line2,
        QStringLiteral("line2")
        );

    if (koreanAddress)
    {
        const QFont koreanFont =
            FontManager::getKoreanFont();

        for (QLineEdit* edit : {
                 section.buildingName,
                 section.province,
                 section.city,
                 section.district,
                 section.line1,
                 section.line2,
                 section.postalCode
             })
        {
            if (edit)
            {
                edit->setFont(koreanFont);
            }
        }
    }

    return section;
}
