#include "personal_details_page.h"
#include "ui/shared/dialogs/user_prompt_service.h"
#include "ui/shared/pages/autosave_coordinator.h"

#include "core/application_services.h"
#include "app/services/feature_services.h"
#include "core/resource_paths.h"
#include "features/campus/data/campus_json_repository.h"
#include "features/my_info/data/personal_details_repository.h"
#include "features/my_info/data/signature_image_processor.h"
#include "features/my_info/data/typed_signature_renderer.h"
#include "ui/shared/constants/gui_constants.h"
#include "ui/shared/dialogs/file_dialog_service.h"
#include "ui/shared/styles/roles.h"
#include "ui/shared/utils/widget_sizing.h"
#include "ui/shared/widgets/no_wheel_combobox.h"
#include "ui/shared/widgets/text_fit_push_button.h"

#include <QCheckBox>
#include <QComboBox>
#include <QEvent>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QImage>
#include <QImageReader>
#include <QLabel>
#include <QLineEdit>
#include <QPixmap>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QTimer>
#include <QVBoxLayout>

namespace
{
constexpr int UntitledCardTopMargin = 4;
constexpr int CompactFieldWidth = 170;
constexpr int MyInformationFieldVerticalPadding = 14;
constexpr int SignaturePreviewHeight = 120;
const QString NotAvailableText =
    QStringLiteral("N/A");

QStringList supportedImagePatterns()
{
    QStringList patterns;

    for (const QByteArray& format : QImageReader::supportedImageFormats())
    {
        const QString suffix =
            QString::fromLatin1(format).toLower();

        if (!suffix.isEmpty())
        {
            patterns.append(
                QStringLiteral("*.%1").arg(suffix)
                );
        }

        if (suffix == QStringLiteral("jpeg"))
        {
            patterns.append(QStringLiteral("*.jpg"));
        }
    }

    patterns.removeDuplicates();
    patterns.sort(Qt::CaseInsensitive);
    return patterns;
}

SettingsService* openSettingsService(
    ApplicationServices* services
    )
{
    auto* settingsService =
        services
            ? services->settingsService()
            : nullptr;

    return settingsService && settingsService->isAvailable()
        ? settingsService
        : nullptr;
}

CampusJsonRepository campusRepository()
{
    return CampusJsonRepository(
        ResourcePaths::Campuses::directory()
        );
}

QString campusDisplayName(
    const CampusInfo& campus
    )
{
    return campus.campusName.trimmed().isEmpty()
        ? campus.id.trimmed()
        : campus.campusName.trimmed();
}

int findCampusIndex(
    QComboBox* combo,
    const QString& savedCampus
    )
{
    if (!combo || savedCampus.trimmed().isEmpty())
    {
        return -1;
    }

    const QString normalized =
        savedCampus.trimmed();

    for (int index = 0; index < combo->count(); ++index)
    {
        if (
            combo->itemData(index).toString().compare(
                normalized,
                Qt::CaseInsensitive
                ) == 0
            || combo->itemText(index).compare(
                normalized,
                Qt::CaseInsensitive
                ) == 0
            )
        {
            return index;
        }
    }

    return -1;
}

int readableFieldHeight(
    const QWidget* field
    )
{
    if (!field)
    {
        return 0;
    }

    const QFontMetrics metrics(field->font());

    return metrics.lineSpacing()
        + qMax(
            MyInformationFieldVerticalPadding,
            (metrics.descent() * 2) + 8
            );
}

void matchFieldHeights(
    QWidget* first,
    QWidget* second,
    QWidget* third,
    QWidget* fourth
    )
{
    int fieldHeight = 0;

    const auto includeField =
        [&fieldHeight](QWidget* field)
        {
            if (!field)
            {
                return;
            }

            fieldHeight =
                qMax(
                    fieldHeight,
                    qMax(
                        qMax(
                            field->sizeHint().height(),
                            field->minimumSizeHint().height()
                            ),
                        readableFieldHeight(field)
                        )
                    );
        };

    includeField(first);
    includeField(second);
    includeField(third);
    includeField(fourth);

    const auto applyHeight =
        [fieldHeight](QWidget* field)
        {
            if (!field || fieldHeight <= 0)
            {
                return;
            }

            field->setMinimumHeight(fieldHeight);
            field->updateGeometry();
        };

    applyHeight(first);
    applyHeight(second);
    applyHeight(third);
    applyHeight(fourth);
}

}

bool PersonalDetailsPage::eventFilter(
    QObject* watched,
    QEvent* event
    )
{
    if (
        event
        && watched == m_signaturePreviewLabel
        && event->type() == QEvent::Resize
        )
    {
        QTimer::singleShot(
            0,
            this,
            [this]()
            {
                updateSignaturePreview();
            }
            );
    }

    if (
        event
        && (
            watched == m_nameEdit
            || watched == m_campusCombo
            || watched == m_zoomLoginIdEdit
            || watched == m_zoomPasswordEdit
            )
        && (
            event->type() == QEvent::FontChange
            || event->type() == QEvent::ApplicationFontChange
            || event->type() == QEvent::Polish
            || event->type() == QEvent::Show
            || event->type() == QEvent::StyleChange
            )
        )
    {
        QTimer::singleShot(
            0,
            this,
            [this]()
            {
                updateMyInformationFieldWidths();
            }
            );
    }

    return BasePage::eventFilter(
        watched,
        event
        );
}

void PersonalDetailsPage::handleEditableChanged()
{
    if (m_autosave->isLoading())
    {
        return;
    }

    m_autosave->markDirty();
}
void PersonalDetailsPage::handleZoomNotAvailableChanged(
    bool checked
    )
{
    Q_UNUSED(checked);

    setZoomFieldsEnabled();
    handleEditableChanged();
}

void PersonalDetailsPage::selectSignatureMode()
{
    setSignatureMode(
        sender() == m_signatureTypeModeButton
            ? SignatureMode::Type
            : SignatureMode::Image
        );
}

void PersonalDetailsPage::selectTypedSignatureFont()
{
    const int index = m_typedSignatureFontButtons.indexOf(
        qobject_cast<QPushButton*>(sender())
        );

    if (index < 0)
    {
        return;
    }

    const TypedSignatureFont selectedFont =
        TypedSignature::fontFromStoredValue(index);

    if (m_typedSignatureFont == selectedFont)
    {
        return;
    }

    m_typedSignatureFont = selectedFont;
    updateTypedSignatureImage();
    handleEditableChanged();
}

void PersonalDetailsPage::handleTypedSignatureChanged()
{
    updateTypedSignatureImage();
    handleEditableChanged();
}

void PersonalDetailsPage::chooseSignatureImage()
{
    const QStringList patterns =
        supportedImagePatterns();
    const QString filter =
        patterns.isEmpty()
            ? tr("PNG and JPEG Images (*.png *.jpg *.jpeg)")
            : tr("Supported Images (%1)")
                .arg(patterns.join(QLatin1Char(' ')));

    const std::optional<QString> selection =
        DialogServices::fileDialogs().openFile(
            OpenFileRequest{
                .parent = this,
                .title = tr("Choose Signature Image"),
                .purpose = FileDialogPurpose::SignatureImage,
                .nameFilters = {filter}
            }
            );

    if (!selection)
    {
        return;
    }

    QImageReader reader(*selection);
    reader.setAutoTransform(true);

    const QImage image =
        reader.read();

    if (image.isNull())
    {
        DialogServices::showWarning(
            this,
            tr("Unsupported Signature Image"),
            tr("The selected file is not a supported image.")
            );
        return;
    }

    const QByteArray encodedImage =
        SignatureImage::prepareForEmbedding(image);
    if (encodedImage.isEmpty())
    {
        DialogServices::showWarning(
            this,
            tr("Signature Image Error"),
            tr("The signature image could not be prepared for embedding.")
            );
        return;
    }

    m_signatureImageData =
        encodedImage;
    m_signatureMode = SignatureMode::Image;
    updateSignatureControls();
    updateSignaturePreview();
    handleEditableChanged();
}
void PersonalDetailsPage::removeSignatureImage()
{
    if (m_signatureImageData.isEmpty())
    {
        return;
    }

    m_signatureImageData.clear();
    updateSignatureControls();
    updateSignaturePreview();
    handleEditableChanged();
}
void PersonalDetailsPage::buildMyInformationSection()
{
    m_myInformationHeading =
        createTopLevelHeading(
            tr("My Information"),
            m_scrollContent
            );
    m_scrollContentLayout->addWidget(
        m_myInformationHeading
        );

    auto* card =
        new QFrame(m_scrollContent);
    card->setProperty(
        "role",
        UiRoles::Card
        );
    card->setObjectName(
        "sectionCard"
        );

    auto* cardLayout =
        new QVBoxLayout(card);
    cardLayout->setAlignment(Qt::AlignTop);
    cardLayout->setContentsMargins(
        UiConstants::ClassInfo::SectionCard::Margin,
        UntitledCardTopMargin,
        UiConstants::ClassInfo::SectionCard::Margin,
        UiConstants::ClassInfo::SectionCard::Margin
        );
    cardLayout->setSpacing(
        UiConstants::ClassInfo::SectionCard::Spacing
        );

    auto* grid =
        new QGridLayout;
    grid->setHorizontalSpacing(
        UiConstants::ClassInfo::Form::HorizontalSpacing
        );
    grid->setVerticalSpacing(
        UiConstants::ClassInfo::Form::VerticalSpacing
        );

    m_nameEdit =
        new QLineEdit(card);
    m_campusCombo =
        new NoWheelComboBox(card);

    m_zoomLoginIdEdit =
        new QLineEdit(card);
    m_zoomPasswordEdit =
        new QLineEdit(card);
    m_zoomNotAvailableCheck =
        new QCheckBox(
            tr("N/A"),
            card
            );
    m_zoomNotAvailableCheck->setObjectName(
        "zoomNotAvailableCheck"
        );

    WidgetSizing::installTextAwareFieldWidth(
        m_nameEdit,
        UiConstants::Forms::FieldMinimumWidth
        );
    WidgetSizing::installTextAwareFieldWidth(
        m_campusCombo,
        UiConstants::Forms::FieldMinimumWidth
        );
    WidgetSizing::installTextAwareFieldWidth(
        m_zoomLoginIdEdit,
        UiConstants::Forms::FieldMinimumWidth
        );
    WidgetSizing::installTextAwareFieldWidth(
        m_zoomPasswordEdit,
        UiConstants::Forms::FieldMinimumWidth
        );
    matchFieldHeights(
        m_nameEdit,
        m_campusCombo,
        m_zoomLoginIdEdit,
        m_zoomPasswordEdit
        );

    m_nameEdit->installEventFilter(this);
    m_campusCombo->installEventFilter(this);
    m_zoomLoginIdEdit->installEventFilter(this);
    m_zoomPasswordEdit->installEventFilter(this);

    m_nameLabel =
        createFieldLabel(tr("My Name"), card);
    m_campusLabel =
        createFieldLabel(tr("My Campus"), card);
    m_zoomLoginIdLabel =
        createFieldLabel(tr("Zoom Login ID"), card);
    m_zoomPasswordLabel =
        createFieldLabel(tr("Zoom Password"), card);
    m_zoomLabel =
        createFieldLabel(tr("Zoom"), card);

    grid->addWidget(
        m_nameLabel,
        0,
        0,
        Qt::AlignLeft
        );
    grid->addWidget(
        m_campusLabel,
        0,
        1,
        Qt::AlignLeft
        );
    grid->addWidget(
        m_zoomLoginIdLabel,
        0,
        2,
        Qt::AlignLeft
        );
    grid->addWidget(
        m_zoomPasswordLabel,
        0,
        3,
        Qt::AlignLeft
        );
    grid->addWidget(
        m_zoomLabel,
        0,
        4,
        Qt::AlignLeft
        );

    grid->addWidget(
        m_nameEdit,
        1,
        0
        );
    grid->addWidget(
        m_campusCombo,
        1,
        1
        );
    grid->addWidget(
        m_zoomLoginIdEdit,
        1,
        2
        );
    grid->addWidget(
        m_zoomPasswordEdit,
        1,
        3
        );
    grid->addWidget(
        m_zoomNotAvailableCheck,
        1,
        4,
        Qt::AlignLeft | Qt::AlignVCenter
        );
    grid->setColumnStretch(0, 1);
    grid->setColumnStretch(1, 1);
    grid->setColumnStretch(2, 1);
    grid->setColumnStretch(3, 1);
    grid->setColumnStretch(5, 0);

    cardLayout->addLayout(
        grid
        );

    m_scrollContentLayout->addWidget(
        card
        );

    connect(
        m_nameEdit,
        &QLineEdit::textChanged,
        this,
        &PersonalDetailsPage::handleEditableChanged
        );
    connect(
        m_campusCombo,
        &QComboBox::currentTextChanged,
        this,
        &PersonalDetailsPage::handleEditableChanged
        );
    connect(
        m_zoomLoginIdEdit,
        &QLineEdit::textChanged,
        this,
        &PersonalDetailsPage::handleEditableChanged
        );
    connect(
        m_zoomPasswordEdit,
        &QLineEdit::textChanged,
        this,
        &PersonalDetailsPage::handleEditableChanged
        );
    connect(
        m_zoomNotAvailableCheck,
        &QCheckBox::toggled,
        this,
        &PersonalDetailsPage::handleZoomNotAvailableChanged
        );
}
void PersonalDetailsPage::buildSignatureSection()
{
    m_scrollContentLayout->addSpacing(
        UiConstants::Pages::MajorSectionSpacing
        );

    m_signatureHeading =
        createTopLevelHeading(
            tr("Signature"),
            m_scrollContent
            );
    m_scrollContentLayout->addWidget(
        m_signatureHeading
        );

    auto* card =
        new QFrame(m_scrollContent);
    card->setProperty(
        "role",
        UiRoles::Card
        );
    card->setObjectName(
        "sectionCard"
        );

    auto* cardLayout =
        new QVBoxLayout(card);
    cardLayout->setContentsMargins(
        UiConstants::ClassInfo::SectionCard::Margin,
        UiConstants::ClassInfo::SectionCard::Margin,
        UiConstants::ClassInfo::SectionCard::Margin,
        UiConstants::ClassInfo::SectionCard::Margin
        );
    cardLayout->setSpacing(
        UiConstants::ClassInfo::SectionCard::Spacing
        );

    m_signatureInstructionsLabel =
        new QLabel(
            tr("Choose an image file or type your signature."),
            card
            );
    m_signatureInstructionsLabel->setObjectName(
        "sectionSubtitle"
        );
    m_signatureInstructionsLabel->setWordWrap(true);
    cardLayout->addWidget(
        m_signatureInstructionsLabel
        );

    auto* modeLayout =
        new QHBoxLayout;
    modeLayout->setContentsMargins(0, 0, 0, 0);
    modeLayout->setSpacing(UiConstants::Pages::Spacing);

    m_signatureImageModeButton =
        new TextFitPushButton(
            tr("Image"),
            card
            );
    m_signatureImageModeButton->setObjectName(
        "signatureImageModeButton"
        );
    m_signatureImageModeButton->setCheckable(true);

    m_signatureTypeModeButton =
        new TextFitPushButton(
            tr("Type"),
            card
            );
    m_signatureTypeModeButton->setObjectName(
        "signatureTypeModeButton"
        );
    m_signatureTypeModeButton->setCheckable(true);

    modeLayout->addWidget(m_signatureImageModeButton);
    modeLayout->addWidget(m_signatureTypeModeButton);
    modeLayout->addStretch();
    cardLayout->addLayout(modeLayout);

    m_signaturePreviewLabel =
        new QLabel(card);
    m_signaturePreviewLabel->setObjectName(
        "signatureImagePreview"
        );
    m_signaturePreviewLabel->setAlignment(
        Qt::AlignCenter
        );
    m_signaturePreviewLabel->setFixedHeight(
        SignaturePreviewHeight
        );
    m_signaturePreviewLabel->setSizePolicy(
        QSizePolicy::Expanding,
        QSizePolicy::Fixed
        );
    m_signaturePreviewLabel->installEventFilter(this);
    cardLayout->addWidget(
        m_signaturePreviewLabel
        );

    m_signatureImageControls =
        new QWidget(card);
    auto* actionsLayout =
        new QHBoxLayout;
    actionsLayout->setContentsMargins(0, 0, 0, 0);
    actionsLayout->setSpacing(
        UiConstants::Pages::Spacing
        );
    actionsLayout->addStretch();

    m_chooseSignatureButton =
        new TextFitPushButton(
            tr("Add Signature Image..."),
            card
            );
    m_removeSignatureButton =
        new TextFitPushButton(
            tr("Remove"),
            card
            );

    actionsLayout->addWidget(m_chooseSignatureButton);
    actionsLayout->addWidget(m_removeSignatureButton);
    m_signatureImageControls->setLayout(actionsLayout);
    cardLayout->addWidget(m_signatureImageControls);

    m_typedSignatureControls =
        new QWidget(card);
    auto* typedLayout =
        new QVBoxLayout(m_typedSignatureControls);
    typedLayout->setContentsMargins(0, 0, 0, 0);
    typedLayout->setSpacing(UiConstants::Pages::Spacing);

    m_typedSignatureLabel =
        createFieldLabel(tr("Type your signature"), m_typedSignatureControls);
    typedLayout->addWidget(m_typedSignatureLabel);

    m_typedSignatureEdit =
        new QLineEdit(m_typedSignatureControls);
    m_typedSignatureEdit->setObjectName("typedSignatureEdit");
    m_typedSignatureEdit->setPlaceholderText(
        tr("Type your name")
        );
    typedLayout->addWidget(m_typedSignatureEdit);

    m_typedSignatureFontLabel =
        createFieldLabel(tr("Choose a style"), m_typedSignatureControls);
    typedLayout->addWidget(m_typedSignatureFontLabel);

    auto* fontGrid =
        new QGridLayout;
    fontGrid->setContentsMargins(0, 0, 0, 0);
    fontGrid->setHorizontalSpacing(UiConstants::Pages::Spacing);
    fontGrid->setVerticalSpacing(UiConstants::Pages::Spacing);

    const QList<TypedSignatureFont> signatureFonts = {
        TypedSignatureFont::JustAnotherHand,
        TypedSignatureFont::DancingScript,
        TypedSignatureFont::GreatVibes,
        TypedSignatureFont::Caveat
    };

    for (int index = 0; index < signatureFonts.size(); ++index)
    {
        const TypedSignatureFont signatureFont =
            signatureFonts.at(index);
        auto* fontCard =
            new QFrame(m_typedSignatureControls);
        fontCard->setObjectName("signatureFontCard");
        fontCard->setProperty("role", UiRoles::Card);

        auto* fontCardLayout =
            new QVBoxLayout(fontCard);
        fontCardLayout->setContentsMargins(12, 10, 12, 10);
        fontCardLayout->setSpacing(4);

        auto* fontName =
            new QLabel(
                TypedSignature::displayName(signatureFont),
                fontCard
                );
        fontName->setObjectName("signatureFontName");
        fontCardLayout->addWidget(fontName);

        auto* fontPreview =
            new QLabel(fontCard);
        fontPreview->setObjectName("typedSignatureFontPreview");
        fontPreview->setFixedHeight(64);
        fontPreview->setAlignment(Qt::AlignCenter);
        fontCardLayout->addWidget(fontPreview);

        auto* selectButton =
            new TextFitPushButton(
                tr("Use this font"),
                fontCard
                );
        selectButton->setObjectName("typedSignatureFontButton");
        selectButton->setCheckable(true);
        fontCardLayout->addWidget(selectButton);

        m_typedSignatureFontPreviews.append(fontPreview);
        m_typedSignatureFontButtons.append(selectButton);
        fontGrid->addWidget(fontCard, index / 2, index % 2);

        connect(
            selectButton,
            &QPushButton::clicked,
            this,
            &PersonalDetailsPage::selectTypedSignatureFont
            );
    }

    fontGrid->setColumnStretch(0, 1);
    fontGrid->setColumnStretch(1, 1);
    typedLayout->addLayout(fontGrid);
    cardLayout->addWidget(m_typedSignatureControls);

    m_scrollContentLayout->addWidget(
        card
        );

    connect(
        m_signatureImageModeButton,
        &QPushButton::clicked,
        this,
        &PersonalDetailsPage::selectSignatureMode
        );
    connect(
        m_signatureTypeModeButton,
        &QPushButton::clicked,
        this,
        &PersonalDetailsPage::selectSignatureMode
        );
    connect(
        m_chooseSignatureButton,
        &QPushButton::clicked,
        this,
        &PersonalDetailsPage::chooseSignatureImage
        );
    connect(
        m_removeSignatureButton,
        &QPushButton::clicked,
        this,
        &PersonalDetailsPage::removeSignatureImage
        );
    connect(
        m_typedSignatureEdit,
        &QLineEdit::textChanged,
        this,
        &PersonalDetailsPage::handleTypedSignatureChanged
        );

    updateSignatureControls();
    updateTypedSignatureFontOptions();
    updateSignaturePreview();
}
void PersonalDetailsPage::loadPageData()
{
    m_autosave->setLoading(true);

    loadStoredSettings();

    m_autosave->setLoading(false);
    clearDirty();
}
void PersonalDetailsPage::loadStoredSettings()
{
    auto* settingsService = openSettingsService(m_services);

    if (!settingsService)
    {
        return;
    }

    const QSignalBlocker nameBlocker(m_nameEdit);
    const QSignalBlocker campusBlocker(m_campusCombo);
    const QSignalBlocker loginBlocker(m_zoomLoginIdEdit);
    const QSignalBlocker passwordBlocker(m_zoomPasswordEdit);
    const QSignalBlocker checkBlocker(m_zoomNotAvailableCheck);
    const QSignalBlocker typedSignatureBlocker(m_typedSignatureEdit);

    const PersonalDetails details =
        PersonalDetailsRepository(settingsService).load();

    m_nameEdit->setText(details.name);

    const QString campus = details.campus;

    m_campusCombo->clear();

    const QList<CampusInfo> campuses =
        campusRepository().loadCampuses();

    for (const CampusInfo& campusInfo : campuses)
    {
        const QString displayName =
            campusDisplayName(campusInfo);

        if (displayName.isEmpty())
        {
            continue;
        }

        m_campusCombo->addItem(
            displayName,
            campusInfo.id
            );
    }

    const int campusIndex =
        findCampusIndex(
            m_campusCombo,
            campus
            );

    m_campusCombo->setCurrentIndex(
        campusIndex >= 0
            ? campusIndex
            : 0
        );

    if (
        m_campusCombo->currentIndex() >= 0
        && m_campusCombo->currentText().compare(
            campus.trimmed(),
            Qt::CaseInsensitive
            ) != 0
        )
    {
        [[maybe_unused]] const Status campusSaved =
            PersonalDetailsRepository(settingsService).saveCampus(
                m_campusCombo->currentText()
                );
    }

    const QString loginId = details.zoomLoginId;
    const QString password = details.zoomPassword;

    m_zoomLoginIdEdit->setText(
        loginId.trimmed().isEmpty()
            ? NotAvailableText
            : loginId
        );
    m_zoomPasswordEdit->setText(
        password.trimmed().isEmpty()
            ? NotAvailableText
            : password
        );
    m_zoomNotAvailableCheck->setChecked(details.zoomNotAvailable);
    m_signatureImageData = details.signatureImage;
    m_signatureMode = details.signatureMode;
    m_typedSignatureFont =
        TypedSignature::fontFromStoredValue(details.typedSignatureFont);
    m_typedSignatureEdit->setText(details.typedSignatureText);

    setZoomFieldsEnabled();
    updateMyInformationFieldWidths();
    updateSignatureControls();
    updateTypedSignatureFontOptions();
    updateSignaturePreview();
}
bool PersonalDetailsPage::saveMyInfoInternal()
{
    auto* settingsService = openSettingsService(m_services);

    if (!settingsService)
    {
        return false;
    }

    m_autosave->cancelPendingSave();

    if (
        !m_zoomNotAvailableCheck
        || !m_zoomNotAvailableCheck->isChecked()
        )
    {
        normalizeZoomFields();
    }

    PersonalDetails details;
    details.name = m_nameEdit->text();
    details.campus = m_campusCombo->currentText();
    details.zoomLoginId = m_zoomLoginIdEdit->text();
    details.zoomPassword = m_zoomPasswordEdit->text();
    details.zoomNotAvailable = m_zoomNotAvailableCheck->isChecked();
    details.signatureImage = m_signatureImageData;
    details.signatureMode = m_signatureMode;
    details.typedSignatureText = m_typedSignatureEdit->text();
    details.typedSignatureFont = static_cast<int>(m_typedSignatureFont);

    if (!PersonalDetailsRepository(settingsService).save(details))
    {
        return false;
    }

    clearDirty();
    return true;
}
bool PersonalDetailsPage::normalizeZoomFields()
{
    bool changed = false;

    changed =
        normalizeLineEdit(
            m_zoomLoginIdEdit,
            NotAvailableText
            )
        || changed;
    changed =
        normalizeLineEdit(
            m_zoomPasswordEdit,
            NotAvailableText
            )
        || changed;

    if (changed)
    {
        updateMyInformationFieldWidths();
    }

    return changed;
}
bool PersonalDetailsPage::normalizeLineEdit(
    QLineEdit* edit,
    const QString& defaultText
    )
{
    if (
        !edit
        || !edit->text().trimmed().isEmpty()
        )
    {
        return false;
    }

    const QSignalBlocker blocker(edit);

    edit->setText(
        defaultText
        );

    return true;
}
void PersonalDetailsPage::setZoomFieldsEnabled()
{
    const bool fieldsEnabled =
        !m_zoomNotAvailableCheck
        || !m_zoomNotAvailableCheck->isChecked();

    if (m_zoomLoginIdEdit)
    {
        m_zoomLoginIdEdit->setEnabled(
            fieldsEnabled
            );
    }

    if (m_zoomPasswordEdit)
    {
        m_zoomPasswordEdit->setEnabled(
            fieldsEnabled
            );
    }
}
void PersonalDetailsPage::updateMyInformationFieldWidths()
{
    WidgetSizing::updateTextAwareFieldWidth(
        m_nameEdit,
        UiConstants::Forms::FieldMinimumWidth
        );
    WidgetSizing::updateTextAwareFieldWidth(
        m_campusCombo,
        UiConstants::Forms::FieldMinimumWidth
        );
    WidgetSizing::updateTextAwareFieldWidth(
        m_zoomLoginIdEdit,
        UiConstants::Forms::FieldMinimumWidth
        );
    WidgetSizing::updateTextAwareFieldWidth(
        m_zoomPasswordEdit,
        UiConstants::Forms::FieldMinimumWidth
        );

    matchFieldHeights(
        m_nameEdit,
        m_campusCombo,
        m_zoomLoginIdEdit,
        m_zoomPasswordEdit
        );
}
void PersonalDetailsPage::setSignatureMode(SignatureMode mode)
{
    if (m_signatureMode == mode)
    {
        updateSignatureControls();
        return;
    }

    m_signatureMode = mode;

    if (m_signatureMode == SignatureMode::Type)
    {
        updateTypedSignatureImage();
    }

    updateSignatureControls();
    updateSignaturePreview();
    handleEditableChanged();
}

void PersonalDetailsPage::updateTypedSignatureImage()
{
    if (m_signatureMode != SignatureMode::Type
        || !m_typedSignatureEdit)
    {
        return;
    }

    m_signatureImageData =
        TypedSignature::renderForEmbedding(
            m_typedSignatureEdit->text(),
            m_typedSignatureFont
            );
    updateTypedSignatureFontOptions();
    updateSignaturePreview();
}

void PersonalDetailsPage::updateSignatureControls()
{
    const bool imageMode = m_signatureMode == SignatureMode::Image;

    if (m_signatureImageModeButton)
    {
        m_signatureImageModeButton->setChecked(imageMode);
    }

    if (m_signatureTypeModeButton)
    {
        m_signatureTypeModeButton->setChecked(!imageMode);
    }

    if (m_signatureImageControls)
    {
        m_signatureImageControls->setVisible(imageMode);
    }

    if (m_typedSignatureControls)
    {
        m_typedSignatureControls->setVisible(!imageMode);
    }

    if (m_chooseSignatureButton)
    {
        m_chooseSignatureButton->setText(
            m_signatureImageData.isEmpty()
                ? tr("Add Signature Image...")
                : tr("Replace Signature...")
            );
    }

    if (m_removeSignatureButton)
    {
        m_removeSignatureButton->setText(tr("Remove"));
        m_removeSignatureButton->setEnabled(
            !m_signatureImageData.isEmpty()
            );
    }
}

void PersonalDetailsPage::updateTypedSignatureFontOptions()
{
    const QString previewText =
        m_typedSignatureEdit
        && !m_typedSignatureEdit->text().trimmed().isEmpty()
            ? m_typedSignatureEdit->text()
            : tr("Your Signature");

    for (int index = 0;
         index < m_typedSignatureFontButtons.size();
         ++index)
    {
        const TypedSignatureFont option =
            TypedSignature::fontFromStoredValue(index);
        const bool selected = option == m_typedSignatureFont;
        QPushButton* const button =
            m_typedSignatureFontButtons.at(index);

        button->setChecked(selected);
        button->setText(
            selected
                ? tr("Selected")
                : tr("Use this font")
            );

        if (index >= m_typedSignatureFontPreviews.size())
        {
            continue;
        }

        QLabel* const preview =
            m_typedSignatureFontPreviews.at(index);
        preview->setPixmap(
            QPixmap::fromImage(
                TypedSignature::render(
                    previewText,
                    option,
                    QSize(240, 64)
                    )
                )
            );
    }
}

void PersonalDetailsPage::updateSignaturePreview()
{
    if (!m_signaturePreviewLabel)
    {
        return;
    }

    const QSize previewSize =
        m_signaturePreviewLabel
            ->contentsRect()
            .adjusted(16, 16, -16, -16)
            .size();

    if (m_signatureMode == SignatureMode::Type)
    {
        const QString typedSignature =
            m_typedSignatureEdit
            && !m_typedSignatureEdit->text().trimmed().isEmpty()
                ? m_typedSignatureEdit->text()
                : tr("Your Signature");

        const QImage preview =
            TypedSignature::render(
                typedSignature,
                m_typedSignatureFont,
                previewSize
                );

        m_signaturePreviewLabel->setText(QString());
        m_signaturePreviewLabel->setPixmap(
            QPixmap::fromImage(preview)
            );
        return;
    }

    if (m_signatureImageData.isEmpty())
    {
        m_signaturePreviewLabel->setPixmap(QPixmap());
        m_signaturePreviewLabel->setText(
            tr("No signature image added")
            );
        return;
    }

    QPixmap signature;
    if (!signature.loadFromData(m_signatureImageData))
    {
        m_signatureImageData.clear();
        updateSignaturePreview();
        return;
    }

    m_signaturePreviewLabel->setText(QString());
    m_signaturePreviewLabel->setPixmap(
        signature.scaled(
            previewSize,
            Qt::KeepAspectRatio,
            Qt::SmoothTransformation
            )
        );
}
void PersonalDetailsPage::clearDirty()
{
    m_autosave->markClean();
}
