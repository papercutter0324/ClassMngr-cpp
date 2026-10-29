#include "next/ui/campus_directory_page.h"

#include "next/application/campus_dashboard_campus_save_use_case.h"
#include "next/application/campus_directory_read_query.h"

#include <QComboBox>
#include <QCheckBox>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QImage>
#include <QLabel>
#include <QLineEdit>
#include <QPalette>
#include <QPlainTextEdit>
#include <QPixmap>
#include <QPushButton>
#include <QScrollArea>
#include <QSizePolicy>
#include <QSignalBlocker>
#include <QUrl>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>

namespace ClassMngr::Next::Ui
{
namespace
{

QString campusText(const char* sourceText)
{
    return QCoreApplication::translate("CampusDirectoryPage", sourceText);
}

QString fromUtf8(const std::string& value)
{
    return QString::fromUtf8(value.data(), static_cast<qsizetype>(value.size()));
}

std::string toUtf8(const QString& value)
{
    const QByteArray bytes = value.toUtf8();
    return std::string(bytes.constData(), static_cast<std::size_t>(bytes.size()));
}

QString jsonText(const std::string& value)
{
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(
        QByteArray::fromStdString(value),
        &error
        );
    if (error.error != QJsonParseError::NoError)
    {
        return fromUtf8(value);
    }
    return QString::fromUtf8(document.toJson(QJsonDocument::Indented));
}

std::string compactJsonText(const QString& value)
{
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(value.toUtf8(), &error);
    if (error.error != QJsonParseError::NoError)
    {
        return toUtf8(value);
    }
    return toUtf8(QString::fromUtf8(document.toJson(QJsonDocument::Compact)));
}

QString errorText(const Domain::OperationError& error)
{
    return QString::fromUtf8(
        error.message.data(),
        static_cast<qsizetype>(error.message.size())
        );
}

QStringList splitLines(const std::vector<std::string>& values)
{
    QStringList lines;
    lines.reserve(static_cast<qsizetype>(values.size()));
    for (const std::string& value : values)
    {
        lines.append(fromUtf8(value));
    }
    return lines;
}

std::vector<std::string> linesFromText(const QString& value)
{
    std::vector<std::string> lines;
    const QStringList values = value.split(u'\n');
    lines.reserve(static_cast<std::size_t>(values.size()));
    for (const QString& line : values)
    {
        if (!line.trimmed().isEmpty())
        {
            lines.push_back(toUtf8(line));
        }
    }
    return lines;
}

class CallbackCampusDirectoryReadPort final
    : public Application::CampusDirectoryReadPort
{
public:
    explicit CallbackCampusDirectoryReadPort(
        const std::function<Application::CampusDirectoryReadResult()>& read
        )
        : m_read(&read)
    {
    }

    [[nodiscard]] Application::CampusDirectoryReadResult loadCampuses() const override
    {
        return (*m_read)();
    }

private:
    const std::function<Application::CampusDirectoryReadResult()>* m_read;
};

QString sectionText(const CampusDirectorySection section)
{
    switch (section)
    {
    case CampusDirectorySection::Information: return campusText("Office Information");
    case CampusDirectorySection::Directions: return campusText("Directions");
    case CampusDirectorySection::Address: return campusText("Address");
    case CampusDirectorySection::Housing: return campusText("Housing");
    case CampusDirectorySection::Maps: return campusText("Maps");
    }
    return campusText("Campus Directory");
}

QWidget* createSectionCard(const QString& title, QWidget* parent)
{
    auto* card = new QFrame(parent);
    card->setObjectName(QStringLiteral("campusDirectoryCard"));
    card->setFrameShape(QFrame::StyledPanel);
    auto* layout = new QVBoxLayout(card);
    layout->setContentsMargins(16, 14, 16, 14);
    auto* heading = new QLabel(title, card);
    QFont font = heading->font();
    font.setBold(true);
    font.setPointSize(font.pointSize() + 1);
    heading->setFont(font);
    layout->addWidget(heading);
    return card;
}

} // namespace

CampusDirectoryPage::CampusDirectoryPage(
    const PageContext& context,
    const CampusDirectorySection section,
    CampusDirectoryPageActions actions,
    QWidget* parent
    )
    : Page(context, parent)
    , m_section(section)
    , m_actions(std::move(actions))
{
    setObjectName(QStringLiteral("nextCampusDirectoryPage_%1")
        .arg(static_cast<int>(section)));
    buildUi();
}

CampusDirectoryPage::~CampusDirectoryPage()
{
    clearMapResources();
}

PageOperationResult<void> CampusDirectoryPage::onActivating()
{
    if (!m_loaded)
    {
        loadSelectedCampusFromPreference();
    }
    else if (m_section == CampusDirectorySection::Maps && m_currentCampus.has_value())
    {
        loadMapResources();
    }
    return {};
}

bool CampusDirectoryPage::hasUnsavedChanges() const noexcept
{
    return m_dirty;
}

PageOperationResult<void> CampusDirectoryPage::saveChanges()
{
    if (!m_dirty || !m_currentCampus.has_value())
    {
        return {};
    }
    if (!m_actions.saveCampus)
    {
        const QString message = campusText("Campus saving is unavailable.");
        setStatus(message, true);
        return std::unexpected(PageOperationError{message});
    }

    const Application::CampusDashboardCampusSnapshot candidate = collectFields();
    const Domain::Result<void> saved = m_actions.saveCampus(
        candidate,
        Application::CampusDashboardCampusSaveIntent::Update
        );
    if (!saved)
    {
        const QString message = errorText(saved.error());
        setStatus(message, true);
        return std::unexpected(PageOperationError{message});
    }

    m_currentCampus = candidate;
    m_dirty = false;
    setHasUnsavedChanges(false);
    setStatus(campusText("Campus changes saved."));
    refreshMapLinkButtons();
    if (m_section == CampusDirectorySection::Maps)
    {
        loadMapResources();
    }
    return {};
}

PageOperationResult<void> CampusDirectoryPage::discardChanges()
{
    if (!m_currentCampus.has_value())
    {
        m_dirty = false;
        setHasUnsavedChanges(false);
        return {};
    }
    loadCurrentCampus(m_currentCampus->id);
    if (m_dirty)
    {
        const QString message = campusText("Campus changes could not be reloaded.");
        setStatus(message, true);
        return std::unexpected(PageOperationError{message});
    }
    return {};
}

void CampusDirectoryPage::onSuspending() noexcept
{
    clearMapResources();
}

void CampusDirectoryPage::buildUi()
{
    auto* pageLayout = new QVBoxLayout(this);
    pageLayout->setContentsMargins(22, 20, 22, 18);
    pageLayout->setSpacing(12);

    auto* headerLayout = new QHBoxLayout();
    m_title = new QLabel(pageTitle(), this);
    m_title->setObjectName(QStringLiteral("campusDirectoryTitle"));
    QFont titleFont = m_title->font();
    titleFont.setBold(true);
    titleFont.setPointSize(titleFont.pointSize() + 5);
    m_title->setFont(titleFont);
    headerLayout->addWidget(m_title, 1);
    m_campusSelector = new QComboBox(this);
    m_campusSelector->setObjectName(QStringLiteral("campusDirectoryCampusSelector"));
    m_campusSelector->setMinimumWidth(220);
    headerLayout->addWidget(m_campusSelector);
    pageLayout->addLayout(headerLayout);

    auto* actionRow = new QHBoxLayout();
    m_status = new QLabel(this);
    m_status->setObjectName(QStringLiteral("campusDirectoryStatus"));
    m_status->setWordWrap(true);
    actionRow->addWidget(m_status, 1);

    m_retryButton = new QPushButton(campusText("Retry"), this);
    m_retryButton->setObjectName(QStringLiteral("campusDirectoryRetry"));
    actionRow->addWidget(m_retryButton);
    connect(m_retryButton, &QPushButton::clicked, this, [this] { retry(); });

    m_importButton = new QPushButton(campusText("Import legacy campus data"), this);
    m_importButton->setObjectName(QStringLiteral("campusDirectoryImportLegacy"));
    m_importButton->setVisible(
        m_actions.editCapability.canEdit()
        && m_section == CampusDirectorySection::Information
        );
    actionRow->addWidget(m_importButton);
    connect(m_importButton, &QPushButton::clicked, this, [this] { importLegacy(); });

    m_discardButton = new QPushButton(campusText("Discard"), this);
    m_discardButton->setObjectName(QStringLiteral("campusDirectoryDiscard"));
    m_discardButton->setVisible(m_actions.editCapability.canEdit());
    m_discardButton->setEnabled(false);
    actionRow->addWidget(m_discardButton);
    connect(m_discardButton, &QPushButton::clicked, this, [this] {
        static_cast<void>(discardChanges());
    });

    m_saveButton = new QPushButton(campusText("Save"), this);
    m_saveButton->setObjectName(QStringLiteral("campusDirectorySave"));
    m_saveButton->setVisible(m_actions.editCapability.canEdit());
    m_saveButton->setEnabled(false);
    actionRow->addWidget(m_saveButton);
    connect(m_saveButton, &QPushButton::clicked, this, [this] {
        static_cast<void>(saveChanges());
    });
    pageLayout->addLayout(actionRow);

    if (!m_actions.editCapability.canEdit())
    {
        auto* readOnlyNotice = new QLabel(
            campusText("Campus information is read-only in this session."),
            this
            );
        readOnlyNotice->setObjectName(QStringLiteral("campusDirectoryReadOnlyNotice"));
        pageLayout->addWidget(readOnlyNotice);
    }

    auto* scroll = new QScrollArea(this);
    scroll->setObjectName(QStringLiteral("campusDirectoryScrollArea"));
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    m_formContainer = new QWidget(scroll);
    m_formContainer->setObjectName(QStringLiteral("campusDirectoryForm"));
    m_formLayout = new QVBoxLayout(m_formContainer);
    m_formLayout->setContentsMargins(0, 0, 0, 0);
    m_formLayout->setSpacing(12);
    buildSectionForm();
    scroll->setWidget(m_formContainer);
    pageLayout->addWidget(scroll, 1);

    connect(
        m_campusSelector,
        qOverload<int>(&QComboBox::currentIndexChanged),
        this,
        [this](const int index)
        {
            if (m_loading || index < 0) return;
            if (m_dirty)
            {
                const QSignalBlocker blocker(m_campusSelector);
                const int current = m_campusSelector->findData(
                    QString::fromUtf8(m_currentCampus->id.value().c_str())
                    );
                m_campusSelector->setCurrentIndex(current);
                setStatus(campusText("Save or discard edits before changing campuses."), true);
                return;
            }
            const auto campusId = Domain::CampusId::fromString(
                m_campusSelector->currentData().toString().toUtf8().toStdString()
                );
            if (!campusId) return;

            if (m_actions.saveLastSelectedCampus)
            {
                const Domain::Result<void> saved = m_actions.saveLastSelectedCampus(*campusId);
                if (!saved) setStatus(errorText(saved.error()), true);
            }
            loadCurrentCampus(*campusId);
        }
        );
}

void CampusDirectoryPage::buildSectionForm()
{
    auto addLine = [this](const QString& key, const char* label)
    {
        auto* card = createSectionCard(campusText(label), m_formContainer);
        auto* form = new QFormLayout();
        form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
        auto* field = new QLineEdit(card);
        field->setObjectName(QStringLiteral("campusField_%1").arg(key));
        field->setReadOnly(!m_actions.editCapability.canEdit());
        connect(field, &QLineEdit::textEdited, this, [this] { fieldEdited(); });
        form->addRow(campusText(label), field);
        qobject_cast<QVBoxLayout*>(card->layout())->addLayout(form);
        m_lineFields.insert(key, field);
        m_formLayout->addWidget(card);
    };
    auto addText = [this](const QString& key, const char* label, int minimumHeight)
    {
        auto* card = createSectionCard(campusText(label), m_formContainer);
        auto* form = new QFormLayout();
        form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
        auto* field = new QPlainTextEdit(card);
        field->setObjectName(QStringLiteral("campusField_%1").arg(key));
        field->setMinimumHeight(minimumHeight);
        field->setReadOnly(!m_actions.editCapability.canEdit());
        connect(field, &QPlainTextEdit::textChanged, this, [this] { fieldEdited(); });
        form->addRow(campusText(label), field);
        qobject_cast<QVBoxLayout*>(card->layout())->addLayout(form);
        m_textFields.insert(key, field);
        m_formLayout->addWidget(card);
    };

    switch (m_section)
    {
    case CampusDirectorySection::Information:
        addLine(QStringLiteral("campusName"), "Campus");
        addLine(QStringLiteral("campusCode"), "Campus code");
        addLine(QStringLiteral("buildingName"), "Building");
        addLine(QStringLiteral("phoneNumber"), "Phone");
        addLine(QStringLiteral("officeNumber"), "Office");
        addLine(QStringLiteral("officeWifi"), "Office Wi-Fi");
        addLine(QStringLiteral("officeWifiPassword"), "Wi-Fi password");
        addLine(QStringLiteral("printerName"), "Printer");
        addText(QStringLiteral("printerSteps"), "Printer setup", 100);
        addLine(QStringLiteral("printerDriverUrl"), "Printer driver link");
        m_printerDriverUnavailable = new QCheckBox(
            campusText("Printer driver link unavailable"),
            m_formContainer
            );
        m_printerDriverUnavailable->setObjectName(
            QStringLiteral("campusPrinterDriverUnavailable")
            );
        m_printerDriverUnavailable->setEnabled(
            m_actions.editCapability.canEdit()
            );
        connect(
            m_printerDriverUnavailable,
            &QCheckBox::toggled,
            this,
            [this] { fieldEdited(); }
            );
        m_formLayout->addWidget(m_printerDriverUnavailable);
        addLine(QStringLiteral("photocopierCode"), "Photocopier code");
        break;
    case CampusDirectorySection::Directions:
        addText(QStringLiteral("directionsNote"), "Notes for arrival", 100);
        addText(QStringLiteral("transitSteps"), "Public transit", 150);
        addText(QStringLiteral("arrivalInfo"), "On arrival", 120);
        break;
    case CampusDirectorySection::Address:
        addText(QStringLiteral("address"), "Complete address", 110);
        addLine(QStringLiteral("buildingName"), "Building name (English)");
        addLine(QStringLiteral("buildingNameKr"), "Building name (Korean)");
        addText(QStringLiteral("directionsAddressEnJson"), "English address details", 220);
        addText(QStringLiteral("directionsAddressKrJson"), "Korean address details", 220);
        break;
    case CampusDirectorySection::Housing:
        addText(QStringLiteral("housingLocationsJson"), "Housing locations", 360);
        break;
    case CampusDirectorySection::Maps:
    {
        auto* card = createSectionCard(campusText("Online map links"), m_formContainer);
        auto* form = new QFormLayout();
        auto* naver = new QLineEdit(card);
        naver->setObjectName(QStringLiteral("campusField_naverMapUrl"));
        naver->setReadOnly(!m_actions.editCapability.canEdit());
        connect(naver, &QLineEdit::textEdited, this, [this] {
            fieldEdited(); refreshMapLinkButtons();
        });
        form->addRow(campusText("Naver Map"), naver);
        m_lineFields.insert(QStringLiteral("naverMapUrl"), naver);
        auto* kakao = new QLineEdit(card);
        kakao->setObjectName(QStringLiteral("campusField_kakaoMapUrl"));
        kakao->setReadOnly(!m_actions.editCapability.canEdit());
        connect(kakao, &QLineEdit::textEdited, this, [this] {
            fieldEdited(); refreshMapLinkButtons();
        });
        form->addRow(campusText("Kakao Map"), kakao);
        m_lineFields.insert(QStringLiteral("kakaoMapUrl"), kakao);
        auto* openRow = new QHBoxLayout();
        auto* openNaver = new QPushButton(campusText("Open Naver Map"), card);
        openNaver->setObjectName(QStringLiteral("campusMapNaver"));
        connect(openNaver, &QPushButton::clicked, this, [this] {
            openHttpsUrl(m_lineFields.value(QStringLiteral("naverMapUrl"))->text());
        });
        auto* openKakao = new QPushButton(campusText("Open Kakao Map"), card);
        openKakao->setObjectName(QStringLiteral("campusMapKakao"));
        connect(openKakao, &QPushButton::clicked, this, [this] {
            openHttpsUrl(m_lineFields.value(QStringLiteral("kakaoMapUrl"))->text());
        });
        openRow->addWidget(openNaver);
        openRow->addWidget(openKakao);
        form->addRow(openRow);
        qobject_cast<QVBoxLayout*>(card->layout())->addLayout(form);
        m_formLayout->addWidget(card);
        break;
    }
    }
    m_formLayout->addStretch(1);
}

void CampusDirectoryPage::loadSelectedCampusFromPreference()
{
    if (!m_actions.loadCampuses)
    {
        setStatus(campusText("Campus directory is unavailable."), true);
        return;
    }
    const CallbackCampusDirectoryReadPort port(m_actions.loadCampuses);
    const Application::CampusDirectoryReadResult loaded =
        Application::CampusDirectoryReadQuery::execute(port);
    if (!loaded)
    {
        setStatus(errorText(loaded.error()), true);
        return;
    }
    m_directory = loaded.value();
    m_loading = true;
    {
        const QSignalBlocker blocker(m_campusSelector);
        m_campusSelector->clear();
        for (const Application::CampusDirectoryEntry& campus : m_directory)
        {
            m_campusSelector->addItem(
                QString::fromUtf8(
                    campus.displayName.data(),
                    static_cast<qsizetype>(campus.displayName.size())
                    ),
                QString::fromUtf8(campus.id.value().c_str())
                );
        }
    }
    m_loading = false;
    m_loaded = true;

    if (m_directory.empty())
    {
        m_currentCampus.reset();
        setStatus(campusText("No campuses are available."), true);
        return;
    }

    const std::optional<Domain::CampusId> saved = m_actions.readLastSelectedCampus
        ? m_actions.readLastSelectedCampus()
        : std::nullopt;
    int selectedIndex = -1;
    if (saved.has_value())
    {
        selectedIndex = m_campusSelector->findData(
            QString::fromUtf8(saved->value().c_str())
            );
    }
    if (selectedIndex < 0)
    {
        selectedIndex = 0;
        const auto fallback = Domain::CampusId::fromString(
            m_campusSelector->itemData(selectedIndex).toString().toUtf8().toStdString()
            );
        if (fallback && m_actions.saveLastSelectedCampus)
        {
            const Domain::Result<void> savedFallback =
                m_actions.saveLastSelectedCampus(*fallback);
            if (!savedFallback) setStatus(errorText(savedFallback.error()), true);
        }
    }
    m_campusSelector->setCurrentIndex(selectedIndex);
    const auto selectedId = Domain::CampusId::fromString(
        m_campusSelector->itemData(selectedIndex).toString().toUtf8().toStdString()
        );
    if (selectedId) loadCurrentCampus(*selectedId);
}

void CampusDirectoryPage::loadCurrentCampus(const Domain::CampusId& campusId)
{
    if (!m_actions.loadCampus)
    {
        setStatus(campusText("Campus details are unavailable."), true);
        return;
    }
    const Application::CampusDashboardSelectedCampusReadResult loaded =
        m_actions.loadCampus(campusId);
    if (!loaded)
    {
        setStatus(errorText(loaded.error()), true);
        return;
    }
    if (!loaded.value().has_value())
    {
        setStatus(campusText("The selected campus could not be found."), true);
        return;
    }
    m_currentCampus = loaded.value().value();
    m_dirty = false;
    setHasUnsavedChanges(false);
    populateFields(*m_currentCampus);
    setStatus(QString());
    refreshMapLinkButtons();
    clearMapResources();
    if (m_section == CampusDirectorySection::Maps)
    {
        loadMapResources();
    }
}

void CampusDirectoryPage::populateFields(
    const Application::CampusDashboardCampusSnapshot& campus
    )
{
    m_loading = true;
    const auto setLine = [this](const QString& key, const std::string& value)
    {
        if (QLineEdit* field = m_lineFields.value(key, nullptr))
        {
            field->setText(fromUtf8(value));
        }
    };
    const auto setText = [this](const QString& key, const QString& value)
    {
        if (QPlainTextEdit* field = m_textFields.value(key, nullptr))
        {
            field->setPlainText(value);
        }
    };
    setLine(QStringLiteral("campusName"), campus.campusName);
    setLine(QStringLiteral("campusCode"), campus.campusCode);
    setLine(QStringLiteral("buildingName"), campus.buildingName);
    setLine(QStringLiteral("buildingNameKr"), campus.buildingNameKr);
    setLine(QStringLiteral("phoneNumber"), campus.phoneNumber);
    setLine(QStringLiteral("officeNumber"), campus.officeNumber);
    setLine(QStringLiteral("officeWifi"), campus.officeWifi);
    setLine(QStringLiteral("officeWifiPassword"), campus.officeWifiPassword);
    setLine(QStringLiteral("printerName"), campus.printerName);
    setLine(QStringLiteral("printerDriverUrl"), campus.printerDriverUrl);
    if (m_printerDriverUnavailable)
    {
        m_printerDriverUnavailable->setChecked(
            campus.printerDriverUrlUnavailable
            );
    }
    setLine(QStringLiteral("photocopierCode"), campus.photocopierCode);
    setLine(QStringLiteral("naverMapUrl"), campus.naverMapUrl);
    setLine(QStringLiteral("kakaoMapUrl"), campus.kakaoMapUrl);
    setText(QStringLiteral("printerSteps"), fromUtf8(campus.printerSteps));
    setText(QStringLiteral("directionsNote"), fromUtf8(campus.directionsNote));
    setText(QStringLiteral("transitSteps"), splitLines(campus.transitSteps).join(u'\n'));
    setText(QStringLiteral("arrivalInfo"), fromUtf8(campus.arrivalInfo));
    setText(QStringLiteral("address"), fromUtf8(campus.address));
    setText(QStringLiteral("directionsAddressEnJson"), jsonText(campus.directionsAddressEnJson));
    setText(QStringLiteral("directionsAddressKrJson"), jsonText(campus.directionsAddressKrJson));
    setText(QStringLiteral("housingLocationsJson"), jsonText(campus.housingLocationsJson));
    m_loading = false;
    updateDirtyState();
}

Application::CampusDashboardCampusSnapshot CampusDirectoryPage::collectFields() const
{
    Application::CampusDashboardCampusSnapshot campus = *m_currentCampus;
    const auto line = [this](const QString& key, const std::string& fallback)
    {
        const QLineEdit* field = m_lineFields.value(key, nullptr);
        return field ? toUtf8(field->text()) : fallback;
    };
    const auto text = [this](const QString& key, const std::string& fallback)
    {
        const QPlainTextEdit* field = m_textFields.value(key, nullptr);
        return field ? toUtf8(field->toPlainText()) : fallback;
    };
    campus.campusName = line(QStringLiteral("campusName"), campus.campusName);
    campus.campusCode = line(QStringLiteral("campusCode"), campus.campusCode);
    campus.buildingName = line(QStringLiteral("buildingName"), campus.buildingName);
    campus.buildingNameKr = line(QStringLiteral("buildingNameKr"), campus.buildingNameKr);
    campus.phoneNumber = line(QStringLiteral("phoneNumber"), campus.phoneNumber);
    campus.officeNumber = line(QStringLiteral("officeNumber"), campus.officeNumber);
    campus.officeWifi = line(QStringLiteral("officeWifi"), campus.officeWifi);
    campus.officeWifiPassword = line(QStringLiteral("officeWifiPassword"), campus.officeWifiPassword);
    campus.printerName = line(QStringLiteral("printerName"), campus.printerName);
    campus.printerSteps = text(QStringLiteral("printerSteps"), campus.printerSteps);
    campus.printerDriverUrl = line(QStringLiteral("printerDriverUrl"), campus.printerDriverUrl);
    if (m_printerDriverUnavailable)
    {
        campus.printerDriverUrlUnavailable = m_printerDriverUnavailable->isChecked();
    }
    campus.photocopierCode = line(QStringLiteral("photocopierCode"), campus.photocopierCode);
    campus.naverMapUrl = line(QStringLiteral("naverMapUrl"), campus.naverMapUrl);
    campus.kakaoMapUrl = line(QStringLiteral("kakaoMapUrl"), campus.kakaoMapUrl);
    campus.directionsNote = text(QStringLiteral("directionsNote"), campus.directionsNote);
    campus.transitSteps = linesFromText(
        m_textFields.contains(QStringLiteral("transitSteps"))
            ? m_textFields.value(QStringLiteral("transitSteps"))->toPlainText()
            : splitLines(campus.transitSteps).join(u'\n')
        );
    campus.arrivalInfo = text(QStringLiteral("arrivalInfo"), campus.arrivalInfo);
    campus.address = text(QStringLiteral("address"), campus.address);
    campus.directionsAddressEnJson = compactJsonText(
        m_textFields.contains(QStringLiteral("directionsAddressEnJson"))
            ? m_textFields.value(QStringLiteral("directionsAddressEnJson"))->toPlainText()
            : jsonText(campus.directionsAddressEnJson)
        );
    campus.directionsAddressKrJson = compactJsonText(
        m_textFields.contains(QStringLiteral("directionsAddressKrJson"))
            ? m_textFields.value(QStringLiteral("directionsAddressKrJson"))->toPlainText()
            : jsonText(campus.directionsAddressKrJson)
        );
    campus.housingLocationsJson = compactJsonText(
        m_textFields.contains(QStringLiteral("housingLocationsJson"))
            ? m_textFields.value(QStringLiteral("housingLocationsJson"))->toPlainText()
            : jsonText(campus.housingLocationsJson)
        );
    return campus;
}

void CampusDirectoryPage::clearMapResources() noexcept
{
    m_mapImages.clear();
    for (QWidget* card : m_mapCards)
    {
        delete card;
    }
    m_mapCards.clear();
}

void CampusDirectoryPage::loadMapResources()
{
    clearMapResources();
    if (!m_currentCampus.has_value() || !m_actions.mapResourcesFor)
    {
        return;
    }
    const auto resources = m_actions.mapResourcesFor(*m_currentCampus);
    if (!resources)
    {
        auto* problem = new QLabel(errorText(resources.error()), m_formContainer);
        problem->setObjectName(QStringLiteral("campusMapLoadError"));
        problem->setWordWrap(true);
        m_formLayout->insertWidget(0, problem);
        m_mapCards.push_back(problem);
        setStatus(errorText(resources.error()), true);
        return;
    }
    if (resources.value().empty())
    {
        auto* empty = new QLabel(campusText("No map images are available for this campus."), m_formContainer);
        empty->setObjectName(QStringLiteral("campusMapEmpty"));
        m_formLayout->insertWidget(0, empty);
        m_mapCards.push_back(empty);
        return;
    }

    for (std::size_t index = 0; index < resources.value().size(); ++index)
    {
        const Resources::ResourceId& id = resources.value().at(index);
        const auto request = Resources::ResourceRequest::create(
            id,
            resourceContext().pageScope()
            );
        if (!request)
        {
            setStatus(request.error().message, true);
            continue;
        }
        auto loaded = resourceContext().resourceLoader().loadBoundedImage(*request, 1520);
        auto* card = createSectionCard(
            campusText("Campus map"),
            m_formContainer
            );
        card->setObjectName(QStringLiteral("campusMapImageCard"));
        auto* content = qobject_cast<QVBoxLayout*>(card->layout());
        auto* imageLabel = new QLabel(card);
        imageLabel->setObjectName(QStringLiteral("campusMapImage"));
        imageLabel->setAlignment(Qt::AlignCenter);
        imageLabel->setMinimumHeight(90);
        imageLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        if (!loaded)
        {
            imageLabel->setText(campusText("Map image could not be loaded."));
            imageLabel->setProperty("resourceError", loaded.error().message);
            setStatus(loaded.error().message, true);
        }
        else
        {
            const QImage& image = loaded->get();
            imageLabel->setPixmap(QPixmap::fromImage(image));
            imageLabel->setMinimumHeight(std::min(image.height(), 280));
            m_mapImages.push_back(std::move(loaded.value()));
        }
        content->addWidget(imageLabel);
        m_formLayout->insertWidget(static_cast<int>(index), card);
        m_mapCards.push_back(card);
    }
}

void CampusDirectoryPage::refreshMapLinkButtons()
{
    const auto update = [this](const QString& name, const QString& fieldKey)
    {
        if (auto* button = findChild<QPushButton*>(name))
        {
            const QLineEdit* field = m_lineFields.value(fieldKey, nullptr);
            const QUrl url(field ? field->text() : QString());
            button->setEnabled(url.scheme().compare(QStringLiteral("https"), Qt::CaseInsensitive) == 0
                && !url.host().isEmpty());
        }
    };
    update(QStringLiteral("campusMapNaver"), QStringLiteral("naverMapUrl"));
    update(QStringLiteral("campusMapKakao"), QStringLiteral("kakaoMapUrl"));
}

void CampusDirectoryPage::updateDirtyState()
{
    if (m_loading || !m_currentCampus.has_value()) return;
    const Application::CampusDashboardCampusSnapshot candidate = collectFields();
    const bool dirty = candidate != *m_currentCampus;
    if (m_dirty != dirty)
    {
        m_dirty = dirty;
        setHasUnsavedChanges(dirty);
    }
    if (m_saveButton) m_saveButton->setEnabled(dirty);
    if (m_discardButton) m_discardButton->setEnabled(dirty);
}

void CampusDirectoryPage::setStatus(const QString& text, const bool isError)
{
    if (!m_status) return;
    m_status->setText(text);
    QPalette palette = m_status->palette();
    palette.setColor(
        QPalette::WindowText,
        isError ? palette.color(QPalette::BrightText) : palette.color(QPalette::WindowText)
        );
    m_status->setPalette(palette);
}

void CampusDirectoryPage::retry()
{
    if (m_dirty)
    {
        static_cast<void>(saveChanges());
        return;
    }
    m_loaded = false;
    loadSelectedCampusFromPreference();
}

void CampusDirectoryPage::importLegacy()
{
    if (!m_actions.editCapability.canEdit() || !m_actions.importLegacyCampuses)
    {
        setStatus(campusText("Legacy campus import is unavailable."), true);
        return;
    }
    if (m_dirty)
    {
        setStatus(campusText("Save or discard edits before importing legacy campus data."), true);
        return;
    }
    const Domain::Result<std::size_t> result = m_actions.importLegacyCampuses();
    if (!result)
    {
        setStatus(errorText(result.error()), true);
        return;
    }
    m_loaded = false;
    loadSelectedCampusFromPreference();
    setStatus(campusText("Legacy campus data imported."));
}

void CampusDirectoryPage::fieldEdited()
{
    updateDirtyState();
}

void CampusDirectoryPage::openHttpsUrl(const QString& urlText)
{
    const QUrl url(urlText);
    if (url.scheme().compare(QStringLiteral("https"), Qt::CaseInsensitive) != 0
        || url.host().isEmpty())
    {
        return;
    }
    if (!QDesktopServices::openUrl(url))
    {
        setStatus(campusText("The map link could not be opened."), true);
    }
}

QString CampusDirectoryPage::pageTitle() const
{
    return sectionText(m_section);
}

} // namespace ClassMngr::Next::Ui
