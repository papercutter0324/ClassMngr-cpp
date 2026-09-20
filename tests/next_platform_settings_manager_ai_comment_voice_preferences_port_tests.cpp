#include "core/settingsmanager.h"
#include "next/application/ai_comment_voice_preferences.h"
#include "next/platform/settings_manager_ai_comment_voice_preferences_port.h"

#include <QTemporaryDir>
#include <QtTest/QtTest>

using namespace ClassMngr::Next;
using namespace ClassMngr::Next::Application;
using namespace ClassMngr::Next::Platform;

namespace
{

QString voiceKey()
{
    return QString::fromUtf8(
        OptionKeys::AiCommentVoice
        );
}

} // namespace

class NextPlatformSettingsManagerAiCommentVoicePreferencesPortTests final
    : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanup();
    void usesTheExactLegacyKey();
    void mapsStoredVoiceValues();
    void missingAndUnknownValuesDefaultToDirectToStudent();
    void unavailableSettingsDefaultToDirectToStudent();

private:
    QTemporaryDir m_settingsRoot;
};

void NextPlatformSettingsManagerAiCommentVoicePreferencesPortTests::
initTestCase()
{
    QVERIFY(m_settingsRoot.isValid());
    QVERIFY(
        qputenv(
            "CLASSMNGR_SETTINGS_ROOT",
            m_settingsRoot.path().toUtf8()
            )
        );

    SettingsManager::instance().clear();
    SettingsManager::instance().sync();
}

void NextPlatformSettingsManagerAiCommentVoicePreferencesPortTests::cleanup()
{
    SettingsManager::instance().clear();
    SettingsManager::instance().sync();
}

void NextPlatformSettingsManagerAiCommentVoicePreferencesPortTests::
usesTheExactLegacyKey()
{
    QCOMPARE(
        voiceKey(),
        QStringLiteral("options/aiCommentVoice")
        );

    SettingsManager::instance().set(
        voiceKey(),
        1
        );

    const SettingsManagerAiCommentVoicePreferencesPort port;
    QCOMPARE(
        port.read(),
        AiCommentVoice::ThirdPerson
        );
}

void NextPlatformSettingsManagerAiCommentVoicePreferencesPortTests::
mapsStoredVoiceValues()
{
    SettingsManager& settings = SettingsManager::instance();
    const SettingsManagerAiCommentVoicePreferencesPort port;

    settings.set(voiceKey(), 0);
    QCOMPARE(
        port.read(),
        AiCommentVoice::DirectToStudent
        );

    settings.set(voiceKey(), 1);
    QCOMPARE(
        port.read(),
        AiCommentVoice::ThirdPerson
        );
}

void NextPlatformSettingsManagerAiCommentVoicePreferencesPortTests::
missingAndUnknownValuesDefaultToDirectToStudent()
{
    SettingsManager& settings = SettingsManager::instance();
    const SettingsManagerAiCommentVoicePreferencesPort port;

    settings.remove(voiceKey());
    QCOMPARE(
        port.read(),
        AiCommentVoice::DirectToStudent
        );

    settings.set(voiceKey(), -1);
    QCOMPARE(
        port.read(),
        AiCommentVoice::DirectToStudent
        );

    settings.set(voiceKey(), 2);
    QCOMPARE(
        port.read(),
        AiCommentVoice::DirectToStudent
        );

    settings.set(voiceKey(), QStringLiteral("unknown"));
    QCOMPARE(
        port.read(),
        AiCommentVoice::DirectToStudent
        );
}

void NextPlatformSettingsManagerAiCommentVoicePreferencesPortTests::
unavailableSettingsDefaultToDirectToStudent()
{
    SettingsManager::instance().set(
        voiceKey(),
        QVariant()
        );

    const SettingsManagerAiCommentVoicePreferencesPort port;
    QCOMPARE(
        port.read(),
        AiCommentVoice::DirectToStudent
        );
    QVERIFY(!SettingsManager::instance().get(voiceKey()).isValid());
}

QTEST_APPLESS_MAIN(
    NextPlatformSettingsManagerAiCommentVoicePreferencesPortTests
    )

#include "next_platform_settings_manager_ai_comment_voice_preferences_port_tests.moc"
