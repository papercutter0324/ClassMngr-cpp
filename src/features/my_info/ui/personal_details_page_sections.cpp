#include "personal_details_page.h"
#include "ui/shared/dialogs/user_prompt_service.h"
#include "ui/shared/pages/autosave_coordinator.h"

#include "core/application_services.h"
#include "app/services/feature_services.h"
#include "core/resource_paths.h"
#include "features/campus/data/campus_json_repository.h"
#include "features/my_info/data/personal_details_repository.h"
#include "features/my_info/data/signature_image_processor.h"
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
constexpr int SignaturePreviewHeight = 220;
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
            tr("Add a PNG or JPEG signature image. Other supported image formats are converted to PNG."),
            card
            );
    m_signatureInstructionsLabel->setObjectName(
        "sectionSubtitle"
        );
    m_signatureInstructionsLabel->setWordWrap(true);
    cardLayout->addWidget(
        m_signatureInstructionsLabel
        );

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

    actionsLayout->addWidget(
        m_chooseSignatureButton
        );
    actionsLayout->addWidget(
        m_removeSignatureButton
        );
    cardLayout->addLayout(
        actionsLayout
        );

    m_scrollContentLayout->addWidget(
        card
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

    setZoomFieldsEnabled();
    updateMyInformationFieldWidths();
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
void PersonalDetailsPage::updateSignaturePreview()
{
    if (!m_signaturePreviewLabel)
    {
        return;
    }

    if (m_signatureImageData.isEmpty())
    {
        m_signaturePreviewLabel->setPixmap(QPixmap());
        m_signaturePreviewLabel->setText(
            tr("No signature image added")
            );

        if (m_chooseSignatureButton)
        {
            m_chooseSignatureButton->setText(
                tr("Add Signature Image...")
                );
        }

        if (m_removeSignatureButton)
        {
            m_removeSignatureButton->setEnabled(false);
        }
        return;
    }

    QPixmap signature;
    if (!signature.loadFromData(m_signatureImageData))
    {
        m_signatureImageData.clear();
        updateSignaturePreview();
        return;
    }

    const QSize previewSize =
        m_signaturePreviewLabel
            ->contentsRect()
            .adjusted(16, 16, -16, -16)
            .size();

    m_signaturePreviewLabel->setText(QString());
    m_signaturePreviewLabel->setPixmap(
        signature.scaled(
            previewSize,
            Qt::KeepAspectRatio,
            Qt::SmoothTransformation
            )
        );

    if (m_chooseSignatureButton)
    {
        m_chooseSignatureButton->setText(
            tr("Replace Signature Image...")
            );
    }

    if (m_removeSignatureButton)
    {
        m_removeSignatureButton->setEnabled(true);
    }
}
void PersonalDetailsPage::clearDirty()
{
    m_autosave->markClean();
}
