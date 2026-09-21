#include "app/controllers/language_controller.h"
#include "core/fontmanager.h"
#include "core/language_service.h"
#include "core/settingsmanager.h"
#include "next/platform/language_preference_port.h"
#include "ui/shared/actions/action_registry.h"
#include "ui/shared/constants/options.h"
#include "ui/shared/state/option_state_keys.h"

#include <QApplication>
#include <QFile>
#include <QtTest>

using namespace ClassMngr::Next;
using namespace ClassMngr::Next::Application;
using namespace ClassMngr::Next::Domain;
using namespace ClassMngr::Next::Platform;

namespace
{

QString languageSettingsKey()
{
    return QString::fromUtf8(OptionKeys::Language);
}

void restoreSetting(
    SettingsManager& settings,
    const QString& key,
    const QVariant& value
    )
{
    if (value.isValid())
    {
        settings.set(key, value);
    }
    else
    {
        settings.remove(key);
    }

    settings.sync();
}

} // namespace

class LanguagePreferencePortTests final : public QObject
{
    Q_OBJECT

private slots:
    void portMapsEveryTypedPreferenceToTheLegacyService();
    void invalidPreferenceDoesNotCallOrMutateTheLanguageService();
    void controllerSynchronizesStartupStateWithoutReapplyingLanguage();
    void validActionUpdatesTypedStatePersistenceFontAndPresentation();
    void actionWritesCanonicalLanguageValuesAndKeepsOtherOptionWrites();
    void invalidOrFailedChangesPreserveTypedState();
};

void LanguagePreferencePortTests::portMapsEveryTypedPreferenceToTheLegacyService()
{
    LanguageService languageService;
    LanguagePreferencePort port(languageService);

    QVERIFY(port.apply(LanguagePreference::SystemDefault));
    QCOMPARE(languageService.currentLanguage(), Language::SystemDefault);

    QVERIFY(port.apply(LanguagePreference::English));
    QCOMPARE(languageService.currentLanguage(), Language::English);
    QVERIFY(!languageService.loadedLocaleName().isEmpty());
    QVERIFY(QFile::exists(
        QStringLiteral(":/i18n/ClassMngr_%1.qm")
            .arg(languageService.loadedLocaleName())
        ));

    QVERIFY(port.apply(LanguagePreference::Korean));
    QCOMPARE(languageService.currentLanguage(), Language::Korean);
    QCOMPARE(languageService.loadedLocaleName(), QStringLiteral("ko_KR"));
    QVERIFY(QFile::exists(QStringLiteral(":/i18n/ClassMngr_ko_KR.qm")));
}

void LanguagePreferencePortTests::
invalidPreferenceDoesNotCallOrMutateTheLanguageService()
{
    LanguageService languageService;
    LanguagePreferencePort port(languageService);
    QVERIFY(port.apply(LanguagePreference::Korean));

    const Language languageBefore = languageService.currentLanguage();
    const QString localeBefore = languageService.loadedLocaleName();

    const auto result = port.apply(
        static_cast<LanguagePreference>(99)
        );

    QVERIFY(!result);
    QCOMPARE(result.error().code, ErrorCode::InvalidInput);
    QCOMPARE(languageService.currentLanguage(), languageBefore);
    QCOMPARE(languageService.loadedLocaleName(), localeBefore);
}

void LanguagePreferencePortTests::
controllerSynchronizesStartupStateWithoutReapplyingLanguage()
{
    SettingsManager& settings = SettingsManager::instance();
    const QString key = languageSettingsKey();
    const QVariant savedValue = settings.get(key);
    settings.set(key, static_cast<int>(Language::Korean));

    ActionRegistry actions;
    actions.createActions();

    LanguageService languageService;
    QVERIFY(languageService.setLanguage(Language::English));
    const QString loadedLocaleBeforeConnect =
        languageService.loadedLocaleName();

    LanguageController controller(languageService, nullptr);
    controller.connectActions(actions);

    QCOMPARE(languageService.currentLanguage(), Language::English);
    QCOMPARE(
        languageService.loadedLocaleName(),
        loadedLocaleBeforeConnect
        );
    QCOMPARE(
        controller.preferencesSnapshot().languagePreference(),
        LanguagePreference::Korean
        );

    restoreSetting(settings, key, savedValue);
}

void LanguagePreferencePortTests::
validActionUpdatesTypedStatePersistenceFontAndPresentation()
{
    auto* app = qobject_cast<QApplication*>(QCoreApplication::instance());
    QVERIFY(app);

    SettingsManager& settings = SettingsManager::instance();
    const QString key = languageSettingsKey();
    const QVariant savedValue = settings.get(key);
    settings.set(key, static_cast<int>(Language::SystemDefault));

    ActionRegistry actions;
    actions.createActions();

    int previousHandlerCalls = 0;
    actions.languageState->onChanged =
        [&previousHandlerCalls](Language)
    {
        ++previousHandlerCalls;
    };

    LanguageService languageService;
    QVERIFY(languageService.setLanguage(Language::English));
    FontManager::applyGlobalFont(
        *app,
        languageService.loadedLocaleName()
        );
    const QFont englishFont = app->font();

    LanguageController controller(languageService, nullptr);
    controller.connectActions(actions);

    actions.languageState->set(Language::Korean);

    QCOMPARE(previousHandlerCalls, 1);
    QCOMPARE(languageService.currentLanguage(), Language::Korean);
    QCOMPARE(
        controller.preferencesSnapshot().languagePreference(),
        LanguagePreference::Korean
        );
    QCOMPARE(settings.get(key).toInt(), static_cast<int>(Language::Korean));
    QVERIFY(app->font() != englishFont);

    restoreSetting(settings, key, savedValue);
}

void LanguagePreferencePortTests::
actionWritesCanonicalLanguageValuesAndKeepsOtherOptionWrites()
{
    SettingsManager& settings = SettingsManager::instance();
    const QString languageKey = languageSettingsKey();
    const QString saveModeKey = QString::fromUtf8(OptionKeys::SaveMode);
    const QVariant savedLanguage = settings.get(languageKey);
    const QVariant savedSaveMode = settings.get(saveModeKey);

    settings.set(languageKey, 0);
    settings.set(saveModeKey, 0);

    ActionRegistry actions;
    actions.createActions();

    actions.languageState->set(Language::SystemDefault);
    QCOMPARE(settings.get(languageKey).toInt(), 0);

    actions.languageState->set(Language::English);
    QCOMPARE(settings.get(languageKey).toInt(), 1);

    actions.languageState->set(Language::Korean);
    QCOMPARE(settings.get(languageKey).toInt(), 5);

    actions.saveModeState->set(SaveMode::Manual);
    QCOMPARE(settings.get(saveModeKey).toInt(), 1);

    restoreSetting(settings, languageKey, savedLanguage);
    restoreSetting(settings, saveModeKey, savedSaveMode);
}

void LanguagePreferencePortTests::invalidOrFailedChangesPreserveTypedState()
{
    LanguageService languageService;
    LanguageController controller(languageService, nullptr);
    QVERIFY(controller.changeLanguage(LanguagePreference::English));
    const auto beforeInvalid = controller.preferencesSnapshot();

    const auto invalid = controller.changeLanguage(
        static_cast<LanguagePreference>(99)
        );
    QVERIFY(!invalid);
    QCOMPARE(invalid.error().code, ErrorCode::InvalidInput);
    QVERIFY(controller.preferencesSnapshot() == beforeInvalid);

    LanguageService serviceWithoutResources;
    LanguagePreferencePort port(serviceWithoutResources);
    const auto failed = port.apply(LanguagePreference::English);
    if (!failed)
    {
        QCOMPARE(failed.error().code, ErrorCode::Technical);
    }
    else
    {
        QVERIFY(serviceWithoutResources.loadedLocaleName().endsWith(
            QStringLiteral("en_US")
            ));
    }
}

QTEST_MAIN(LanguagePreferencePortTests)

#include "language_preference_port_tests.moc"
