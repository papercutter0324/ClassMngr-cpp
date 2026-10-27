#include "next/ui/initial_page.h"

#include "features/my_info/data/signature_image_processor.h"
#include "features/my_info/data/typed_signature_renderer.h"
#include "next/application/initial_setup_personal_name_requirement.h"
#include "next/application/initial_setup_teacher_entry_skip_policy.h"
#include "next/application/initial_setup_teacher_identity_validation.h"

#include <QComboBox>
#include <QCoreApplication>
#include <QFormLayout>
#include <QFont>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPixmap>
#include <QPushButton>
#include <QRadioButton>
#include <QSignalBlocker>
#include <QScrollArea>
#include <QVBoxLayout>

#include <cstddef>
#include <array>
#include <string>
#include <utility>

namespace
{

QString initialPageText(const char* sourceText)
{
    return QCoreApplication::translate("InitialPage", sourceText);
}

QString personalDetailsText(const char* sourceText)
{
    return QCoreApplication::translate("PersonalDetailsPage", sourceText);
}

QString setupDetailsText(const char* sourceText)
{
    return QCoreApplication::translate(
        "InitialSetupPersonalDetailsPage",
        sourceText
        );
}

QString teacherEntryText(const char* sourceText)
{
    return QCoreApplication::translate(
        "InitialSetupTeacherEntryPage",
        sourceText
        );
}

QString teacherInfoText(const char* sourceText)
{
    return QCoreApplication::translate("TeacherInfoPage", sourceText);
}

QString qString(const std::string& value)
{
    return QString::fromUtf8(
        value.data(),
        static_cast<qsizetype>(value.size())
        );
}

std::string utf8(const QString& value)
{
    const QByteArray bytes = value.toUtf8();
    return std::string(
        bytes.constData(),
        static_cast<std::size_t>(bytes.size())
        );
}

std::u16string utf16(const QString& value)
{
    return value.toStdU16String();
}

std::string bytes(const QByteArray& value)
{
    return value.isEmpty()
        ? std::string{}
        : std::string(
              value.constData(),
              static_cast<std::size_t>(value.size())
              );
}

TypedSignatureFont rendererFont(
    ClassMngr::Next::Application::PersonalSignatureFontChoice choice
    )
{
    using Choice = ClassMngr::Next::Application::PersonalSignatureFontChoice;
    switch (choice)
    {
    case Choice::DancingScript:
        return TypedSignatureFont::DancingScript;
    case Choice::GreatVibes:
        return TypedSignatureFont::GreatVibes;
    case Choice::Caveat:
        return TypedSignatureFont::Caveat;
    case Choice::JustAnotherHand:
    default:
        return TypedSignatureFont::JustAnotherHand;
    }
}

ClassMngr::Next::Application::PersonalSignatureFontChoice applicationFont(
    const QVariant& value
    )
{
    using Choice = ClassMngr::Next::Application::PersonalSignatureFontChoice;
    switch (value.toInt())
    {
    case 1:
        return Choice::DancingScript;
    case 2:
        return Choice::GreatVibes;
    case 3:
        return Choice::Caveat;
    case 0:
    default:
        return Choice::JustAnotherHand;
    }
}

} // namespace

namespace ClassMngr::Next::Ui
{

InitialPage::InitialPage(
    const PageContext& context,
    InitialSetupPageActions actions,
    QWidget* parent
    )
    : Page(context, parent),
      m_actions(std::move(actions))
{
    setObjectName(QStringLiteral("classMngrNextInitialSetupPage"));

    auto* title = new QLabel(initialPageText("Initial Setup"), this);
    title->setObjectName(QStringLiteral("classMngrNextInitialSetupTitle"));
    QFont titleFont = title->font();
    titleFont.setPointSize(titleFont.pointSize() + 8);
    titleFont.setWeight(QFont::DemiBold);
    title->setFont(titleFont);

    auto* description = new QLabel(
        initialPageText(
            "Create a ClassMngr profile to begin setup. Your profile is saved as a .tps file."
            ),
        this
        );
    description->setObjectName(
        QStringLiteral("classMngrNextInitialSetupDescription")
        );
    description->setWordWrap(true);
    description->setAlignment(Qt::AlignHCenter);

    m_statusLabel = new QLabel(this);
    m_statusLabel->setObjectName(
        QStringLiteral("classMngrNextInitialSetupStatus")
        );
    m_statusLabel->setWordWrap(true);
    m_statusLabel->setText(initialPageText("Choose where to save your profile."));

    m_profileLabel = new QLabel(this);
    m_profileLabel->setObjectName(
        QStringLiteral("classMngrNextInitialSetupProfileLocation")
        );
    m_profileLabel->setWordWrap(true);
    m_profileLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);

    m_recoveryLabel = new QLabel(this);
    m_recoveryLabel->setObjectName(
        QStringLiteral("classMngrNextInitialSetupRecoveryLocation")
        );
    m_recoveryLabel->setWordWrap(true);
    m_recoveryLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);

    m_createButton = new QPushButton(initialPageText("Create Profile…"), this);
    m_createButton->setObjectName(
        QStringLiteral("classMngrNextInitialSetupCreateProfile")
        );
    m_resumeButton = new QPushButton(initialPageText("Resume Setup"), this);
    m_resumeButton->setObjectName(
        QStringLiteral("classMngrNextInitialSetupResume")
        );
    m_restoreButton = new QPushButton(this);
    m_restoreButton->setObjectName(
        QStringLiteral("classMngrNextInitialSetupRestore")
        );
    m_cancelButton = new QPushButton(initialPageText("Cancel Setup"), this);
    m_cancelButton->setObjectName(
        QStringLiteral("classMngrNextInitialSetupCancel")
        );

    auto* setupContent = new QWidget(this);
    auto* setupContentLayout = new QVBoxLayout(setupContent);
    setupContentLayout->setContentsMargins(0, 0, 0, 0);
    setupContentLayout->setSpacing(12);

    m_personalDetailsGroup = new QGroupBox(
        setupDetailsText("Add Your Information"),
        setupContent
        );
    m_personalDetailsGroup->setObjectName(
        QStringLiteral("classMngrNextInitialSetupPersonalDetails")
        );
    auto* detailsLayout = new QVBoxLayout(m_personalDetailsGroup);
    auto* nameForm = new QFormLayout;
    auto* nameLabel = new QLabel(
        personalDetailsText("My Name") + QStringLiteral(" *"),
        m_personalDetailsGroup
        );
    nameLabel->setObjectName(
        QStringLiteral("classMngrNextInitialSetupPersonalNameLabel")
        );
    m_personalNameEdit = new QLineEdit(m_personalDetailsGroup);
    m_personalNameEdit->setObjectName(
        QStringLiteral("classMngrNextInitialSetupPersonalName")
        );
    m_personalNameEdit->setClearButtonEnabled(true);
    nameForm->addRow(nameLabel, m_personalNameEdit);
    detailsLayout->addLayout(nameForm);

    auto* signatureHeading = new QLabel(
        personalDetailsText("Signature"),
        m_personalDetailsGroup
        );
    QFont signatureHeadingFont = signatureHeading->font();
    signatureHeadingFont.setWeight(QFont::DemiBold);
    signatureHeading->setFont(signatureHeadingFont);
    detailsLayout->addWidget(signatureHeading);

    auto* signatureModeRow = new QHBoxLayout;
    m_imageSignatureModeButton = new QRadioButton(
        personalDetailsText("Image"),
        m_personalDetailsGroup
        );
    m_imageSignatureModeButton->setObjectName(
        QStringLiteral("classMngrNextInitialSetupSignatureImageMode")
        );
    m_typedSignatureModeButton = new QRadioButton(
        personalDetailsText("Type"),
        m_personalDetailsGroup
        );
    m_typedSignatureModeButton->setObjectName(
        QStringLiteral("classMngrNextInitialSetupSignatureTypeMode")
        );
    m_imageSignatureModeButton->setChecked(true);
    signatureModeRow->addWidget(m_imageSignatureModeButton);
    signatureModeRow->addWidget(m_typedSignatureModeButton);
    signatureModeRow->addStretch(1);
    detailsLayout->addLayout(signatureModeRow);

    m_signaturePreviewLabel = new QLabel(m_personalDetailsGroup);
    m_signaturePreviewLabel->setObjectName(
        QStringLiteral("classMngrNextInitialSetupSignaturePreview")
        );
    m_signaturePreviewLabel->setAlignment(Qt::AlignCenter);
    m_signaturePreviewLabel->setWordWrap(true);
    m_signaturePreviewLabel->setFixedHeight(150);
    m_signaturePreviewLabel->setFrameShape(QFrame::StyledPanel);
    detailsLayout->addWidget(m_signaturePreviewLabel);

    auto* imageActions = new QHBoxLayout;
    imageActions->addStretch(1);
    m_chooseSignatureButton = new QPushButton(
        personalDetailsText("Add Signature Image..."),
        m_personalDetailsGroup
        );
    m_chooseSignatureButton->setObjectName(
        QStringLiteral("classMngrNextInitialSetupChooseSignature")
        );
    m_removeSignatureButton = new QPushButton(
        personalDetailsText("Remove"),
        m_personalDetailsGroup
        );
    m_removeSignatureButton->setObjectName(
        QStringLiteral("classMngrNextInitialSetupRemoveSignature")
        );
    imageActions->addWidget(m_chooseSignatureButton);
    imageActions->addWidget(m_removeSignatureButton);
    detailsLayout->addLayout(imageActions);

    m_typedSignatureControls = new QWidget(m_personalDetailsGroup);
    m_typedSignatureControls->setObjectName(
        QStringLiteral("classMngrNextInitialSetupTypedSignatureControls")
        );
    auto* typedLayout = new QFormLayout(m_typedSignatureControls);
    m_typedSignatureEdit = new QLineEdit(m_typedSignatureControls);
    m_typedSignatureEdit->setObjectName(
        QStringLiteral("classMngrNextInitialSetupTypedSignature")
        );
    m_typedSignatureEdit->setPlaceholderText(
        personalDetailsText("Type your signature")
        );
    typedLayout->addRow(
        personalDetailsText("Type your signature"),
        m_typedSignatureEdit
        );
    m_signatureFontCombo = new QComboBox(m_typedSignatureControls);
    m_signatureFontCombo->setObjectName(
        QStringLiteral("classMngrNextInitialSetupSignatureFont")
        );
    m_signatureFontCombo->addItem(
        TypedSignature::displayName(TypedSignatureFont::JustAnotherHand),
        static_cast<int>(Application::PersonalSignatureFontChoice::JustAnotherHand)
        );
    m_signatureFontCombo->addItem(
        TypedSignature::displayName(TypedSignatureFont::DancingScript),
        static_cast<int>(Application::PersonalSignatureFontChoice::DancingScript)
        );
    m_signatureFontCombo->addItem(
        TypedSignature::displayName(TypedSignatureFont::GreatVibes),
        static_cast<int>(Application::PersonalSignatureFontChoice::GreatVibes)
        );
    m_signatureFontCombo->addItem(
        TypedSignature::displayName(TypedSignatureFont::Caveat),
        static_cast<int>(Application::PersonalSignatureFontChoice::Caveat)
        );
    typedLayout->addRow(
        personalDetailsText("Choose a style"),
        m_signatureFontCombo
        );
    detailsLayout->addWidget(m_typedSignatureControls);

    m_personalDetailsStatusLabel = new QLabel(m_personalDetailsGroup);
    m_personalDetailsStatusLabel->setObjectName(
        QStringLiteral("classMngrNextInitialSetupPersonalDetailsStatus")
        );
    m_personalDetailsStatusLabel->setWordWrap(true);
    detailsLayout->addWidget(m_personalDetailsStatusLabel);

    auto* detailsActions = new QHBoxLayout;
    detailsActions->addStretch(1);
    m_retryPersonalDetailsButton = new QPushButton(
        setupDetailsText("Retry"),
        m_personalDetailsGroup
        );
    m_retryPersonalDetailsButton->setObjectName(
        QStringLiteral("classMngrNextInitialSetupPersonalDetailsRetry")
        );
    m_savePersonalDetailsButton = new QPushButton(
        setupDetailsText("Save Changes"),
        m_personalDetailsGroup
        );
    m_savePersonalDetailsButton->setObjectName(
        QStringLiteral("classMngrNextInitialSetupPersonalDetailsSave")
        );
    detailsActions->addWidget(m_retryPersonalDetailsButton);
    detailsActions->addWidget(m_savePersonalDetailsButton);
    detailsLayout->addLayout(detailsActions);

    setupContentLayout->addWidget(m_personalDetailsGroup);

    m_teacherEntryGroup = new QGroupBox(
        teacherInfoText("Teacher Information"),
        setupContent
        );
    m_teacherEntryGroup->setObjectName(
        QStringLiteral("classMngrNextInitialSetupTeacherEntry")
        );
    auto* teacherLayout = new QVBoxLayout(m_teacherEntryGroup);

    auto* requiredTeacherGroup = new QGroupBox(
        teacherEntryText("Required Information"),
        m_teacherEntryGroup
        );
    auto* requiredTeacherForm = new QFormLayout(requiredTeacherGroup);
    requiredTeacherForm->setFormAlignment(Qt::AlignLeft | Qt::AlignTop);
    requiredTeacherForm->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    m_teacherEnglishNameEdit = new QLineEdit(requiredTeacherGroup);
    m_teacherEnglishNameEdit->setObjectName(
        QStringLiteral("classMngrNextInitialSetupTeacherEnglishName")
        );
    requiredTeacherForm->addRow(
        teacherInfoText("English Name") + QStringLiteral(" *"),
        m_teacherEnglishNameEdit
        );

    m_teacherKoreanNameEdit = new QLineEdit(requiredTeacherGroup);
    m_teacherKoreanNameEdit->setObjectName(
        QStringLiteral("classMngrNextInitialSetupTeacherKoreanName")
        );
    requiredTeacherForm->addRow(
        teacherInfoText("Korean Name") + QStringLiteral(" *"),
        m_teacherKoreanNameEdit
        );

    m_teacherPreferredSpellingEdit = new QLineEdit(requiredTeacherGroup);
    m_teacherPreferredSpellingEdit->setObjectName(
        QStringLiteral("classMngrNextInitialSetupTeacherPreferredSpelling")
        );
    requiredTeacherForm->addRow(
        teacherInfoText("Preferred Spelling") + QStringLiteral(" *"),
        m_teacherPreferredSpellingEdit
        );

    m_teacherRoomEdit = new QLineEdit(requiredTeacherGroup);
    m_teacherRoomEdit->setObjectName(
        QStringLiteral("classMngrNextInitialSetupTeacherRoom")
        );
    requiredTeacherForm->addRow(
        teacherInfoText("Room Number"),
        m_teacherRoomEdit
        );
    teacherLayout->addWidget(requiredTeacherGroup);

    auto* teacherRequirement = new QLabel(
        teacherEntryText(
            "Enter values for at least 2 of the 3 starred identity fields."
            ),
        m_teacherEntryGroup
        );
    teacherRequirement->setObjectName(
        QStringLiteral("classMngrNextInitialSetupTeacherRequirement")
        );
    teacherRequirement->setWordWrap(true);
    teacherLayout->addWidget(teacherRequirement);

    auto* optionalTeacherGroup = new QGroupBox(
        teacherEntryText("Optional Details"),
        m_teacherEntryGroup
        );
    auto* optionalTeacherForm = new QFormLayout(optionalTeacherGroup);
    optionalTeacherForm->setFormAlignment(Qt::AlignLeft | Qt::AlignTop);
    optionalTeacherForm->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    m_teacherPreferredNameCombo = new QComboBox(optionalTeacherGroup);
    m_teacherPreferredNameCombo->setObjectName(
        QStringLiteral("classMngrNextInitialSetupTeacherPreferredName")
        );
    optionalTeacherForm->addRow(
        teacherInfoText("Preferred Name"),
        m_teacherPreferredNameCombo
        );

    m_teacherInternetTypeCombo = new QComboBox(optionalTeacherGroup);
    m_teacherInternetTypeCombo->setObjectName(
        QStringLiteral("classMngrNextInitialSetupTeacherInternetType")
        );
    m_teacherInternetTypeCombo->addItem(QStringLiteral("WiFi"), QStringLiteral("WiFi"));
    m_teacherInternetTypeCombo->addItem(QStringLiteral("LAN"), QStringLiteral("LAN"));
    m_teacherInternetTypeCombo->addItem(
        teacherInfoText("Both"), QStringLiteral("Both")
        );
    m_teacherInternetTypeCombo->addItem(
        teacherInfoText("N/A"), QStringLiteral("N/A")
        );
    optionalTeacherForm->addRow(
        teacherInfoText("Internet Type"),
        m_teacherInternetTypeCombo
        );

    m_teacherWifiNameEdit = new QLineEdit(optionalTeacherGroup);
    m_teacherWifiNameEdit->setObjectName(
        QStringLiteral("classMngrNextInitialSetupTeacherWifiName")
        );
    optionalTeacherForm->addRow(
        teacherInfoText("WiFi Name"),
        m_teacherWifiNameEdit
        );
    m_teacherWifiPasswordEdit = new QLineEdit(optionalTeacherGroup);
    m_teacherWifiPasswordEdit->setObjectName(
        QStringLiteral("classMngrNextInitialSetupTeacherWifiPassword")
        );
    m_teacherWifiPasswordEdit->setEchoMode(QLineEdit::Password);
    optionalTeacherForm->addRow(
        teacherInfoText("WiFi Password"),
        m_teacherWifiPasswordEdit
        );

    m_teacherProjectionTypeCombo = new QComboBox(optionalTeacherGroup);
    m_teacherProjectionTypeCombo->setObjectName(
        QStringLiteral("classMngrNextInitialSetupTeacherProjectionType")
        );
    m_teacherProjectionTypeCombo->addItem(
        QStringLiteral("HDMI"), QStringLiteral("HDMI")
        );
    m_teacherProjectionTypeCombo->addItem(
        QStringLiteral("Zoom"), QStringLiteral("Zoom")
        );
    m_teacherProjectionTypeCombo->addItem(
        teacherInfoText("Any"), QStringLiteral("Any")
        );
    m_teacherProjectionTypeCombo->addItem(
        teacherInfoText("N/A"), QStringLiteral("N/A")
        );
    optionalTeacherForm->addRow(
        teacherInfoText("Projection Type"),
        m_teacherProjectionTypeCombo
        );

    m_teacherZoomIdEdit = new QLineEdit(optionalTeacherGroup);
    m_teacherZoomIdEdit->setObjectName(
        QStringLiteral("classMngrNextInitialSetupTeacherZoomId")
        );
    optionalTeacherForm->addRow(
        teacherInfoText("Zoom ID"),
        m_teacherZoomIdEdit
        );
    m_teacherZoomPasswordEdit = new QLineEdit(optionalTeacherGroup);
    m_teacherZoomPasswordEdit->setObjectName(
        QStringLiteral("classMngrNextInitialSetupTeacherZoomPassword")
        );
    m_teacherZoomPasswordEdit->setEchoMode(QLineEdit::Password);
    optionalTeacherForm->addRow(
        teacherInfoText("Zoom Password"),
        m_teacherZoomPasswordEdit
        );
    teacherLayout->addWidget(optionalTeacherGroup);

    m_teacherEntryStatusLabel = new QLabel(m_teacherEntryGroup);
    m_teacherEntryStatusLabel->setObjectName(
        QStringLiteral("classMngrNextInitialSetupTeacherEntryStatus")
        );
    m_teacherEntryStatusLabel->setWordWrap(true);
    teacherLayout->addWidget(m_teacherEntryStatusLabel);

    auto* teacherActions = new QHBoxLayout;
    teacherActions->addStretch(1);
    m_addAnotherTeacherButton = new QPushButton(
        teacherEntryText("Add Another Teacher"),
        m_teacherEntryGroup
        );
    m_addAnotherTeacherButton->setObjectName(
        QStringLiteral("classMngrNextInitialSetupAddAnotherTeacher")
        );
    m_continueTeacherEntryButton = new QPushButton(
        teacherEntryText("Continue"),
        m_teacherEntryGroup
        );
    m_continueTeacherEntryButton->setObjectName(
        QStringLiteral("classMngrNextInitialSetupContinueTeacherEntry")
        );
    m_importTeachersButton = new QPushButton(
        teacherEntryText("Import Teachers from Workbook"),
        m_teacherEntryGroup
        );
    m_importTeachersButton->setObjectName(
        QStringLiteral("classMngrNextInitialSetupImportTeachers")
        );
    teacherActions->insertWidget(0, m_importTeachersButton);
    teacherActions->addWidget(m_addAnotherTeacherButton);
    teacherActions->addWidget(m_continueTeacherEntryButton);
    teacherLayout->addLayout(teacherActions);
    setupContentLayout->addWidget(m_teacherEntryGroup);
    setupContentLayout->addStretch(1);

    auto* setupScroll = new QScrollArea(this);
    setupScroll->setObjectName(
        QStringLiteral("classMngrNextInitialSetupDetailsScroll")
        );
    setupScroll->setWidgetResizable(true);
    setupScroll->setFrameShape(QFrame::NoFrame);
    setupScroll->setWidget(setupContent);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(48, 32, 48, 32);
    layout->setSpacing(10);
    layout->addStretch(1);
    layout->addWidget(title, 0, Qt::AlignHCenter);
    layout->addWidget(description);
    layout->addSpacing(4);
    layout->addWidget(m_statusLabel);
    layout->addWidget(m_profileLabel);
    layout->addWidget(m_recoveryLabel);
    layout->addWidget(setupScroll, 1);
    layout->addWidget(m_createButton, 0, Qt::AlignHCenter);
    layout->addWidget(m_resumeButton, 0, Qt::AlignHCenter);
    layout->addWidget(m_restoreButton, 0, Qt::AlignHCenter);
    layout->addWidget(m_cancelButton, 0, Qt::AlignHCenter);
    layout->addStretch(1);

    connect(m_createButton, &QPushButton::clicked, this, [this] {
        if (m_actions.createProfile)
        {
            m_actions.createProfile();
        }
        refreshSetupState();
    });
    connect(m_resumeButton, &QPushButton::clicked, this, [this] {
        if (m_actions.resumeSetup)
        {
            m_actions.resumeSetup();
        }
        refreshSetupState();
    });
    connect(m_restoreButton, &QPushButton::clicked, this, [this] {
        if (m_actions.restoreSetup)
        {
            m_actions.restoreSetup();
        }
        refreshSetupState();
    });
    connect(m_cancelButton, &QPushButton::clicked, this, [this] {
        if (m_actions.cancelSetup)
        {
            m_actions.cancelSetup();
        }
        refreshSetupState();
    });
    connect(m_retryPersonalDetailsButton, &QPushButton::clicked,
            this, &InitialPage::refreshPersonalDetails);
    connect(m_savePersonalDetailsButton, &QPushButton::clicked,
            this, &InitialPage::savePersonalDetails);
    connect(m_addAnotherTeacherButton, &QPushButton::clicked,
            this, [this] { saveTeacher(true); });
    connect(m_continueTeacherEntryButton, &QPushButton::clicked,
            this, [this] { saveTeacher(false); });
    connect(m_importTeachersButton, &QPushButton::clicked,
            this, &InitialPage::importTeachersFromWorkbook);
    connect(m_teacherEnglishNameEdit, &QLineEdit::textChanged,
            this, &InitialPage::updatePreferredTeacherNames);
    connect(m_teacherPreferredSpellingEdit, &QLineEdit::textChanged,
            this, &InitialPage::updatePreferredTeacherNames);
    connect(m_chooseSignatureButton, &QPushButton::clicked, this, [this] {
        if (!m_actions.chooseSignatureImage)
        {
            return;
        }

        const auto selected = m_actions.chooseSignatureImage();
        if (!selected)
        {
            setPersonalDetailsStatus(
                setupDetailsText("The signature image could not be selected: %1")
                    .arg(qString(selected.error().message))
                );
            return;
        }
        if (!selected.value().has_value())
        {
            return;
        }

        const QByteArray prepared = SignatureImage::prepareForEmbedding(
            *selected.value()
            );
        if (prepared.isEmpty())
        {
            setPersonalDetailsStatus(
                personalDetailsText(
                    "The selected file is not a supported image."
                    )
                );
            return;
        }

        m_imageSignature = prepared;
        m_personalDetailsEdited = true;
        updatePersonalDetailsControls();
        updateSignaturePreview();
    });
    connect(m_removeSignatureButton, &QPushButton::clicked, this, [this] {
        m_imageSignature.clear();
        m_personalDetailsEdited = true;
        updatePersonalDetailsControls();
        updateSignaturePreview();
    });
    connect(m_imageSignatureModeButton, &QRadioButton::clicked, this, [this] {
        m_personalDetailsEdited = true;
        updatePersonalDetailsControls();
        updateSignaturePreview();
    });
    connect(m_typedSignatureModeButton, &QRadioButton::clicked, this, [this] {
        m_personalDetailsEdited = true;
        updatePersonalDetailsControls();
        updateSignaturePreview();
    });
    connect(m_personalNameEdit, &QLineEdit::textChanged, this, [this] {
        if (!m_updatingPersonalDetails)
        {
            m_personalDetailsEdited = true;
        }
    });
    connect(m_typedSignatureEdit, &QLineEdit::textChanged, this, [this] {
        if (!m_updatingPersonalDetails)
        {
            m_personalDetailsEdited = true;
        }
        updateSignaturePreview();
    });
    connect(m_signatureFontCombo,
            qOverload<int>(&QComboBox::currentIndexChanged),
            this,
            [this] {
                if (!m_updatingPersonalDetails)
                {
                    m_personalDetailsEdited = true;
                }
                updateSignaturePreview();
            });

    m_personalDetailsGroup->hide();
    m_teacherEntryGroup->hide();
    refreshSetupState();
}

void InitialPage::refreshSetupState()
{
    const InitialSetupPageState state = m_actions.state
        ? m_actions.state()
        : InitialSetupPageState{};

    if (!state.statusMessage.isEmpty())
    {
        m_statusLabel->setText(state.statusMessage);
    }
    else if (state.setupPending)
    {
        m_statusLabel->setText(initialPageText("Profile created. Setup is pending."));
    }
    else if (state.recoveryAvailable)
    {
        m_statusLabel->setText(initialPageText("An interrupted setup needs your choice."));
    }
    else if (state.recoveryError)
    {
        m_statusLabel->setText(initialPageText("The setup recovery record could not be read."));
    }
    else
    {
        m_statusLabel->setText(initialPageText("Choose where to save your profile."));
    }

    if (state.setupPending && !state.profileLocation.isEmpty())
    {
        m_profileLabel->setText(
            initialPageText("Profile: %1").arg(state.profileLocation)
            );
        m_profileLabel->show();
    }
    else if (state.recoveryAvailable
             && !state.recoveryProfileLocation.isEmpty())
    {
        m_profileLabel->setText(
            initialPageText("Profile to recover: %1").arg(state.recoveryProfileLocation)
            );
        m_profileLabel->show();
    }
    else
    {
        m_profileLabel->clear();
        m_profileLabel->hide();
    }

    if ((state.recoveryAvailable || state.setupPending)
        && state.recoveryHasOriginalProfile)
    {
        m_recoveryLabel->setText(
            initialPageText("The previous profile is preserved until setup is complete.")
            );
        m_recoveryLabel->show();
        m_restoreButton->setText(initialPageText("Restore Previous Profile"));
    }
    else if (state.recoveryAvailable)
    {
        m_recoveryLabel->setText(
            initialPageText(
                "No previous profile was replaced. You can resume or discard this profile."
                )
            );
        m_recoveryLabel->show();
        m_restoreButton->setText(initialPageText("Discard Incomplete Profile"));
    }
    else
    {
        m_recoveryLabel->clear();
        m_recoveryLabel->hide();
    }

    m_createButton->setVisible(
        !state.setupPending && !state.recoveryAvailable && !state.recoveryError
        );
    m_resumeButton->setVisible(
        !state.setupPending && state.recoveryAvailable
        );
    m_restoreButton->setVisible(
        !state.setupPending && state.recoveryAvailable
        );
    m_cancelButton->setVisible(state.setupPending);
    m_personalDetailsGroup->setVisible(state.setupPending);
    m_teacherEntryGroup->setVisible(state.setupPending);

    if (state.setupPending && !m_setupWasPending)
    {
        m_teacherEntryComplete = false;
        clearTeacherFields();
        m_teacherEntryStatusLabel->clear();
        updateTeacherEntryControls();
        m_personalDetailsLoaded = false;
        m_personalDetailsEdited = false;
        m_personalDetails = Application::PersonalDetailsSaveRequest{};
        m_imageSignature.clear();
        loadPersonalDetails();
    }
    else if (!state.setupPending && m_setupWasPending)
    {
        m_teacherEntryComplete = false;
        clearTeacherFields();
        m_teacherEntryStatusLabel->clear();
        updateTeacherEntryControls();
        m_personalDetailsLoaded = false;
        m_personalDetailsEdited = false;
        m_personalDetails = Application::PersonalDetailsSaveRequest{};
        m_imageSignature.clear();
        m_updatingPersonalDetails = true;
        m_personalNameEdit->clear();
        m_typedSignatureEdit->clear();
        m_imageSignatureModeButton->setChecked(true);
        m_signatureFontCombo->setCurrentIndex(0);
        m_updatingPersonalDetails = false;
        setPersonalDetailsStatus(QString());
        updatePersonalDetailsControls();
        updateSignaturePreview();
    }

    m_setupWasPending = state.setupPending;
}

void InitialPage::saveTeacher(const bool addAnother)
{
    if (!m_setupWasPending || m_teacherEntryComplete)
    {
        return;
    }

    const QString englishName = m_teacherEnglishNameEdit->text().trimmed();
    const QString koreanName = m_teacherKoreanNameEdit->text().trimmed();
    const QString preferredSpelling =
        m_teacherPreferredSpellingEdit->text().trimmed();
    const QString room = m_teacherRoomEdit->text().trimmed();
    const bool allRequiredAndRoomFieldsEmpty =
        englishName.isEmpty()
        && koreanName.isEmpty()
        && preferredSpelling.isEmpty()
        && room.isEmpty();
    const bool allTeacherFieldsEmpty =
        allRequiredAndRoomFieldsEmpty
        && m_teacherPreferredNameCombo->currentIndex() < 0
        && m_teacherInternetTypeCombo->currentIndex() == 0
        && m_teacherWifiNameEdit->text().trimmed().isEmpty()
        && m_teacherWifiPasswordEdit->text().isEmpty()
        && m_teacherProjectionTypeCombo->currentIndex() == 0
        && m_teacherZoomIdEdit->text().trimmed().isEmpty()
        && m_teacherZoomPasswordEdit->text().isEmpty();

    if (allTeacherFieldsEmpty && !addAnother)
    {
        const Application::InitialSetupTeacherChoicesResult choices =
            m_actions.loadTeacherChoices
                ? m_actions.loadTeacherChoices()
                : Application::InitialSetupTeacherChoicesResult::failure({
                      .code = Domain::ErrorCode::Technical,
                      .message = "Teachers could not be loaded.",
                      .recoverable = true
                  });
        if (!choices)
        {
            QString message = teacherEntryText(
                "Teacher choices could not be loaded. Retry to continue."
                );
            if (!choices.error().message.empty())
            {
                message += QLatin1Char('\n') + qString(choices.error().message);
            }
            m_teacherEntryStatusLabel->setText(message);
            return;
        }
        const bool availableTeacherExists = choices
            && !choices.value().teachers.empty();
        if (Application::initialSetupTeacherEntryShouldBeSkipped(
                allTeacherFieldsEmpty,
                availableTeacherExists
                ))
        {
            m_teacherEntryComplete = true;
            m_teacherEntryStatusLabel->setText(teacherEntryText(
                "Continuing with the teachers already in this profile. Setup remains pending."
                ));
            updateTeacherEntryControls();
            return;
        }
    }

    const Application::InitialSetupTeacherIdentityRequirements requirements =
        Application::initialSetupTeacherIdentityRequirements(
            !englishName.isEmpty(),
            !koreanName.isEmpty(),
            !preferredSpelling.isEmpty()
            );
    if (!requirements.complete())
    {
        QStringList missingFields;
        if (requirements.englishNameMissing())
        {
            missingFields.append(teacherInfoText("English Name"));
        }
        if (requirements.koreanNameMissing())
        {
            missingFields.append(teacherInfoText("Korean Name"));
        }
        if (requirements.preferredSpellingMissing())
        {
            missingFields.append(teacherInfoText("Preferred Spelling"));
        }
        m_teacherEntryStatusLabel->setText(
            teacherEntryText(
                "Complete at least %1 of these fields before continuing:\n\n- %2"
                )
                .arg(static_cast<int>(requirements.additionalFieldsRequired()))
                .arg(missingFields.join(QStringLiteral("\n- ")))
            );
        return;
    }

    QString preferredName = m_teacherPreferredNameCombo->currentText().trimmed();
    if (preferredName.isEmpty())
    {
        preferredName = !englishName.isEmpty()
            ? englishName
            : (!preferredSpelling.isEmpty() ? preferredSpelling : koreanName);
    }

    const Application::TeacherCreateRequest request{
        .fields = Domain::TeacherProfileFields{
            .teacherKr = utf16(koreanName),
            .teacherEn = utf16(englishName),
            .preferredRomanization = utf16(preferredSpelling),
            .preferredName = utf16(preferredName),
            .roomNumber = utf16(room),
            .wifiName = utf16(m_teacherWifiNameEdit->text().trimmed()),
            .wifiPassword = utf16(m_teacherWifiPasswordEdit->text()),
            .internetType = utf16(
                m_teacherInternetTypeCombo->currentData().toString()
                ),
            .zoomId = utf16(m_teacherZoomIdEdit->text().trimmed()),
            .zoomPassword = utf16(m_teacherZoomPasswordEdit->text()),
            .projectionType = utf16(
                m_teacherProjectionTypeCombo->currentData().toString()
                )
        }
    };
    const Application::TeacherCreateResult saved = m_actions.createTeacher
        ? m_actions.createTeacher(request)
        : Application::TeacherCreateResult::failure({
              .code = Domain::ErrorCode::Technical,
              .message = "The teacher could not be saved.",
              .recoverable = true
          });
    if (!saved)
    {
        QString message = teacherEntryText(
            "The teacher could not be saved. Your entries are still here; retry to save."
            );
        if (!saved.error().message.empty())
        {
            message += QLatin1Char('\n') + qString(saved.error().message);
        }
        m_teacherEntryStatusLabel->setText(message);
        return;
    }

    if (addAnother)
    {
        clearTeacherFields();
        m_teacherEntryStatusLabel->setText(teacherEntryText(
            "Teacher saved. Enter another teacher or continue to the next step."
            ));
        return;
    }

    m_teacherEntryComplete = true;
    m_teacherEntryStatusLabel->setText(teacherEntryText(
        "Teacher saved. Setup remains pending."
        ));
    updateTeacherEntryControls();
}

void InitialPage::importTeachersFromWorkbook()
{
    if (!m_actions.state || !m_actions.state().setupPending)
    {
        return;
    }

    InitialSetupTeacherImportDialog dialog(
        InitialSetupTeacherImportActions{
            .chooseWorkbook = m_actions.teacherImport.chooseWorkbook,
            .validateWorkbook = m_actions.teacherImport.validateWorkbook,
            .validationTimeoutMilliseconds =
                m_actions.teacherImport.validationTimeoutMilliseconds,
            .apply = m_actions.teacherImport.apply,
            .cancelSetup = m_actions.cancelSetup
        },
        this
        );
    if (dialog.exec() == QDialog::Accepted)
    {
        m_teacherEntryStatusLabel->setText(dialog.successSummary());
    }
    refreshSetupState();
}

void InitialPage::clearTeacherFields()
{
    for (QLineEdit* edit : {
             m_teacherEnglishNameEdit,
             m_teacherKoreanNameEdit,
             m_teacherPreferredSpellingEdit,
             m_teacherRoomEdit,
             m_teacherWifiNameEdit,
             m_teacherWifiPasswordEdit,
             m_teacherZoomIdEdit,
             m_teacherZoomPasswordEdit
         })
    {
        edit->clear();
    }
    m_teacherInternetTypeCombo->setCurrentIndex(0);
    m_teacherProjectionTypeCombo->setCurrentIndex(0);
    updatePreferredTeacherNames();
}

void InitialPage::updatePreferredTeacherNames()
{
    QStringList choices;
    for (const QString& choice : {
             m_teacherEnglishNameEdit->text().trimmed(),
             m_teacherPreferredSpellingEdit->text().trimmed()
         })
    {
        if (!choice.isEmpty() && !choices.contains(choice))
        {
            choices.append(choice);
        }
    }

    const QString previous = m_teacherPreferredNameCombo->currentText();
    const QSignalBlocker blocker(m_teacherPreferredNameCombo);
    m_teacherPreferredNameCombo->clear();
    m_teacherPreferredNameCombo->addItems(choices);
    m_teacherPreferredNameCombo->setEnabled(!choices.isEmpty());
    const int selected = choices.indexOf(previous);
    m_teacherPreferredNameCombo->setCurrentIndex(
        selected >= 0 ? selected : (choices.isEmpty() ? -1 : 0)
        );
}

void InitialPage::updateTeacherEntryControls()
{
    const bool enabled = !m_teacherEntryComplete;
    const std::array<QWidget*, 13> controls{
        m_teacherEnglishNameEdit,
        m_teacherKoreanNameEdit,
        m_teacherPreferredSpellingEdit,
        m_teacherRoomEdit,
        m_teacherPreferredNameCombo,
        m_teacherInternetTypeCombo,
        m_teacherWifiNameEdit,
        m_teacherWifiPasswordEdit,
        m_teacherProjectionTypeCombo,
        m_teacherZoomIdEdit,
        m_teacherZoomPasswordEdit,
        m_addAnotherTeacherButton,
        m_continueTeacherEntryButton
    };
    for (QWidget* control : controls)
    {
        control->setEnabled(enabled);
    }
}

void InitialPage::refreshPersonalDetails()
{
    if (m_setupWasPending)
    {
        loadPersonalDetails();
    }
}

void InitialPage::loadPersonalDetails()
{
    const Application::PersonalDetailsReadResult loaded = m_actions.loadPersonalDetails
        ? m_actions.loadPersonalDetails()
        : Application::PersonalDetailsReadResult::failure({
              .code = Domain::ErrorCode::Technical,
              .message = "Personal details could not be loaded.",
              .recoverable = true
          });

    if (!loaded)
    {
        QString message = setupDetailsText(
            "Personal details could not be loaded. Your form is unchanged; retry to load saved values."
            );
        if (!loaded.error().message.empty())
        {
            message += QLatin1Char('\n') + qString(loaded.error().message);
        }
        setPersonalDetailsStatus(message);
        m_retryPersonalDetailsButton->show();
        m_personalDetailsLoaded = false;
        updatePersonalDetailsControls();
        return;
    }

    m_personalDetails = loaded.value();
    if (m_personalDetailsEdited)
    {
        syncPersonalDetailsFromForm();
    }
    else
    {
        m_updatingPersonalDetails = true;
        m_personalNameEdit->setText(qString(m_personalDetails.name));
        m_imageSignature = QByteArray(
            m_personalDetails.signatureImage.value().data(),
            static_cast<qsizetype>(m_personalDetails.signatureImage.value().size())
            );
        m_typedSignatureEdit->setText(
            qString(m_personalDetails.typedSignatureText)
            );
        const bool typeMode = m_personalDetails.signatureMode
            == Application::PersonalSignatureMode::Type;
        m_typedSignatureModeButton->setChecked(typeMode);
        m_imageSignatureModeButton->setChecked(!typeMode);
        const int fontIndex = m_signatureFontCombo->findData(
            static_cast<int>(m_personalDetails.typedSignatureFont)
            );
        m_signatureFontCombo->setCurrentIndex(qMax(0, fontIndex));
        m_updatingPersonalDetails = false;
    }

    m_personalDetailsLoaded = true;
    m_personalDetailsEdited = false;
    m_retryPersonalDetailsButton->hide();
    setPersonalDetailsStatus(QString());
    updatePersonalDetailsControls();
    updateSignaturePreview();
}

void InitialPage::savePersonalDetails()
{
    if (!m_personalDetailsLoaded)
    {
        setPersonalDetailsStatus(setupDetailsText(
            "Load the saved personal details before saving."
            ));
        return;
    }

    const QString trimmedName = m_personalNameEdit->text().trimmed();
    if (!Application::initialSetupPersonalNameRequirementMet(
            !trimmedName.isEmpty()
            ))
    {
        setPersonalDetailsStatus(
            setupDetailsText("Enter your name before saving.")
            );
        return;
    }

    syncPersonalDetailsFromForm();
    const Application::PersonalDetailsSaveResult saved = m_actions.savePersonalDetails
        ? m_actions.savePersonalDetails(m_personalDetails)
        : Application::PersonalDetailsSaveResult::failure({
              .code = Domain::ErrorCode::Technical,
              .message = "Personal details could not be saved.",
              .recoverable = true
          });

    if (!saved)
    {
        QString message = setupDetailsText(
            "Personal details could not be saved. Setup remains pending; retry to save."
            );
        if (!saved.error().message.empty())
        {
            message += QLatin1Char('\n') + qString(saved.error().message);
        }
        setPersonalDetailsStatus(message);
        m_personalDetailsEdited = true;
        return;
    }

    m_personalDetailsEdited = false;
    setPersonalDetailsStatus(
        setupDetailsText("Personal details saved. Setup is still pending.")
        );
}

void InitialPage::syncPersonalDetailsFromForm()
{
    m_personalDetails.name = utf8(m_personalNameEdit->text().trimmed());
    m_personalDetails.signatureMode = m_typedSignatureModeButton->isChecked()
        ? Application::PersonalSignatureMode::Type
        : Application::PersonalSignatureMode::Image;
    m_personalDetails.typedSignatureText = utf8(m_typedSignatureEdit->text());
    m_personalDetails.typedSignatureFont = applicationFont(
        m_signatureFontCombo->currentData()
        );

    const QByteArray signatureBytes = m_personalDetails.signatureMode
            == Application::PersonalSignatureMode::Type
        ? TypedSignature::renderForEmbedding(
              m_typedSignatureEdit->text(),
              rendererFont(m_personalDetails.typedSignatureFont)
              )
        : m_imageSignature;
    m_personalDetails.signatureImage = Domain::PersonalSignatureImageSaveBytes(
        bytes(signatureBytes)
        );
}

void InitialPage::updatePersonalDetailsControls()
{
    const bool imageMode = m_imageSignatureModeButton->isChecked();
    m_chooseSignatureButton->setVisible(imageMode);
    m_removeSignatureButton->setVisible(imageMode);
    m_removeSignatureButton->setEnabled(!m_imageSignature.isEmpty());
    m_typedSignatureControls->setVisible(!imageMode);
    m_savePersonalDetailsButton->setEnabled(m_personalDetailsLoaded);
}

void InitialPage::updateSignaturePreview()
{
    if (m_typedSignatureModeButton->isChecked())
    {
        const QString signatureText = m_typedSignatureEdit->text();
        if (signatureText.trimmed().isEmpty())
        {
            m_signaturePreviewLabel->setPixmap(QPixmap());
            m_signaturePreviewLabel->setText(
                personalDetailsText("Type your signature to see a preview.")
                );
            return;
        }

        const int fontIndex = m_signatureFontCombo->currentIndex();
        const TypedSignatureFont font = TypedSignature::fontFromStoredValue(
            fontIndex < 0 ? 0 : m_signatureFontCombo->currentData().toInt()
            );
        const QImage preview = TypedSignature::render(
            signatureText,
            font,
            m_signaturePreviewLabel->contentsRect()
                .adjusted(16, 12, -16, -12)
                .size()
            );
        m_signaturePreviewLabel->setText(QString());
        m_signaturePreviewLabel->setPixmap(QPixmap::fromImage(preview));
        return;
    }

    if (m_imageSignature.isEmpty())
    {
        m_signaturePreviewLabel->setPixmap(QPixmap());
        m_signaturePreviewLabel->setText(
            personalDetailsText("No signature image added")
            );
        return;
    }

    QPixmap signature;
    if (!signature.loadFromData(m_imageSignature))
    {
        m_imageSignature.clear();
        m_signaturePreviewLabel->setPixmap(QPixmap());
        m_signaturePreviewLabel->setText(
            personalDetailsText("No signature image added")
            );
        return;
    }

    m_signaturePreviewLabel->setText(QString());
    m_signaturePreviewLabel->setPixmap(signature.scaled(
        m_signaturePreviewLabel->contentsRect()
            .adjusted(16, 12, -16, -12)
            .size(),
        Qt::KeepAspectRatio,
        Qt::SmoothTransformation
        ));
}

void InitialPage::setPersonalDetailsStatus(const QString& message)
{
    m_personalDetailsStatusLabel->setText(message);
    m_personalDetailsStatusLabel->setVisible(!message.isEmpty());
}

} // namespace ClassMngr::Next::Ui
