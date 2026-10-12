#pragma once

#include <QObject>
#include <QAction>

#include "ui/shared/state/option_state.h"
#include "ui/shared/constants/options.h"

#include <functional>
#include <memory>

class QMenu;

namespace ClassMngr::Next::Application
{
class AiCommentVoicePreferencesPort;
class AutomaticUpdatePreferencesPort;
class FontSizePreferencesPort;
class SaveModePreferencesPort;
}

namespace ClassMngr::Next::Domain
{
struct OperationError;
}

class ActionRegistry : public QObject
{
    Q_OBJECT

public:
    explicit ActionRegistry(QObject* parent = nullptr);

    void createActions(
        std::shared_ptr<
            ClassMngr::Next::Application::SaveModePreferencesPort
            > saveModePreferencesPort = {},
        std::function<void(
            const ClassMngr::Next::Domain::OperationError&
            )> saveModePreferenceErrorHandler = {},
        std::shared_ptr<
            ClassMngr::Next::Application::AutomaticUpdatePreferencesPort
            > automaticUpdatePreferencesPort = {},
        std::function<void(
            const ClassMngr::Next::Domain::OperationError&
            )> automaticUpdatePreferenceErrorHandler = {},
        std::shared_ptr<
            ClassMngr::Next::Application::FontSizePreferencesPort
            > fontSizePreferencesPort = {},
        std::function<void(
            const ClassMngr::Next::Domain::OperationError&
            )> fontSizePreferenceErrorHandler = {},
        std::shared_ptr<
            ClassMngr::Next::Application::AiCommentVoicePreferencesPort
            > aiCommentVoicePreferencesPort = {},
        std::function<void(
            const ClassMngr::Next::Domain::OperationError&
            )> aiCommentVoicePreferenceErrorHandler = {}
        );
    void retranslate();
    void refreshThemedIcons();

    // =====================================================
    // Option States (NEW SYSTEM)
    // =====================================================

    OptionState<SaveMode>* saveModeState = nullptr;
    OptionState<Theme>* themeState = nullptr;
    OptionState<Language>* languageState = nullptr;
    OptionState<FontSize>* fontSizeState = nullptr;
    OptionState<DocumentPageSpacing>* documentPageSpacingState = nullptr;
    OptionState<DocumentViewerBackground>* documentViewerBackgroundState = nullptr;
    OptionState<AiCommentProvider>* aiCommentProviderState = nullptr;
    OptionState<AiCommentVoice>* aiCommentVoiceState = nullptr;

    QAction* showSidebarTooltips = nullptr;
    QAction* animateSidebarText = nullptr;
    QAction* automaticallyCheckForUpdates = nullptr;
#ifdef Q_OS_MACOS
    QAction* showPowerPointDataAccessNotice = nullptr;
#endif

    // =====================================================
    // File
    // =====================================================

    QAction* newFile = nullptr;
    QAction* openFile = nullptr;
    QAction* saveFile = nullptr;
    QAction* saveAsFile = nullptr;
    QAction* exportAsFile = nullptr;
    QAction* closeFile = nullptr;
    QAction* exitApp = nullptr;
    QMenu* recentFilesMenu = nullptr;

    // =====================================================
    // Print / Export
    // =====================================================

    QAction* printCurrentPage = nullptr;
    QAction* saveCurrentPageAs = nullptr;
    QMenu* printExportMenu = nullptr;

    // =====================================================
    // Edit
    // =====================================================

    QAction* undo = nullptr;
    QAction* redo = nullptr;
    QAction* cut = nullptr;
    QAction* copy = nullptr;
    QAction* paste = nullptr;

    // =====================================================
    // Class
    // =====================================================

    QAction* newClass = nullptr;
    QAction* deleteClass = nullptr;
    QAction* importClasses = nullptr;
    QAction* exportClasses = nullptr;
    QAction* newTeacher = nullptr;
    QAction* deleteTeacher = nullptr;
    QAction* upcomingBirthdays = nullptr;
    QAction* importTeachers = nullptr;

    // =====================================================
    // Help
    // =====================================================

    QAction* checkForUpdates = nullptr;
    QAction* about = nullptr;

    // =====================================================
    // Admin
    // =====================================================

    QAction* manageCampuses = nullptr;

private:
    QAction* createAction(
        const QString& text,
        const QString& statusTip);

    QAction* createAction(
        const QIcon& icon,
        const QString& text,
        const QString& statusTip);

    QAction* createCheckableAction(
        const QString& text,
        const QString& statusTip = QString()
        );

    void createFileActions();
    void createEditActions();
    void createPrintExportActions();
    void createClassActions();
    void createOptionActions(
        std::shared_ptr<
            ClassMngr::Next::Application::SaveModePreferencesPort
            > saveModePreferencesPort,
        const std::function<void(
            const ClassMngr::Next::Domain::OperationError&
            )>& saveModePreferenceErrorHandler,
        std::shared_ptr<
            ClassMngr::Next::Application::AutomaticUpdatePreferencesPort
            > automaticUpdatePreferencesPort,
        const std::function<void(
            const ClassMngr::Next::Domain::OperationError&
            )>& automaticUpdatePreferenceErrorHandler,
        std::shared_ptr<
            ClassMngr::Next::Application::FontSizePreferencesPort
            > fontSizePreferencesPort,
        const std::function<void(
            const ClassMngr::Next::Domain::OperationError&
            )>& fontSizePreferenceErrorHandler,
        std::shared_ptr<
            ClassMngr::Next::Application::AiCommentVoicePreferencesPort
            > aiCommentVoicePreferencesPort,
        const std::function<void(
            const ClassMngr::Next::Domain::OperationError&
            )>& aiCommentVoicePreferenceErrorHandler
        );
    void createHelpActions();
    void createAdminActions();
};
