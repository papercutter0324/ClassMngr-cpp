#include "core/settingsmanager.h"
#include "next/application/ai_comment_voice_preferences.h"
#include "next/platform/settings_manager_ai_comment_voice_preferences_port.h"

#include <QSettings>
#include <QTemporaryDir>
#include <QtTest/QtTest>

#include <string>

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
    void writesBothVoiceValuesForRoundTrip();
    void missingAndUnknownValuesDefaultToDirectToStudent();
    void unavailableSettingsDefaultToDirectToStudent();
    void syncFailurePreservesPreviousValueAndAllowsRetry();
    void syncFailurePreservesMissingValueAndUnrelatedSettings();

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
    writesBothVoiceValuesForRoundTrip()
{
    SettingsManager& settings = SettingsManager::instance();
    const SettingsManagerAiCommentVoicePreferencesPort port;

    QVERIFY(port.write(AiCommentVoice::ThirdPerson));
    QCOMPARE(
        settings.get(voiceKey()).toInt(),
        1
        );
    QCOMPARE(
        port.read(),
        AiCommentVoice::ThirdPerson
        );

    QVERIFY(port.write(AiCommentVoice::DirectToStudent));
    QCOMPARE(
        settings.get(voiceKey()).toInt(),
        0
        );
    QCOMPARE(
        port.read(),
        AiCommentVoice::DirectToStudent
        );

    const auto invalidWrite = port.write(
        static_cast<AiCommentVoice>(99)
        );
    QVERIFY(!invalidWrite);
    QCOMPARE(
        invalidWrite.error().code,
        Domain::ErrorCode::InvalidInput
        );
    QCOMPARE(
        settings.get(voiceKey()).toInt(),
        0
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

void NextPlatformSettingsManagerAiCommentVoicePreferencesPortTests::
syncFailurePreservesPreviousValueAndAllowsRetry()
{
    SettingsManager& settings = SettingsManager::instance();
    const QString key = voiceKey();
    const QString unrelatedKey = QStringLiteral("options/unrelatedValue");
    settings.set(key, 0);
    settings.set(unrelatedKey, 41);
    settings.sync();

    bool injectAccessError = true;
    int synchronizationAttempts = 0;
    const SettingsManagerAiCommentVoicePreferencesPort faultablePort(
        [&injectAccessError, &synchronizationAttempts](QSettings& settings)
        {
            ++synchronizationAttempts;
            if (injectAccessError)
            {
                return QSettings::AccessError;
            }

            settings.sync();
            return settings.status();
        }
        );

    const auto failedWrite = faultablePort.write(AiCommentVoice::ThirdPerson);
    QVERIFY(!failedWrite);
    QCOMPARE(failedWrite.error().code, Domain::ErrorCode::Technical);
    QVERIFY(failedWrite.error().recoverable);
    QVERIFY(
        failedWrite.error().message.find("AccessError")
        != std::string::npos
        );
    QCOMPARE(synchronizationAttempts, 1);
    QCOMPARE(settings.get(key).toInt(), 0);
    QCOMPARE(faultablePort.read(), AiCommentVoice::DirectToStudent);
    QCOMPARE(settings.get(unrelatedKey).toInt(), 41);

    QSettings persistedSettings(
        m_settingsRoot.filePath(QStringLiteral("PaperCloud/ClassMngr.ini")),
        QSettings::IniFormat
        );
    persistedSettings.sync();
    QCOMPARE(persistedSettings.status(), QSettings::NoError);
    QCOMPARE(persistedSettings.value(key).toInt(), 0);
    QCOMPARE(persistedSettings.value(unrelatedKey).toInt(), 41);

    injectAccessError = false;
    const auto retryResult = faultablePort.write(AiCommentVoice::ThirdPerson);
    QVERIFY(retryResult);
    QCOMPARE(synchronizationAttempts, 2);
    QCOMPARE(settings.get(key).toInt(), 1);
    QCOMPARE(faultablePort.read(), AiCommentVoice::ThirdPerson);
    QCOMPARE(settings.get(unrelatedKey).toInt(), 41);
    persistedSettings.sync();
    QCOMPARE(persistedSettings.status(), QSettings::NoError);
    QCOMPARE(persistedSettings.value(key).toInt(), 1);
    QCOMPARE(persistedSettings.value(unrelatedKey).toInt(), 41);
}

void NextPlatformSettingsManagerAiCommentVoicePreferencesPortTests::
syncFailurePreservesMissingValueAndUnrelatedSettings()
{
    SettingsManager& settings = SettingsManager::instance();
    const QString key = voiceKey();
    const QString unrelatedKey = QStringLiteral("options/unrelatedValue");
    settings.remove(key);
    settings.set(unrelatedKey, 29);
    settings.sync();

    const SettingsManagerAiCommentVoicePreferencesPort faultablePort(
        [](QSettings&)
        {
            return QSettings::AccessError;
        }
        );

    const auto failedWrite = faultablePort.write(AiCommentVoice::ThirdPerson);
    QVERIFY(!failedWrite);
    QCOMPARE(failedWrite.error().code, Domain::ErrorCode::Technical);
    QVERIFY(failedWrite.error().recoverable);
    QVERIFY(
        failedWrite.error().message.find("AccessError")
        != std::string::npos
        );
    QVERIFY(!settings.get(key).isValid());
    QCOMPARE(faultablePort.read(), AiCommentVoice::DirectToStudent);
    QCOMPARE(settings.get(unrelatedKey).toInt(), 29);

    QSettings persistedSettings(
        m_settingsRoot.filePath(QStringLiteral("PaperCloud/ClassMngr.ini")),
        QSettings::IniFormat
        );
    persistedSettings.sync();
    QCOMPARE(persistedSettings.status(), QSettings::NoError);
    QVERIFY(!persistedSettings.contains(key));
    QCOMPARE(persistedSettings.value(unrelatedKey).toInt(), 29);
}

QTEST_APPLESS_MAIN(
    NextPlatformSettingsManagerAiCommentVoicePreferencesPortTests
    )

#include "next_platform_settings_manager_ai_comment_voice_preferences_port_tests.moc"
