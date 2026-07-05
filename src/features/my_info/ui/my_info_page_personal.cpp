#include "my_info_page.h"

#include "core/application_services.h"
#include "core/resource_paths.h"
#include "data/data_service.h"
#include "features/campus/data/campus_json_repository.h"
#include "ui/shared/constants/gui_constants.h"
#include "ui/shared/styles/roles.h"
#include "ui/shared/utils/widget_sizing.h"
#include "ui/shared/widgets/no_wheel_combobox.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFrame>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QSignalBlocker>
#include <QTimer>
#include <QVariant>
#include <QVBoxLayout>

namespace
{
constexpr int UntitledCardTopMargin = 4;
constexpr int CompactFieldWidth = 170;
constexpr int MyInformationFieldVerticalPadding = 14;
const QString NotAvailableText =
    QStringLiteral("N/A");

DataService* openDataService(
    ApplicationServices* services
    )
{
    auto* dataService =
        services
            ? services->dataService()
            : nullptr;

    return dataService && dataService->isOpen()
        ? dataService
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

namespace SettingsKeys
{
const QString Name =
    QStringLiteral("myInfo/name");
const QString Campus =
    QStringLiteral("myInfo/campus");
const QString ZoomLoginId =
    QStringLiteral("myInfo/zoomLoginId");
const QString ZoomPassword =
    QStringLiteral("myInfo/zoomPassword");
const QString ZoomNotAvailable =
    QStringLiteral("myInfo/zoomNotAvailable");

const QString LegacyZoomEmail =
    QStringLiteral("subPrep/personalZoomEmail");
const QString LegacyZoomPassword =
    QStringLiteral("subPrep/personalZoomPassword");
const QString LegacyZoomNotAvailable =
    QStringLiteral("subPrep/personalZoomNotAvailable");
}

QVariant loadSettingWithLegacyFallback(
    DataService* dataService,
    const QString& primaryKey,
    const QString& legacyKey,
    const QVariant& defaultValue
    )
{
    QVariant value =
        dataService->loadSetting(
            primaryKey,
            QVariant()
            );

    if (value.isValid())
    {
        return value;
    }

    value =
        dataService->loadSetting(
            legacyKey,
            QVariant()
            );

    if (value.isValid())
    {
        dataService->saveSetting(
            primaryKey,
            value
            );
        return value;
    }

    return defaultValue;
}
}

void MyInfoPage::handleEditableChanged()
{
    if (m_loading)
    {
        return;
    }

    updateCalendarCampusFilter();

    m_dirty = true;

    if (
        m_autosaveTimer
        && m_saveMode == SaveMode::Automatic
        )
    {
        m_autosaveTimer->start();
    }
}
void MyInfoPage::handleZoomNotAvailableChanged(
    bool checked
    )
{
    Q_UNUSED(checked);

    setZoomFieldsEnabled();
    handleEditableChanged();
}
void MyInfoPage::autosave()
{
    if (!hasUnsavedChanges())
    {
        return;
    }

    saveMyInfoInternal();
}
void MyInfoPage::buildMyInformationSection()
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
        &MyInfoPage::handleEditableChanged
        );
    connect(
        m_campusCombo,
        &QComboBox::currentTextChanged,
        this,
        &MyInfoPage::handleEditableChanged
        );
    connect(
        m_zoomLoginIdEdit,
        &QLineEdit::textChanged,
        this,
        &MyInfoPage::handleEditableChanged
        );
    connect(
        m_zoomPasswordEdit,
        &QLineEdit::textChanged,
        this,
        &MyInfoPage::handleEditableChanged
        );
    connect(
        m_zoomNotAvailableCheck,
        &QCheckBox::toggled,
        this,
        &MyInfoPage::handleZoomNotAvailableChanged
        );
}
void MyInfoPage::loadPageData()
{
    m_loading = true;

    if (m_autosaveTimer)
    {
        m_autosaveTimer->stop();
    }

    loadStoredSettings();
    refreshGeneratedContent();

    m_loading = false;
    clearDirty();
}
void MyInfoPage::loadStoredSettings()
{
    auto* dataService =
        openDataService(m_services);

    if (!dataService)
    {
        return;
    }

    const QSignalBlocker nameBlocker(m_nameEdit);
    const QSignalBlocker campusBlocker(m_campusCombo);
    const QSignalBlocker loginBlocker(m_zoomLoginIdEdit);
    const QSignalBlocker passwordBlocker(m_zoomPasswordEdit);
    const QSignalBlocker checkBlocker(m_zoomNotAvailableCheck);

    m_nameEdit->setText(
        dataService
            ->loadSetting(
                SettingsKeys::Name,
                QString()
                )
            .toString()
        );

    const QString campus =
        dataService
            ->loadSetting(
                SettingsKeys::Campus,
                QString()
                )
            .toString();

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
        dataService->saveSetting(
            SettingsKeys::Campus,
            m_campusCombo->currentText()
            );
    }

    const QString loginId =
        loadSettingWithLegacyFallback(
            dataService,
            SettingsKeys::ZoomLoginId,
            SettingsKeys::LegacyZoomEmail,
            NotAvailableText
            )
            .toString();
    const QString password =
        loadSettingWithLegacyFallback(
            dataService,
            SettingsKeys::ZoomPassword,
            SettingsKeys::LegacyZoomPassword,
            NotAvailableText
            )
            .toString();

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
    m_zoomNotAvailableCheck->setChecked(
        loadSettingWithLegacyFallback(
            dataService,
            SettingsKeys::ZoomNotAvailable,
            SettingsKeys::LegacyZoomNotAvailable,
            true
            )
            .toBool()
        );

    setZoomFieldsEnabled();
    updateMyInformationFieldWidths();
    updateCalendarCampusFilter();
}
bool MyInfoPage::saveMyInfoInternal()
{
    auto* dataService =
        openDataService(m_services);

    if (!dataService)
    {
        return false;
    }

    if (m_autosaveTimer)
    {
        m_autosaveTimer->stop();
    }

    if (
        !m_zoomNotAvailableCheck
        || !m_zoomNotAvailableCheck->isChecked()
        )
    {
        normalizeZoomFields();
    }

    dataService->saveSetting(
        SettingsKeys::Name,
        m_nameEdit->text()
        );
    dataService->saveSetting(
        SettingsKeys::Campus,
        m_campusCombo->currentText()
        );
    dataService->saveSetting(
        SettingsKeys::ZoomLoginId,
        m_zoomLoginIdEdit->text()
        );
    dataService->saveSetting(
        SettingsKeys::ZoomPassword,
        m_zoomPasswordEdit->text()
        );
    dataService->saveSetting(
        SettingsKeys::ZoomNotAvailable,
        m_zoomNotAvailableCheck->isChecked()
        );

    clearDirty();
    return true;
}
bool MyInfoPage::normalizeZoomFields()
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
bool MyInfoPage::normalizeLineEdit(
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
void MyInfoPage::setZoomFieldsEnabled()
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
void MyInfoPage::updateMyInformationFieldWidths()
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
void MyInfoPage::clearDirty()
{
    m_dirty = false;

    if (m_autosaveTimer)
    {
        m_autosaveTimer->stop();
    }
}
