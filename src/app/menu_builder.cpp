#include "menu_builder.h"

#include "core/settingsmanager.h"
#include "mainwindow.h"
#include "ui/shared/actions/action_registry.h"

#include <QAction>
#include <QCheckBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QMenu>
#include <QMenuBar>
#include <QPushButton>
#include <QRadioButton>
#include <QSpinBox>
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
    layout->setSpacing(10);
    layout->setAlignment(Qt::AlignTop);
    return page;
}

QVBoxLayout* pageLayout(
    QWidget* page
    )
{
    return qobject_cast<QVBoxLayout*>(page->layout());
}

void showPreferencesDialog(
    MainWindow* window
    )
{
    auto& actions = window->actions();

    QDialog dialog(window);
    dialog.setObjectName(
        QStringLiteral("preferencesDialog")
        );
    dialog.setWindowTitle(
        preferencesText("Preferences")
        );
    dialog.resize(680, 560);

    auto* layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(18, 18, 18, 18);
    layout->setSpacing(10);

    auto* tabs = new QTabWidget(&dialog);
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
    auto* excelImportTimeout =
        new QSpinBox(excelImportGroup);
    excelImportTimeout->setObjectName(
        QStringLiteral("preferencesExcelImportTimeoutSeconds")
        );
    excelImportTimeout->setRange(1, 3600);
    excelImportTimeout->setSuffix(
        preferencesText(" seconds")
        );
    excelImportTimeout->setValue(
        SettingsManager::instance().excelImportTimeoutSeconds()
        );
    excelImportTimeout->setToolTip(
        preferencesText(
            "Stop loading a teacher or schedule workbook after this time."
            )
        );
    excelImportLayout->addRow(
        preferencesText("Workbook timeout:"),
        excelImportTimeout
        );
    QObject::connect(
        excelImportTimeout,
        QOverload<int>::of(&QSpinBox::valueChanged),
        [](int seconds)
        {
            SettingsManager::instance().setExcelImportTimeoutSeconds(
                seconds
                );
        }
        );
    generalLayout->addWidget(excelImportGroup);

#ifdef Q_OS_MACOS
    if (actions.showPowerPointDataAccessNotice)
    {
        auto* powerPointButton =
            new QPushButton(
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
        preferencesText("Page Spacing"),
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

    QWidget* aiCommentsPage =
        createPreferencesPage(
            tabs,
            "preferencesAiCommentsTab"
            );
    QVBoxLayout* aiCommentsLayout =
        pageLayout(aiCommentsPage);
    addActionChoices(
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

    auto* buttons =
        new QDialogButtonBox(
            QDialogButtonBox::Close,
            &dialog
            );
    QObject::connect(
        buttons,
        &QDialogButtonBox::rejected,
        &dialog,
        &QDialog::reject
        );
    layout->addWidget(buttons);

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
    buildHelpMenu(window);

    if (window->isAdmin())
    {
        buildAdminMenu(window);
    }
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
