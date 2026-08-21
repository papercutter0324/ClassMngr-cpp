#include "menu_builder.h"

#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "core/settingsmanager.h"
#include "features/calendar/ui/calendar_page.h"
#include "features/calendar/ui/calendar_preferences_panel.h"
#include "features/classes/class_navigation_preferences.h"
#include "features/schedule/schedule_settings_preferences.h"
#include "mainwindow.h"
#include "ui/shared/actions/action_registry.h"
#include "ui/shared/dialogs/dialog_shell.h"
#include "ui/shared/dialogs/user_prompt_service.h"
#include "ui/shared/widgets/text_fit_push_button.h"
#include "ui/shared/state/option_state_keys.h"

#include <QAction>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFrame>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QPushButton>
#include <QRadioButton>
#include <QScrollArea>
#include <QTabWidget>
#include <QVBoxLayout>

#include <initializer_list>

namespace
{

QString preferencesText(
    const char* text
    )
{
    return QCoreApplication::translate(
        "MenuBuilder",
        text
        );
}

QGroupBox* addActionChoices(
    QWidget* parent,
    QVBoxLayout* pageLayout,
    const QString& title,
    std::initializer_list<QAction*> actions
    )
{
    auto* group = new QGroupBox(title, parent);
    auto* layout = new QVBoxLayout(group);
    layout->setSpacing(12);

    for (QAction* action : actions)
    {
        if (!action)
        {
            continue;
        }

        auto* button =
            new QRadioButton(action->text(), group);
        button->setChecked(action->isChecked());
        button->setEnabled(action->isEnabled());

        QObject::connect(
            button,
            &QRadioButton::toggled,
            action,
            [action](bool checked)
            {
                if (checked && !action->isChecked())
                {
                    action->trigger();
                }
            }
            );
        QObject::connect(
            action,
            &QAction::changed,
            button,
            [action, button]()
            {
                button->setChecked(action->isChecked());
                button->setEnabled(action->isEnabled());
                button->setText(action->text());
            }
            );

        layout->addWidget(button);
    }

    pageLayout->addWidget(group);
    return group;
}

QCheckBox* addActionCheckBox(
    QWidget* parent,
    QVBoxLayout* pageLayout,
    QAction* action
    )
{
    if (!action)
    {
        return nullptr;
    }

    auto* checkBox =
        new QCheckBox(action->text(), parent);
    checkBox->setChecked(action->isChecked());
    checkBox->setEnabled(action->isEnabled());

    QObject::connect(
        checkBox,
        &QCheckBox::toggled,
        action,
        [action](bool checked)
        {
            if (checked != action->isChecked())
            {
                action->trigger();
            }
        }
        );
    QObject::connect(
        action,
        &QAction::changed,
        checkBox,
        [action, checkBox]()
        {
            checkBox->setChecked(action->isChecked());
            checkBox->setEnabled(action->isEnabled());
            checkBox->setText(action->text());
        }
        );

    pageLayout->addWidget(checkBox);
    return checkBox;
}

QWidget* createPreferencesPage(
    QTabWidget* tabs,
    const char* objectName
    )
{
    auto* page = new QWidget(tabs);
    page->setObjectName(
        QString::fromLatin1(objectName)
        );

    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(24);
    layout->setAlignment(Qt::AlignTop);
    return page;
}

QScrollArea* createScrollablePreferencesPage(
    QTabWidget* tabs,
    const char* objectName,
    QWidget** pageOut
    )
{
    auto* scrollArea = new QScrollArea(tabs);
    scrollArea->setObjectName(QString::fromLatin1(objectName));
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);

    auto* page = new QWidget(scrollArea);
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(24);
    layout->setAlignment(Qt::AlignTop);
    scrollArea->setWidget(page);
    *pageOut = page;
    return scrollArea;
}

QVBoxLayout* pageLayout(
    QWidget* page
    )
{
    return qobject_cast<QVBoxLayout*>(page->layout());
}

void clearTestingLayout(MainWindow* window)
{
    auto* scheduleService =
        window && window->services()
            ? window->services()->scheduleService()
            : nullptr;

    if (!scheduleService || !scheduleService->isAvailable())
    {
        DialogServices::showWarning(
            window,
            preferencesText("Clear Testing Layout"),
            preferencesText("No Teacher Profile is open.")
            );
        return;
    }

    const PromptChoice answer =
        DialogServices::confirm(
            window,
            preferencesText("Clear Testing Layout?"),
            preferencesText("This removes every Oral Testing block and testing-class assignment. Saved testing classes and their rosters are preserved."),
            preferencesText("Clear Layout"),
            preferencesText("Cancel"),
            true
            );

    if (answer != PromptChoice::Destructive)
    {
        return;
    }

    const Status result = scheduleService->clearTestingAssignments();
    if (!result)
    {
        DialogServices::showWarning(
            window,
            preferencesText("Clear Testing Layout"),
            result.error()
            );
        return;
    }

    window->refreshSchedulePreferences();
}

void addSchedulePreferencesTab(
    QTabWidget* tabs,
    MainWindow* window
    )
{
    QWidget* page = createPreferencesPage(tabs, "preferencesScheduleTab");
    QVBoxLayout* layout = pageLayout(page);
    auto* settingsService =
        window && window->services()
            ? window->services()->settingsService()
            : nullptr;
    const ScheduleSettingsValues values =
        ScheduleSettingsPreferences::load(settingsService);

    auto* displayGroup = new QGroupBox(preferencesText("Display"), page);
    auto* displayLayout = new QVBoxLayout(displayGroup);
    displayLayout->setSpacing(12);
    auto* showEnglishNames = new QCheckBox(
        preferencesText("Show English Names"),
        displayGroup
        );
    showEnglishNames->setObjectName(
        QStringLiteral("preferencesScheduleShowEnglishNames")
        );
    showEnglishNames->setChecked(values.showEnglishNames);
    displayLayout->addWidget(showEnglishNames);
    auto* use24HourTime = new QCheckBox(
        preferencesText("Use 24-Hour Time"),
        displayGroup
        );
    use24HourTime->setObjectName(
        QStringLiteral("preferencesScheduleUse24HourTime")
        );
    use24HourTime->setChecked(values.use24HourTime);
    displayLayout->addWidget(use24HourTime);
    auto* showWeekends = new QCheckBox(
        preferencesText("Show Weekends"),
        displayGroup
        );
    showWeekends->setObjectName(
        QStringLiteral("preferencesScheduleShowWeekends")
        );
    showWeekends->setChecked(values.showWeekends);
    displayLayout->addWidget(showWeekends);
    layout->addWidget(displayGroup);

    auto* intensiveGroup = new QGroupBox(preferencesText("Intensive"), page);
    auto* intensiveLayout = new QVBoxLayout(intensiveGroup);
    intensiveLayout->setSpacing(12);
    auto* showAllHours = new QCheckBox(
        preferencesText("Show All Hours"),
        intensiveGroup
        );
    showAllHours->setObjectName(
        QStringLiteral("preferencesScheduleShowAllIntensiveHours")
        );
    showAllHours->setChecked(values.showAllIntensiveHours);
    intensiveLayout->addWidget(showAllHours);
    auto* intensiveExplanation = new QLabel(
        preferencesText("When disabled, empty hours at the beginning and end of an intensive schedule are hidden."),
        intensiveGroup
        );
    intensiveExplanation->setWordWrap(true);
    intensiveLayout->addWidget(intensiveExplanation);
    layout->addWidget(intensiveGroup);

    auto* testingGroup = new QGroupBox(preferencesText("Testing"), page);
    auto* testingLayout = new QVBoxLayout(testingGroup);
    testingLayout->setSpacing(12);
    auto* testingExplanation = new QLabel(
        preferencesText("M2 and M3 classes are always hidden in Testing mode. Oral Testing blocks and testing-class assignments are saved as one reusable weekly layout."),
        testingGroup
        );
    testingExplanation->setWordWrap(true);
    testingLayout->addWidget(testingExplanation);
    auto* testingAffectsM1 = new QCheckBox(
        preferencesText("Testing also affects M1"),
        testingGroup
        );
    testingAffectsM1->setObjectName(
        QStringLiteral("preferencesScheduleTestingAffectsM1")
        );
    testingAffectsM1->setChecked(values.testingAffectsM1);
    testingLayout->addWidget(testingAffectsM1);
    auto* clearButton = new TextFitPushButton(
        preferencesText("Clear Testing Layout"),
        testingGroup
        );
    clearButton->setObjectName(
        QStringLiteral("preferencesScheduleClearTestingLayout")
        );
    testingLayout->addWidget(clearButton, 0, Qt::AlignLeft);
    layout->addWidget(testingGroup);

    const auto save = [
        window,
        settingsService,
        use24HourTime,
        showEnglishNames,
        showWeekends,
        showAllHours,
        testingAffectsM1
        ]()
    {
        ScheduleSettingsPreferences::save(
            settingsService,
            {
                use24HourTime->isChecked(),
                showEnglishNames->isChecked(),
                showWeekends->isChecked(),
                showAllHours->isChecked(),
                testingAffectsM1->isChecked()
            }
            );
        window->refreshSchedulePreferences();
    };
    for (QCheckBox* checkBox : {
             showEnglishNames,
             use24HourTime,
             showWeekends,
             showAllHours,
             testingAffectsM1
         })
    {
        QObject::connect(checkBox, &QCheckBox::toggled, page, save);
    }
    QObject::connect(
        clearButton,
        &QPushButton::clicked,
        page,
        [window]()
        {
            clearTestingLayout(window);
        }
        );

    tabs->addTab(page, preferencesText("Schedule"));
}

void addCalendarPreferencesTab(
    QTabWidget* tabs,
    MainWindow* window
    )
{
    QWidget* page = nullptr;
    QScrollArea* scrollArea = createScrollablePreferencesPage(
        tabs,
        "preferencesCalendarTab",
        &page
        );
    auto* calendarPage = window ? window->calendarPage() : nullptr;
    auto* panel = new CalendarPreferencesPanel(
        calendarPage ? calendarPage->academicCalendarProvider() : nullptr,
        window && window->services()
            ? window->services()->calendarService()
            : nullptr,
        window && window->services()
            ? window->services()->settingsService()
            : nullptr,
        page
        );
    pageLayout(page)->addWidget(panel);

    if (calendarPage)
    {
        QObject::connect(
            panel,
            &CalendarPreferencesPanel::calendarPreferencesChanged,
            calendarPage,
            &CalendarPage::calendarPreferencesChanged
            );
    }

    tabs->addTab(scrollArea, preferencesText("Calendar"));
}

void addNavigationPreferencesTab(
    QTabWidget* tabs,
    MainWindow* window
    )
{
    QWidget* page = createPreferencesPage(tabs, "preferencesNavigationTab");
    QVBoxLayout* layout = pageLayout(page);
    auto* navigationGroup = new QGroupBox(preferencesText("Display"), page);
    auto* navigationLayout = new QVBoxLayout(navigationGroup);
    navigationLayout->setSpacing(12);
    auto* classesShownGroup = new QGroupBox(
        preferencesText("Classes Shown"),
        navigationGroup
        );
    auto* classesShownLayout = new QVBoxLayout(classesShownGroup);
    classesShownLayout->setSpacing(12);
    const ClassTabNavigation::VisibilityScope scope =
        ClassNavigationPreferences::load(
            window && window->services()
                ? window->services()->settingsService()
                : nullptr
            );
    auto* allClasses = new QRadioButton(
        preferencesText("All Classes"),
        classesShownGroup
        );
    allClasses->setObjectName(QStringLiteral("preferencesNavigationAllClasses"));
    allClasses->setChecked(
        scope == ClassTabNavigation::VisibilityScope::AllClasses
        );
    classesShownLayout->addWidget(allClasses);
    auto* activeSchedule = new QRadioButton(
        preferencesText("Active Schedule"),
        classesShownGroup
        );
    activeSchedule->setObjectName(
        QStringLiteral("preferencesNavigationActiveSchedule")
        );
    activeSchedule->setChecked(
        scope == ClassTabNavigation::VisibilityScope::ActiveSchedule
        );
    classesShownLayout->addWidget(activeSchedule);
    navigationLayout->addWidget(classesShownGroup);
    layout->addWidget(navigationGroup);

    const auto save = [window, allClasses](bool checked)
    {
        if (!checked)
        {
            return;
        }

        ClassNavigationPreferences::save(
            window && window->services()
                ? window->services()->settingsService()
                : nullptr,
            allClasses->isChecked()
                ? ClassTabNavigation::VisibilityScope::AllClasses
                : ClassTabNavigation::VisibilityScope::ActiveSchedule
            );
        window->refreshNavigationPreferences();
    };
    QObject::connect(allClasses, &QRadioButton::toggled, page, save);
    QObject::connect(activeSchedule, &QRadioButton::toggled, page, save);

    tabs->addTab(page, preferencesText("Navigation Bar"));
}

void populatePreferencesDialog(
    DialogShell* dialog,
    MainWindow* window
    );

class PreferencesDialog final : public DialogShell
{
public:
    explicit PreferencesDialog(MainWindow* window)
        : DialogShell(QStringLiteral("preferences"), window)
    {
        populatePreferencesDialog(this, window);
    }
};

void populatePreferencesDialog(
    DialogShell* dialog,
    MainWindow* window
    )
{
    auto& actions = window->actions();

    dialog->setObjectName(
        QStringLiteral("preferencesDialog")
        );
    dialog->setWindowTitle(
        preferencesText("Preferences")
        );
    dialog->resize(680, 560);

    auto* layout = dialog->contentLayout();

    auto* tabs = new QTabWidget(dialog);
    tabs->setObjectName(
        QStringLiteral("preferencesTabs")
        );
    layout->addWidget(tabs, 1);

    QWidget* generalPage =
        createPreferencesPage(
            tabs,
            "preferencesGeneralTab"
            );
    QVBoxLayout* generalLayout =
        pageLayout(generalPage);
    addActionChoices(
        generalPage,
        generalLayout,
        preferencesText("Save Mode"),
        {
            actions.saveModeState
                ? actions.saveModeState->action(
                    SaveMode::Automatic
                    )
                : nullptr,
            actions.saveModeState
                ? actions.saveModeState->action(
                    SaveMode::Manual
                    )
                : nullptr
        }
        );

    auto* updatesGroup =
        new QGroupBox(
            preferencesText("Updates"),
            generalPage
            );
    auto* updatesLayout =
        new QVBoxLayout(updatesGroup);
    updatesLayout->setSpacing(12);
    addActionCheckBox(
        updatesGroup,
        updatesLayout,
        actions.automaticallyCheckForUpdates
        );
    generalLayout->addWidget(updatesGroup);

    auto* sidebarGroup =
        new QGroupBox(
            preferencesText("Sidebar"),
            generalPage
            );
    auto* sidebarLayout =
        new QVBoxLayout(sidebarGroup);
    sidebarLayout->setSpacing(12);
    addActionCheckBox(
        sidebarGroup,
        sidebarLayout,
        actions.showSidebarTooltips
        );
    addActionCheckBox(
        sidebarGroup,
        sidebarLayout,
        actions.animateSidebarText
        );
    generalLayout->addWidget(sidebarGroup);

    auto* excelImportGroup =
        new QGroupBox(
            preferencesText("Excel Imports"),
            generalPage
            );
    auto* excelImportLayout =
        new QFormLayout(excelImportGroup);
    excelImportLayout->setFormAlignment(
        Qt::AlignLeft | Qt::AlignTop
        );
    excelImportLayout->setLabelAlignment(Qt::AlignLeft);
    auto* excelImportTimeout =
        new QComboBox(excelImportGroup);
    excelImportTimeout->setObjectName(
        QStringLiteral("preferencesExcelImportTimeout")
        );
    excelImportTimeout->addItem(
        preferencesText("30 Seconds"),
        30
        );
    excelImportTimeout->addItem(
        preferencesText("1 Minute"), 60);
    excelImportTimeout->addItem(
        preferencesText("2 Minutes"), 120);
    excelImportTimeout->addItem(
        preferencesText("5 Minutes"), 300);
    excelImportTimeout->setCurrentIndex(
        excelImportTimeout->findData(
            SettingsManager::instance().excelImportTimeoutSeconds()
            )
        );
    excelImportTimeout->setToolTip(
        preferencesText(
            "Stop loading a teacher or schedule workbook after this time."
            )
        );
    excelImportLayout->addRow(
        preferencesText("Workbook Timeout:"),
        excelImportTimeout
        );
    excelImportLayout->setAlignment(
        excelImportTimeout,
        Qt::AlignLeft
        );
    QObject::connect(
        excelImportTimeout,
        QOverload<int>::of(&QComboBox::currentIndexChanged),
        [excelImportTimeout](int index)
        {
            SettingsManager::instance().setExcelImportTimeoutSeconds(
                excelImportTimeout->itemData(index).toInt()
                );
        }
        );
    generalLayout->addWidget(excelImportGroup);

#ifdef Q_OS_MACOS
    if (actions.showPowerPointDataAccessNotice)
    {
        auto* powerPointButton =
            new TextFitPushButton(
                actions.showPowerPointDataAccessNotice->text(),
                generalPage
                );
        QObject::connect(
            powerPointButton,
            &QPushButton::clicked,
            actions.showPowerPointDataAccessNotice,
            &QAction::trigger
            );
        generalLayout->addWidget(powerPointButton);
    }
#endif

    tabs->addTab(
        generalPage,
        preferencesText("General")
        );

    QWidget* appearancePage =
        createPreferencesPage(
            tabs,
            "preferencesAppearanceTab"
            );
    QVBoxLayout* appearanceLayout =
        pageLayout(appearancePage);
    addActionChoices(
        appearancePage,
        appearanceLayout,
        preferencesText("Theme"),
        {
            actions.themeState
                ? actions.themeState->action(
                    Theme::SystemDefault
                    )
                : nullptr,
            actions.themeState
                ? actions.themeState->action(Theme::Dark)
                : nullptr,
            actions.themeState
                ? actions.themeState->action(Theme::Light)
                : nullptr
        }
        );
    addActionChoices(
        appearancePage,
        appearanceLayout,
        preferencesText("Language"),
        {
            actions.languageState
                ? actions.languageState->action(
                    Language::SystemDefault
                    )
                : nullptr,
            actions.languageState
                ? actions.languageState->action(
                    Language::English
                    )
                : nullptr,
            actions.languageState
                ? actions.languageState->action(
                    Language::Korean
                    )
                : nullptr
        }
        );
    addActionChoices(
        appearancePage,
        appearanceLayout,
        preferencesText("Font Size"),
        {
            actions.fontSizeState
                ? actions.fontSizeState->action(
                    FontSize::Small
                    )
                : nullptr,
            actions.fontSizeState
                ? actions.fontSizeState->action(
                    FontSize::Normal
                    )
                : nullptr,
            actions.fontSizeState
                ? actions.fontSizeState->action(
                    FontSize::Large
                    )
                : nullptr,
            actions.fontSizeState
                ? actions.fontSizeState->action(
                    FontSize::ExtraLarge
                    )
                : nullptr
        }
        );
    tabs->addTab(
        appearancePage,
        preferencesText("Appearance")
        );

    QWidget* documentsPage =
        createPreferencesPage(
            tabs,
            "preferencesDocumentsTab"
            );
    QVBoxLayout* documentsLayout =
        pageLayout(documentsPage);
    addActionChoices(
        documentsPage,
        documentsLayout,
        preferencesText("PDF Page Spacing"),
        {
            actions.documentPageSpacingState
                ? actions.documentPageSpacingState->action(
                    DocumentPageSpacing::None
                    )
                : nullptr,
            actions.documentPageSpacingState
                ? actions.documentPageSpacingState->action(
                    DocumentPageSpacing::Small
                    )
                : nullptr,
            actions.documentPageSpacingState
                ? actions.documentPageSpacingState->action(
                    DocumentPageSpacing::Medium
                    )
                : nullptr,
            actions.documentPageSpacingState
                ? actions.documentPageSpacingState->action(
                    DocumentPageSpacing::Large
                    )
                : nullptr
        }
        );
    addActionChoices(
        documentsPage,
        documentsLayout,
        preferencesText("Background Color"),
        {
            actions.documentViewerBackgroundState
                ? actions.documentViewerBackgroundState->action(
                    DocumentViewerBackground::Default
                    )
                : nullptr,
            actions.documentViewerBackgroundState
                ? actions.documentViewerBackgroundState->action(
                    DocumentViewerBackground::White
                    )
                : nullptr,
            actions.documentViewerBackgroundState
                ? actions.documentViewerBackgroundState->action(
                    DocumentViewerBackground::Black
                    )
                : nullptr
        }
        );
    tabs->addTab(
        documentsPage,
        preferencesText("Documents")
        );

    addSchedulePreferencesTab(tabs, window);
    addCalendarPreferencesTab(tabs, window);
    addNavigationPreferencesTab(tabs, window);

    QWidget* aiCommentsPage =
        createPreferencesPage(
            tabs,
            "preferencesAiCommentsTab"
            );
    QVBoxLayout* aiCommentsLayout =
        pageLayout(aiCommentsPage);
    QGroupBox* preferredAiWebsiteGroup = addActionChoices(
        aiCommentsPage,
        aiCommentsLayout,
        preferencesText("Preferred AI Website"),
        {
            actions.aiCommentProviderState
                ? actions.aiCommentProviderState->action(
                    AiCommentProvider::ChatGPT
                    )
                : nullptr,
            actions.aiCommentProviderState
                ? actions.aiCommentProviderState->action(
                    AiCommentProvider::Gemini
                    )
                : nullptr,
            actions.aiCommentProviderState
                ? actions.aiCommentProviderState->action(
                    AiCommentProvider::Claude
                    )
                : nullptr,
            actions.aiCommentProviderState
                ? actions.aiCommentProviderState->action(
                    AiCommentProvider::MicrosoftCopilot
                    )
                : nullptr,
            actions.aiCommentProviderState
                ? actions.aiCommentProviderState->action(
                    AiCommentProvider::CustomWebsite
                    )
                : nullptr
        }
        );
    auto* customWebsiteUrl =
        new QLabel(preferredAiWebsiteGroup);
    customWebsiteUrl->setObjectName(
        QStringLiteral("preferencesCustomAiWebsiteUrl")
        );
    customWebsiteUrl->setTextInteractionFlags(
        Qt::TextSelectableByMouse
        );
    customWebsiteUrl->setWordWrap(true);
    const auto updateCustomWebsiteUrl =
        [customWebsiteUrl]()
        {
            const QString url =
                SettingsManager::instance()
                    .get(
                        QString::fromUtf8(
                            OptionKeys::AiCommentCustomWebsiteUrl
                            )
                        )
                    .toString()
                    .trimmed();
            customWebsiteUrl->setText(
                url.isEmpty()
                    ? QString()
                    : preferencesText("Custom website: %1").arg(url)
                );
            customWebsiteUrl->setVisible(!url.isEmpty());
        };
    updateCustomWebsiteUrl();
    if (actions.aiCommentProviderState)
    {
        QObject::connect(
            actions.aiCommentProviderState->action(
                AiCommentProvider::CustomWebsite
                ),
            &QAction::triggered,
            customWebsiteUrl,
            updateCustomWebsiteUrl
            );
    }
    preferredAiWebsiteGroup->layout()->addWidget(customWebsiteUrl);
    addActionChoices(
        aiCommentsPage,
        aiCommentsLayout,
        preferencesText("Comment Voice"),
        {
            actions.aiCommentVoiceState
                ? actions.aiCommentVoiceState->action(
                    AiCommentVoice::DirectToStudent
                    )
                : nullptr,
            actions.aiCommentVoiceState
                ? actions.aiCommentVoiceState->action(
                    AiCommentVoice::ThirdPerson
                    )
                : nullptr
        }
        );
    tabs->addTab(
        aiCommentsPage,
        preferencesText("AI Comments")
        );

    dialog->addButtonBox(QDialogButtonBox::Close);
}

void showPreferencesDialog(
    MainWindow* window
    )
{
    PreferencesDialog dialog(window);
    dialog.exec();
}

} // namespace

// Missing safety: menu duplication risk
// Option: Store menus in MainWindow
//  QMenu* fileMenu;
//  QMenu* editMenu;
//  ...

void MenuBuilder::build(MainWindow* window)
{
    buildFileMenu(window);
    buildEditMenu(window);
    buildClassMenu(window);
    buildPrintExportMenu(window);
    buildHelpMenu(window);

    if (window->isAdmin())
    {
        buildAdminMenu(window);
    }
}

void MenuBuilder::buildPrintExportMenu(
    MainWindow* window
    )
{
    auto& actions = window->actions();

    actions.printExportMenu =
        window->menuBar()->addMenu(
            QCoreApplication::translate(
                "MenuBuilder",
                "Print / Export"
                )
            );

    actions.printExportMenu->addAction(
        actions.printCurrentPage
        );
    actions.printExportMenu->addAction(
        actions.saveCurrentPageAs
        );
    actions.printExportMenu->setEnabled(false);
}

void MenuBuilder::buildFileMenu(MainWindow* window)
{
    auto& actions = window->actions();

    QMenu* fileMenu =
        window->menuBar()->addMenu(
        QCoreApplication::translate("MenuBuilder", "File")
    );

    fileMenu->addAction(actions.newFile);
    fileMenu->addAction(actions.openFile);

    actions.recentFilesMenu =
        fileMenu->addMenu(
            QCoreApplication::translate("MenuBuilder", "Recent Files")
            );

    fileMenu->addSeparator();

    fileMenu->addAction(actions.saveFile);
    fileMenu->addAction(actions.saveAsFile);
    fileMenu->addAction(actions.exportAsFile);

    fileMenu->addSeparator();

    fileMenu->addAction(actions.closeFile);

    fileMenu->addSeparator();

    fileMenu->addAction(actions.exitApp);
}

void MenuBuilder::buildEditMenu(MainWindow* window)
{
    auto& a = window->actions();

    QMenu* menu =
        window->menuBar()->addMenu(
        QCoreApplication::translate("MenuBuilder", "Edit")
    );

    menu->addAction(a.undo);
    menu->addAction(a.redo);

    menu->addSeparator();

    menu->addAction(a.cut);
    menu->addAction(a.copy);
    menu->addAction(a.paste);

    menu->addSeparator();

    auto* preferencesAction =
        menu->addAction(
            QCoreApplication::translate(
                "MenuBuilder",
                "Preferences..."
                )
            );
    preferencesAction->setObjectName(
        QStringLiteral("preferencesAction")
        );
    QObject::connect(
        preferencesAction,
        &QAction::triggered,
        window,
        [window]()
        {
            showPreferencesDialog(window);
        }
        );
}

void MenuBuilder::buildClassMenu(MainWindow* window)
{
    auto& a = window->actions();

    // Classes menu (top-level)
    QMenu* classMenu =
        window->menuBar()->addMenu(
            QCoreApplication::translate("MenuBuilder", "Classes")
            );

    classMenu->addAction(a.newClass);
    classMenu->addAction(a.deleteClass);

    classMenu->addSeparator();

    classMenu->addAction(a.importClasses);
    classMenu->addAction(a.exportClasses);

    // Teachers menu (top-level)
    QMenu* teacherMenu =
        window->menuBar()->addMenu(
            QCoreApplication::translate("MenuBuilder", "Teachers")
            );

    teacherMenu->addAction(a.newTeacher);
    teacherMenu->addAction(a.deleteTeacher);
    teacherMenu->addSeparator();
    teacherMenu->addAction(a.importTeachers);
}

void MenuBuilder::buildHelpMenu(MainWindow* window)
{
    auto& a = window->actions();

    QMenu* menu =
        window->menuBar()->addMenu(
        QCoreApplication::translate("MenuBuilder", "Help")
    );

    menu->addAction(a.checkForUpdates);
    menu->addSeparator();
    menu->addAction(a.about);
}

void MenuBuilder::buildAdminMenu(MainWindow* window)
{
    auto& a = window->actions();

    QMenu* menu =
        window->menuBar()->addMenu(
        QCoreApplication::translate("MenuBuilder", "Admin")
    );

    menu->addAction(a.manageCampuses);
}
