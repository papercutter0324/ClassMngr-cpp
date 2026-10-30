#include "next/ui/preferences_dialog.h"

#include "next/application/excel_import_timeout_preferences.h"

#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QScopedValueRollback>
#include <QTabWidget>
#include <QUrl>
#include <QVBoxLayout>
#include <QDialogButtonBox>
#include <QEvent>

#include <exception>
#include <utility>

namespace ClassMngr::Next::Ui
{
namespace
{

[[nodiscard]] QString text(const char* source)
{
    return QCoreApplication::translate("PreferencesDialog", source);
}

[[nodiscard]] QString optionText(const char* source)
{
    return QCoreApplication::translate("ActionRegistry", source);
}

[[nodiscard]] Domain::OperationError invalidPreference(const char* message)
{
    return {
        .code = Domain::ErrorCode::InvalidInput,
        .message = message,
        .recoverable = false
    };
}

[[nodiscard]] bool isValidHttpsWebsite(const QString& value)
{
    const QUrl url(value.trimmed());
    return url.isValid()
        && url.scheme().compare(QStringLiteral("https"), Qt::CaseInsensitive) == 0
        && !url.host().isEmpty();
}

template <typename Value>
void appendItem(QComboBox* combo, const char* label, const Value value)
{
    combo->addItem(optionText(label), static_cast<int>(value));
}

[[nodiscard]] int findData(const QComboBox* combo, const int value)
{
    const int index = combo ? combo->findData(value) : -1;
    return index >= 0 ? index : 0;
}

void setFormLabel(QFormLayout* form, QWidget* field, const QString& value)
{
    if (form)
    {
        if (auto* const label = qobject_cast<QLabel*>(form->labelForField(field)))
        {
            label->setText(value);
        }
    }
}

}  // namespace

PreferencesDialog::PreferencesDialog(
    Application::PreferencesModel& preferences,
    PreferencesDialogPorts ports,
    PreferencesDialogActions actions,
    QWidget* parent
    )
    : QDialog(parent)
    , m_preferences(preferences)
    , m_ports(ports)
    , m_actions(std::move(actions))
{
    setObjectName(QStringLiteral("nextPreferencesDialog"));
    resize(700, 570);
    buildUi();

    const auto loaded = m_preferences.load();
    m_applicationPreferencesLoaded = static_cast<bool>(loaded);
    if (!loaded)
    {
        showFailure(
            "Unable to Load Preferences",
            loaded.error()
            );
    }

    try
    {
        populateValues();
    }
    catch (const std::exception& exception)
    {
        showFailure(
            "Unable to Load Preferences",
            Domain::OperationError{
                .code = Domain::ErrorCode::Technical,
                .message = exception.what(),
                .recoverable = true
            }
            );
    }
    catch (...)
    {
        showFailure(
            "Unable to Load Preferences",
            Domain::OperationError{
                .code = Domain::ErrorCode::Technical,
                .message = "The saved preference values could not be read.",
                .recoverable = true
            }
            );
    }
    updateControlAvailability();
    retranslateUi();
}

bool PreferencesDialog::applicationPreferencesLoaded() const noexcept
{
    return m_applicationPreferencesLoaded;
}

void PreferencesDialog::changeEvent(QEvent* event)
{
    QDialog::changeEvent(event);
    if (event && event->type() == QEvent::LanguageChange)
    {
        retranslateUi();
    }
}

void PreferencesDialog::buildUi()
{
    auto* const root = new QVBoxLayout(this);
    root->setContentsMargins(18, 16, 18, 14);
    root->setSpacing(10);

    m_error = new QLabel(this);
    m_error->setObjectName(QStringLiteral("preferencesError"));
    m_error->setWordWrap(true);
    m_error->setProperty("role", QStringLiteral("error"));
    m_error->hide();
    root->addWidget(m_error);

    m_tabs = new QTabWidget(this);
    m_tabs->setObjectName(QStringLiteral("preferencesTabs"));
    root->addWidget(m_tabs, 1);

    auto* generalPage = new QWidget(m_tabs);
    generalPage->setObjectName(QStringLiteral("preferencesGeneralTab"));
    auto* generalLayout = new QVBoxLayout(generalPage);
    generalLayout->setContentsMargins(10, 12, 10, 10);
    generalLayout->setSpacing(12);

    auto* saveGroup = new QGroupBox(generalPage);
    saveGroup->setObjectName(QStringLiteral("preferencesSaveModeGroup"));
    auto* saveLayout = new QFormLayout(saveGroup);
    m_saveMode = new QComboBox(saveGroup);
    m_saveMode->setObjectName(QStringLiteral("preferencesSaveMode"));
    appendItem(m_saveMode, "Automatic", Application::SaveMode::Automatic);
    appendItem(m_saveMode, "Manual", Application::SaveMode::Manual);
    saveLayout->addRow(text("Save Mode:"), m_saveMode);
    generalLayout->addWidget(saveGroup);

    auto* updatesGroup = new QGroupBox(generalPage);
    updatesGroup->setObjectName(QStringLiteral("preferencesUpdatesGroup"));
    auto* updatesLayout = new QVBoxLayout(updatesGroup);
    m_autoUpdates = new QCheckBox(updatesGroup);
    m_autoUpdates->setObjectName(QStringLiteral("preferencesAutomaticUpdates"));
    updatesLayout->addWidget(m_autoUpdates);
    generalLayout->addWidget(updatesGroup);

    auto* sidebarGroup = new QGroupBox(generalPage);
    sidebarGroup->setObjectName(QStringLiteral("preferencesSidebarGroup"));
    auto* sidebarLayout = new QVBoxLayout(sidebarGroup);
    m_sidebarTooltips = new QCheckBox(sidebarGroup);
    m_sidebarTooltips->setObjectName(QStringLiteral("preferencesSidebarTooltips"));
    m_sidebarAnimation = new QCheckBox(sidebarGroup);
    m_sidebarAnimation->setObjectName(QStringLiteral("preferencesSidebarAnimation"));
    sidebarLayout->addWidget(m_sidebarTooltips);
    sidebarLayout->addWidget(m_sidebarAnimation);
    generalLayout->addWidget(sidebarGroup);

    auto* timeoutGroup = new QGroupBox(generalPage);
    timeoutGroup->setObjectName(QStringLiteral("preferencesExcelImportsGroup"));
    auto* timeoutLayout = new QFormLayout(timeoutGroup);
    m_excelTimeout = new QComboBox(timeoutGroup);
    m_excelTimeout->setObjectName(QStringLiteral("preferencesExcelImportTimeout"));
    m_excelTimeout->addItem(text("30 Seconds"), 30);
    m_excelTimeout->addItem(text("1 Minute"), 60);
    m_excelTimeout->addItem(text("2 Minutes"), 120);
    m_excelTimeout->addItem(text("5 Minutes"), 300);
    timeoutLayout->addRow(text("Workbook Timeout:"), m_excelTimeout);
    generalLayout->addWidget(timeoutGroup);

#ifdef Q_OS_MACOS
    m_powerPointNotice = new QCheckBox(generalPage);
    m_powerPointNotice->setObjectName(QStringLiteral("preferencesPowerPointNotice"));
    generalLayout->addWidget(m_powerPointNotice);
#endif
    generalLayout->addStretch(1);
    m_tabs->addTab(generalPage, QString{});

    auto* appearancePage = new QWidget(m_tabs);
    appearancePage->setObjectName(QStringLiteral("preferencesAppearanceTab"));
    auto* appearanceLayout = new QFormLayout(appearancePage);
    appearanceLayout->setContentsMargins(10, 16, 10, 10);

    m_theme = new QComboBox(appearancePage);
    m_theme->setObjectName(QStringLiteral("preferencesTheme"));
    appendItem(m_theme, "System Default", Application::ThemePreference::SystemDefault);
    appendItem(m_theme, "Light Theme", Application::ThemePreference::Light);
    appendItem(m_theme, "Dark Theme", Application::ThemePreference::Dark);
    appearanceLayout->addRow(text("Theme:"), m_theme);

    m_language = new QComboBox(appearancePage);
    m_language->setObjectName(QStringLiteral("preferencesLanguage"));
    appendItem(m_language, "System Default", Application::LanguagePreference::SystemDefault);
    appendItem(m_language, "English", Application::LanguagePreference::English);
    appendItem(m_language, "Korean", Application::LanguagePreference::Korean);
    appearanceLayout->addRow(text("Language:"), m_language);

    m_fontSize = new QComboBox(appearancePage);
    m_fontSize->setObjectName(QStringLiteral("preferencesFontSize"));
    appendItem(m_fontSize, "Small", Application::FontSize::Small);
    appendItem(m_fontSize, "Normal", Application::FontSize::Normal);
    appendItem(m_fontSize, "Large", Application::FontSize::Large);
    appendItem(m_fontSize, "Extra Large", Application::FontSize::ExtraLarge);
    appearanceLayout->addRow(text("Font Size:"), m_fontSize);
    m_tabs->addTab(appearancePage, QString{});

    auto* documentsPage = new QWidget(m_tabs);
    documentsPage->setObjectName(QStringLiteral("preferencesDocumentsTab"));
    auto* documentsLayout = new QFormLayout(documentsPage);
    documentsLayout->setContentsMargins(10, 16, 10, 10);
    m_documentSpacing = new QComboBox(documentsPage);
    m_documentSpacing->setObjectName(QStringLiteral("preferencesDocumentSpacing"));
    appendItem(m_documentSpacing, "None", Application::DocumentPageSpacing::None);
    appendItem(m_documentSpacing, "Small", Application::DocumentPageSpacing::Small);
    appendItem(m_documentSpacing, "Medium", Application::DocumentPageSpacing::Medium);
    appendItem(m_documentSpacing, "Large", Application::DocumentPageSpacing::Large);
    documentsLayout->addRow(text("PDF Page Spacing:"), m_documentSpacing);
    m_documentBackground = new QComboBox(documentsPage);
    m_documentBackground->setObjectName(QStringLiteral("preferencesDocumentBackground"));
    appendItem(m_documentBackground, "Default", Application::DocumentViewerBackground::Default);
    appendItem(m_documentBackground, "White", Application::DocumentViewerBackground::White);
    appendItem(m_documentBackground, "Black", Application::DocumentViewerBackground::Black);
    documentsLayout->addRow(text("Background Color:"), m_documentBackground);
    m_tabs->addTab(documentsPage, QString{});

    auto* aiPage = new QWidget(m_tabs);
    aiPage->setObjectName(QStringLiteral("preferencesAiCommentsTab"));
    auto* aiLayout = new QFormLayout(aiPage);
    aiLayout->setContentsMargins(10, 16, 10, 10);
    m_aiProvider = new QComboBox(aiPage);
    m_aiProvider->setObjectName(QStringLiteral("preferencesAiCommentProvider"));
    appendItem(m_aiProvider, "ChatGPT", Application::AiCommentProvider::ChatGPT);
    appendItem(m_aiProvider, "Gemini", Application::AiCommentProvider::Gemini);
    appendItem(m_aiProvider, "Claude", Application::AiCommentProvider::Claude);
    appendItem(m_aiProvider, "Microsoft Copilot", Application::AiCommentProvider::MicrosoftCopilot);
    m_aiProvider->addItem(text("Custom Website…"),
                          static_cast<int>(Application::AiCommentProvider::CustomWebsite));
    aiLayout->addRow(text("Preferred AI Website:"), m_aiProvider);
    m_customWebsite = new QLabel(aiPage);
    m_customWebsite->setObjectName(QStringLiteral("preferencesCustomAiWebsiteUrl"));
    m_customWebsite->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_customWebsite->setWordWrap(true);
    aiLayout->addRow(QString{}, m_customWebsite);
    m_aiVoice = new QComboBox(aiPage);
    m_aiVoice->setObjectName(QStringLiteral("preferencesAiCommentVoice"));
    appendItem(m_aiVoice, "Direct to Student", Application::AiCommentVoice::DirectToStudent);
    appendItem(m_aiVoice, "Third Person", Application::AiCommentVoice::ThirdPerson);
    aiLayout->addRow(text("Comment Voice:"), m_aiVoice);
    m_tabs->addTab(aiPage, QString{});

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    buttons->setObjectName(QStringLiteral("preferencesButtons"));
    QObject::connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    QObject::connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    root->addWidget(buttons);

    connect(m_theme, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &PreferencesDialog::changeTheme);
    connect(m_language, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &PreferencesDialog::changeLanguage);
    connect(m_fontSize, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &PreferencesDialog::changeFontSize);
    connect(m_saveMode, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &PreferencesDialog::changeSaveMode);
    connect(m_autoUpdates, &QCheckBox::toggled, this, [this](const bool enabled) {
        static_cast<void>(saveApplicationPreference("Automatic Updates",
            [enabled](Application::UserPreferencesState& state) {
                return state.setAutomaticUpdateChecksEnabled(enabled);
            }));
    });
    connect(m_sidebarTooltips, &QCheckBox::toggled, this, [this](const bool enabled) {
        static_cast<void>(saveApplicationPreference("Sidebar Tooltips",
            [enabled](Application::UserPreferencesState& state) {
                return state.setSidebarTooltipsEnabled(enabled);
            }));
    });
    connect(m_sidebarAnimation, &QCheckBox::toggled, this, [this](const bool enabled) {
        static_cast<void>(saveApplicationPreference("Sidebar Animation",
            [enabled](Application::UserPreferencesState& state) {
                return state.setSidebarMarqueeEnabled(enabled);
            }));
    });
    connect(m_excelTimeout, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](const int index) {
        const auto choice = Application::tryExcelImportTimeoutSecondsToChoice(
            m_excelTimeout->itemData(index).toInt());
        if (!choice)
        {
            showFailure("Workbook Timeout", invalidPreference(
                "Excel import timeout must be 30, 60, 120, or 300 seconds."));
            const QSignalBlocker blocker(m_excelTimeout);
            m_excelTimeout->setCurrentIndex(findData(
                m_excelTimeout,
                m_preferences.snapshot().excelImportTimeoutSeconds()));
            return;
        }
        static_cast<void>(saveApplicationPreference("Workbook Timeout",
            [seconds = Application::excelImportTimeoutChoiceToSeconds(*choice)](
                Application::UserPreferencesState& state) {
                return state.setExcelImportTimeoutSeconds(seconds);
            }));
    });
#ifdef Q_OS_MACOS
    connect(m_powerPointNotice, &QCheckBox::toggled, this, [this](const bool enabled) {
        static_cast<void>(saveApplicationPreference("PowerPoint Notice",
            [enabled](Application::UserPreferencesState& state) {
                return state.setShowPowerPointDataAccessNotice(enabled);
            }));
    });
#endif
    connect(m_documentSpacing, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &PreferencesDialog::changeDocumentSpacing);
    connect(m_documentBackground, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &PreferencesDialog::changeDocumentBackground);
    connect(m_aiProvider, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &PreferencesDialog::changeAiProvider);
    connect(m_aiVoice, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &PreferencesDialog::changeAiVoice);
}

void PreferencesDialog::populateValues()
{
    QScopedValueRollback<bool> populating(m_populating, true);
    const QSignalBlocker blockerTheme(m_theme);
    const QSignalBlocker blockerLanguage(m_language);
    const QSignalBlocker blockerFont(m_fontSize);
    const QSignalBlocker blockerMode(m_saveMode);
    const QSignalBlocker blockerUpdates(m_autoUpdates);
    const QSignalBlocker blockerTooltips(m_sidebarTooltips);
    const QSignalBlocker blockerAnimation(m_sidebarAnimation);
    const QSignalBlocker blockerTimeout(m_excelTimeout);
    const QSignalBlocker blockerSpacing(m_documentSpacing);
    const QSignalBlocker blockerBackground(m_documentBackground);
    const QSignalBlocker blockerProvider(m_aiProvider);
    const QSignalBlocker blockerVoice(m_aiVoice);
    m_typedPreferencesLoaded = false;
    try
    {
    const auto snapshot = m_preferences.snapshot();
    m_theme->setCurrentIndex(findData(m_theme, static_cast<int>(snapshot.themePreference())));
    m_language->setCurrentIndex(findData(m_language, static_cast<int>(snapshot.languagePreference())));
    m_fontSize->setCurrentIndex(findData(m_fontSize, static_cast<int>(snapshot.fontSize())));
    m_autoUpdates->setChecked(snapshot.automaticUpdateChecksEnabled());
    m_sidebarTooltips->setChecked(snapshot.sidebarTooltipsEnabled());
    m_sidebarAnimation->setChecked(snapshot.sidebarMarqueeEnabled());
    m_excelTimeout->setCurrentIndex(findData(m_excelTimeout, snapshot.excelImportTimeoutSeconds()));
#ifdef Q_OS_MACOS
    m_powerPointNotice->setChecked(snapshot.showPowerPointDataAccessNotice());
#endif

    m_savedSaveMode = m_ports.saveMode.read();
    m_saveMode->setCurrentIndex(findData(m_saveMode, static_cast<int>(m_savedSaveMode)));
    m_savedDocumentSpacing = m_ports.documentSpacing.read();
    m_documentSpacing->setCurrentIndex(findData(
        m_documentSpacing, static_cast<int>(m_savedDocumentSpacing)));
    m_savedDocumentBackground = m_ports.documentBackground.read();
    m_documentBackground->setCurrentIndex(findData(
        m_documentBackground, static_cast<int>(m_savedDocumentBackground)));
    m_savedAiProvider = m_ports.aiProvider.read();
    m_savedAiVoice = m_ports.aiVoice.read();
    m_savedCustomWebsite = m_ports.customWebsite.read();
    m_aiProvider->setCurrentIndex(findData(m_aiProvider, static_cast<int>(m_savedAiProvider)));
    m_aiVoice->setCurrentIndex(findData(m_aiVoice, static_cast<int>(m_savedAiVoice)));
    updateCustomWebsiteText();
    m_typedPreferencesLoaded = true;
    }
    catch (const std::exception& exception)
    {
        showFailure("Unable to Load Preferences", {
            .code = Domain::ErrorCode::Technical,
            .message = exception.what(),
            .recoverable = true
        });
    }
    catch (...)
    {
        showFailure("Unable to Load Preferences", {
            .code = Domain::ErrorCode::Technical,
            .message = "The saved preference values could not be read.",
            .recoverable = true
        });
    }
    updateControlAvailability();
}

void PreferencesDialog::retranslateUi()
{
    setWindowTitle(text("Preferences"));
    m_tabs->setTabText(0, text("General"));
    m_tabs->setTabText(1, text("Appearance"));
    m_tabs->setTabText(2, text("Documents"));
    m_tabs->setTabText(3, text("AI Comments"));
    m_saveMode->setItemText(0, optionText("Automatic"));
    m_saveMode->setItemText(1, optionText("Manual"));
    m_autoUpdates->setText(optionText("Automatically Check for Updates"));
    m_autoUpdates->setToolTip(optionText(
        "Check GitHub Releases for a newer version when ClassMngr starts"));
    m_sidebarTooltips->setText(optionText("Show Sidebar Tooltips"));
    m_sidebarTooltips->setToolTip(optionText(
        "Show full sidebar names in tooltips when they do not fit"));
    m_sidebarAnimation->setText(optionText("Animate Overflowing Sidebar Text"));
    m_sidebarAnimation->setToolTip(optionText(
        "Animate overflowing sidebar names on hover"));
    const char* timeoutLabels[] = {"30 Seconds", "1 Minute", "2 Minutes", "5 Minutes"};
    for (int i = 0; i < 4; ++i)
    {
        m_excelTimeout->setItemText(i, text(timeoutLabels[i]));
    }
#ifdef Q_OS_MACOS
    m_powerPointNotice->setText(optionText("Show Data Access Notice Before Export"));
    m_powerPointNotice->setToolTip(optionText(
        "Show a notice before PowerPoint accesses its protected workspace"));
#endif
    m_theme->setItemText(0, optionText("System Default"));
    m_theme->setItemText(1, optionText("Light Theme"));
    m_theme->setItemText(2, optionText("Dark Theme"));
    m_language->setItemText(0, optionText("System Default"));
    m_language->setItemText(1, optionText("English"));
    m_language->setItemText(2, optionText("Korean"));
    m_fontSize->setItemText(0, optionText("Small"));
    m_fontSize->setItemText(1, optionText("Normal"));
    m_fontSize->setItemText(2, optionText("Large"));
    m_fontSize->setItemText(3, optionText("Extra Large"));
    for (int i = 0; i < 4; ++i)
    {
        m_documentSpacing->setItemText(i, optionText(
            i == 0 ? "None" : i == 1 ? "Small" : i == 2 ? "Medium" : "Large"));
    }
    m_documentBackground->setItemText(0, optionText("Default"));
    m_documentBackground->setItemText(1, optionText("White"));
    m_documentBackground->setItemText(2, optionText("Black"));
    m_aiProvider->setItemText(0, optionText("ChatGPT"));
    m_aiProvider->setItemText(1, optionText("Gemini"));
    m_aiProvider->setItemText(2, optionText("Claude"));
    m_aiProvider->setItemText(3, optionText("Microsoft Copilot"));
    m_aiProvider->setItemText(4, text("Custom Website…"));
    m_aiVoice->setItemText(0, optionText("Direct to Student"));
    m_aiVoice->setItemText(1, optionText("Third Person"));
    if (auto* const saveGroup = findChild<QGroupBox*>(QStringLiteral("preferencesSaveModeGroup")))
    {
        saveGroup->setTitle(text("Save Mode"));
    }
    if (auto* const updatesGroup = findChild<QGroupBox*>(QStringLiteral("preferencesUpdatesGroup")))
    {
        updatesGroup->setTitle(text("Updates"));
    }
    if (auto* const sidebarGroup = findChild<QGroupBox*>(QStringLiteral("preferencesSidebarGroup")))
    {
        sidebarGroup->setTitle(text("Sidebar"));
    }
    if (auto* const timeoutGroup = findChild<QGroupBox*>(QStringLiteral("preferencesExcelImportsGroup")))
    {
        timeoutGroup->setTitle(text("Excel Imports"));
    }
    auto* const saveForm = qobject_cast<QFormLayout*>(
        findChild<QGroupBox*>(QStringLiteral("preferencesSaveModeGroup"))->layout());
    setFormLabel(saveForm, m_saveMode, text("Mode:"));
    auto* const timeoutForm = qobject_cast<QFormLayout*>(
        findChild<QGroupBox*>(QStringLiteral("preferencesExcelImportsGroup"))->layout());
    setFormLabel(timeoutForm, m_excelTimeout, text("Workbook Timeout:"));
    auto* const appearanceForm = qobject_cast<QFormLayout*>(m_theme->parentWidget()->layout());
    if (appearanceForm)
    {
        setFormLabel(appearanceForm, m_theme, text("Theme:"));
        setFormLabel(appearanceForm, m_language, text("Language:"));
        setFormLabel(appearanceForm, m_fontSize, text("Font Size:"));
    }
    auto* const documentsForm = qobject_cast<QFormLayout*>(m_documentSpacing->parentWidget()->layout());
    if (documentsForm)
    {
        setFormLabel(documentsForm, m_documentSpacing, text("PDF Page Spacing:"));
        setFormLabel(documentsForm, m_documentBackground, text("Background Color:"));
    }
    auto* const aiForm = qobject_cast<QFormLayout*>(m_aiProvider->parentWidget()->layout());
    if (aiForm)
    {
        setFormLabel(aiForm, m_aiProvider, text("Preferred AI Website:"));
        setFormLabel(aiForm, m_customWebsite, QString{});
        setFormLabel(aiForm, m_aiVoice, text("Comment Voice:"));
    }
    if (auto* const buttons = findChild<QDialogButtonBox*>(QStringLiteral("preferencesButtons")))
    {
        if (QPushButton* closeButton = buttons->button(QDialogButtonBox::Close))
        {
            closeButton->setText(text("Close"));
        }
    }
    updateCustomWebsiteText();
    if (!m_failureFieldSource.isEmpty())
    {
        m_error->setText(text("%1: %2").arg(
            text(m_failureFieldSource.toUtf8().constData()), m_failureDetail));
        m_error->show();
    }
}

void PreferencesDialog::showFailure(
    const char* fieldSource,
    const Domain::OperationError& error
    )
{
    m_failureFieldSource = QString::fromUtf8(fieldSource);
    m_failureDetail = QString::fromUtf8(
        error.message.data(), static_cast<qsizetype>(error.message.size()));
    m_error->setText(text("%1: %2").arg(
        text(m_failureFieldSource.toUtf8().constData()), m_failureDetail));
    m_error->show();
}

void PreferencesDialog::clearFailure()
{
    m_failureFieldSource.clear();
    m_failureDetail.clear();
    m_error->hide();
}

Domain::Result<void> PreferencesDialog::saveApplicationPreference(
    const char* fieldSource,
    const std::function<Domain::Result<void>(Application::UserPreferencesState&)>& update
    )
{
    if (m_populating || !m_applicationPreferencesLoaded)
    {
        return Domain::Result<void>::failure(invalidPreference(
            "Application preferences could not be loaded."));
    }
    Application::UserPreferencesState candidate = m_preferences.state();
    const auto validated = update(candidate);
    if (!validated)
    {
        showFailure(fieldSource, validated.error());
        populateValues();
        return Domain::Result<void>::failure(validated.error());
    }
    try
    {
        const auto saved = m_preferences.applyAndPersist(candidate);
        if (!saved)
        {
            showFailure(fieldSource, saved.error());
            populateValues();
            return Domain::Result<void>::failure(saved.error());
        }
        clearFailure();
        return Domain::Result<void>::success();
    }
    catch (const std::exception& exception)
    {
        const bool committed = !m_preferences.isDirty()
            && m_preferences.state() == candidate;
        if (committed)
        {
            showFailure(fieldSource, {
                .code = Domain::ErrorCode::Technical,
                .message = std::string("The preference was saved, but a dependent UI update failed: ")
                    + exception.what(),
                .recoverable = true
            });
            populateValues();
            return Domain::Result<void>::success();
        }
        showFailure(fieldSource, {
            .code = Domain::ErrorCode::Technical,
            .message = exception.what(),
            .recoverable = true
        });
        populateValues();
        return Domain::Result<void>::failure({
            .code = Domain::ErrorCode::Technical,
            .message = exception.what(),
            .recoverable = true
        });
    }
    catch (...)
    {
        const bool committed = !m_preferences.isDirty()
            && m_preferences.state() == candidate;
        if (committed)
        {
            showFailure(fieldSource, {
                .code = Domain::ErrorCode::Technical,
                .message = "The preference was saved, but a dependent UI update failed.",
                .recoverable = true
            });
            populateValues();
            return Domain::Result<void>::success();
        }
        showFailure(fieldSource, {
            .code = Domain::ErrorCode::Technical,
            .message = "The preference could not be saved.",
            .recoverable = true
        });
        populateValues();
        return Domain::Result<void>::failure({
            .code = Domain::ErrorCode::Technical,
            .message = "The preference could not be saved.",
            .recoverable = true
        });
    }
}

void PreferencesDialog::changeTheme(const int index)
{
    if (m_populating || !m_applicationPreferencesLoaded) return;
    if (index < 0 || index >= m_theme->count())
    {
        showFailure("Theme", invalidPreference("Theme selection is invalid."));
        populateValues();
        return;
    }
    const auto value = static_cast<Application::ThemePreference>(m_theme->itemData(index).toInt());
    Domain::Result<void> result = Domain::Result<void>::success();
    try
    {
        if (m_actions.applyTheme) result = m_actions.applyTheme(value);
        else result = saveApplicationPreference("Theme", [value](auto& state) {
            return state.setThemePreference(value);
        });
    }
    catch (...) { result = Domain::Result<void>::failure(invalidPreference("The theme could not be applied.")); }
    if (!result)
    {
        showFailure("Theme", result.error());
        populateValues();
    }
    else clearFailure();
}

void PreferencesDialog::changeLanguage(const int index)
{
    if (m_populating || !m_applicationPreferencesLoaded) return;
    if (index < 0 || index >= m_language->count())
    {
        showFailure("Language", invalidPreference("Language selection is invalid."));
        populateValues();
        return;
    }
    const auto value = static_cast<Application::LanguagePreference>(m_language->itemData(index).toInt());
    Domain::Result<void> result = Domain::Result<void>::success();
    try
    {
        if (m_actions.applyLanguage) result = m_actions.applyLanguage(value);
        else result = saveApplicationPreference("Language", [value](auto& state) {
            return state.setLanguagePreference(value);
        });
    }
    catch (...) { result = Domain::Result<void>::failure(invalidPreference("The language could not be applied.")); }
    if (!result)
    {
        showFailure("Language", result.error());
        populateValues();
    }
    else clearFailure();
}

void PreferencesDialog::changeFontSize(const int index)
{
    if (m_populating || !m_applicationPreferencesLoaded) return;
    if (index < 0 || index >= m_fontSize->count())
    {
        showFailure("Font Size", invalidPreference("Font size selection is invalid."));
        populateValues();
        return;
    }
    const auto value = static_cast<Application::FontSize>(m_fontSize->itemData(index).toInt());
    Domain::Result<void> result = Domain::Result<void>::success();
    try
    {
        if (m_actions.applyFontSize) result = m_actions.applyFontSize(value);
        else result = saveApplicationPreference("Font Size", [value](auto& state) {
            return state.setFontSize(value);
        });
    }
    catch (...) { result = Domain::Result<void>::failure(invalidPreference("The font size could not be applied.")); }
    if (!result)
    {
        showFailure("Font Size", result.error());
        populateValues();
    }
    else clearFailure();
}

void PreferencesDialog::changeSaveMode(const int index)
{
    if (m_populating || !m_typedPreferencesLoaded) return;
    if (index < 0 || index >= m_saveMode->count())
    {
        showFailure("Save Mode", invalidPreference("Save Mode selection is invalid."));
        return;
    }
    const auto value = static_cast<Application::SaveMode>(m_saveMode->itemData(index).toInt());
    if (value != Application::SaveMode::Automatic && value != Application::SaveMode::Manual)
    {
        showFailure("Save Mode", invalidPreference("Save Mode must be Automatic or Manual."));
        const QSignalBlocker blocker(m_saveMode);
        m_saveMode->setCurrentIndex(findData(m_saveMode, static_cast<int>(m_savedSaveMode)));
        return;
    }
    const auto saved = m_ports.saveMode.write(value);
    if (!saved)
    {
        showFailure("Save Mode", saved.error());
        const QSignalBlocker blocker(m_saveMode);
        m_saveMode->setCurrentIndex(findData(m_saveMode, static_cast<int>(m_savedSaveMode)));
        return;
    }
    m_savedSaveMode = value;
    if (m_actions.saveModeChanged) m_actions.saveModeChanged(value);
    clearFailure();
}

void PreferencesDialog::changeDocumentSpacing(const int index)
{
    if (m_populating || !m_typedPreferencesLoaded) return;
    if (index < 0 || index >= m_documentSpacing->count())
    {
        showFailure("PDF Page Spacing", invalidPreference("PDF page spacing selection is invalid."));
        const QSignalBlocker blocker(m_documentSpacing);
        m_documentSpacing->setCurrentIndex(findData(m_documentSpacing, static_cast<int>(m_savedDocumentSpacing)));
        return;
    }
    const auto value = static_cast<Application::DocumentPageSpacing>(
        m_documentSpacing->itemData(index).toInt());
    const auto saved = m_ports.documentSpacing.write(value);
    if (!saved)
    {
        showFailure("PDF Page Spacing", saved.error());
        const QSignalBlocker blocker(m_documentSpacing);
        m_documentSpacing->setCurrentIndex(findData(m_documentSpacing, static_cast<int>(m_savedDocumentSpacing)));
        return;
    }
    m_savedDocumentSpacing = value;
    clearFailure();
}

void PreferencesDialog::changeDocumentBackground(const int index)
{
    if (m_populating || !m_typedPreferencesLoaded) return;
    if (index < 0 || index >= m_documentBackground->count())
    {
        showFailure("Background Color", invalidPreference("Background color selection is invalid."));
        const QSignalBlocker blocker(m_documentBackground);
        m_documentBackground->setCurrentIndex(findData(m_documentBackground, static_cast<int>(m_savedDocumentBackground)));
        return;
    }
    const auto value = static_cast<Application::DocumentViewerBackground>(
        m_documentBackground->itemData(index).toInt());
    const auto saved = m_ports.documentBackground.write(value);
    if (!saved)
    {
        showFailure("Background Color", saved.error());
        const QSignalBlocker blocker(m_documentBackground);
        m_documentBackground->setCurrentIndex(findData(m_documentBackground, static_cast<int>(m_savedDocumentBackground)));
        return;
    }
    m_savedDocumentBackground = value;
    clearFailure();
}

void PreferencesDialog::changeAiProvider(const int index)
{
    if (m_populating || !m_typedPreferencesLoaded) return;
    if (index < 0 || index >= m_aiProvider->count())
    {
        showFailure("Preferred AI Website", invalidPreference("AI provider selection is invalid."));
        const QSignalBlocker blocker(m_aiProvider);
        m_aiProvider->setCurrentIndex(findData(m_aiProvider, static_cast<int>(m_savedAiProvider)));
        return;
    }
    const auto value = static_cast<Application::AiCommentProvider>(
        m_aiProvider->itemData(index).toInt());
    if (value == m_savedAiProvider) return;
    std::string nextWebsite = m_savedCustomWebsite;
    if (value == Application::AiCommentProvider::CustomWebsite)
    {
        bool accepted = false;
        const QString entered = QInputDialog::getText(
            this, text("Custom AI Website"), text("HTTPS URL:"),
            QLineEdit::Normal,
            QString::fromUtf8(m_savedCustomWebsite.data(),
                              static_cast<qsizetype>(m_savedCustomWebsite.size())),
            &accepted).trimmed();
        if (!accepted)
        {
            const QSignalBlocker blocker(m_aiProvider);
            m_aiProvider->setCurrentIndex(findData(m_aiProvider, static_cast<int>(m_savedAiProvider)));
            return;
        }
        if (!isValidHttpsWebsite(entered))
        {
            showFailure("Preferred AI Website", invalidPreference(
                "Enter a valid HTTPS website URL."));
            const QSignalBlocker blocker(m_aiProvider);
            m_aiProvider->setCurrentIndex(findData(m_aiProvider, static_cast<int>(m_savedAiProvider)));
            return;
        }
        nextWebsite = entered.toUtf8().toStdString();
        const auto savedWebsite = m_ports.customWebsite.save(nextWebsite);
        if (!savedWebsite)
        {
            showFailure("Preferred AI Website", savedWebsite.error());
            const QSignalBlocker blocker(m_aiProvider);
            m_aiProvider->setCurrentIndex(findData(m_aiProvider, static_cast<int>(m_savedAiProvider)));
            return;
        }
    }

    const auto saved = m_ports.aiProvider.write(value);
    if (!saved)
    {
        if (value == Application::AiCommentProvider::CustomWebsite
            && nextWebsite != m_savedCustomWebsite)
        {
            const auto rollback = m_ports.customWebsite.save(m_savedCustomWebsite);
            if (!rollback)
            {
                showFailure("Preferred AI Website", {
                    .code = Domain::ErrorCode::Technical,
                    .message = saved.error().message + " Also, the previous custom website URL could not be restored: " + rollback.error().message,
                    .recoverable = true
                });
            }
        }
        if (m_failureFieldSource.isEmpty()) showFailure("Preferred AI Website", saved.error());
        const QSignalBlocker blocker(m_aiProvider);
        m_aiProvider->setCurrentIndex(findData(m_aiProvider, static_cast<int>(m_savedAiProvider)));
        updateCustomWebsiteText();
        return;
    }
    m_savedAiProvider = value;
    m_savedCustomWebsite = std::move(nextWebsite);
    updateCustomWebsiteText();
    clearFailure();
}

void PreferencesDialog::changeAiVoice(const int index)
{
    if (m_populating || !m_typedPreferencesLoaded) return;
    if (index < 0 || index >= m_aiVoice->count())
    {
        showFailure("Comment Voice", invalidPreference("Comment voice selection is invalid."));
        const QSignalBlocker blocker(m_aiVoice);
        m_aiVoice->setCurrentIndex(findData(m_aiVoice, static_cast<int>(m_savedAiVoice)));
        return;
    }
    const auto value = static_cast<Application::AiCommentVoice>(m_aiVoice->itemData(index).toInt());
    const auto saved = m_ports.aiVoice.write(value);
    if (!saved)
    {
        showFailure("Comment Voice", saved.error());
        const QSignalBlocker blocker(m_aiVoice);
        m_aiVoice->setCurrentIndex(findData(m_aiVoice, static_cast<int>(m_savedAiVoice)));
        return;
    }
    m_savedAiVoice = value;
    clearFailure();
}

void PreferencesDialog::updateCustomWebsiteText()
{
    const QString url = QString::fromUtf8(
        m_savedCustomWebsite.data(),
        static_cast<qsizetype>(m_savedCustomWebsite.size())).trimmed();
    m_customWebsite->setText(url.isEmpty()
        ? QString{}
        : text("Custom website: %1").arg(url));
    m_customWebsite->setVisible(!url.isEmpty());
}

void PreferencesDialog::updateControlAvailability()
{
    QWidget* const widgets[] = {
        m_autoUpdates,
        m_sidebarTooltips,
        m_sidebarAnimation,
        m_excelTimeout,
        m_theme,
        m_language,
        m_fontSize
    };
    for (QWidget* const widget : widgets)
    {
        if (widget) widget->setEnabled(m_applicationPreferencesLoaded);
    }
    QWidget* const typedWidgets[] = {
        m_saveMode,
        m_documentSpacing,
        m_documentBackground,
        m_aiProvider,
        m_aiVoice
    };
    for (QWidget* const widget : typedWidgets)
    {
        if (widget) widget->setEnabled(m_typedPreferencesLoaded);
    }
    m_customWebsite->setEnabled(m_typedPreferencesLoaded);
#ifdef Q_OS_MACOS
    if (m_powerPointNotice) m_powerPointNotice->setEnabled(m_applicationPreferencesLoaded);
#endif
}

}  // namespace ClassMngr::Next::Ui
