#include "core/settingsmanager.h"
#include "next/platform/settings_manager_splitter_state_port.h"

#include <QTemporaryDir>
#include <QtTest/QtTest>

#include <array>
#include <string>
#include <utility>

using namespace ClassMngr::Next;
using namespace ClassMngr::Next::Application;
using namespace ClassMngr::Next::Platform;

namespace
{

QString keyFor(const QString& semanticKey)
{
    return QStringLiteral("ui/next/splitters/%1").arg(semanticKey);
}

} // namespace

class NextPlatformSettingsManagerSplitterStatePortTests final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanup();
    void mapsEveryStableSplitterToAnIndependentNamespacedKey();
    void roundTripsBinaryPayloadWithoutUsingTheLegacyKey();
    void missingEmptyAndNonBinaryValuesReadAsEmpty();

private:
    QTemporaryDir m_settingsRoot;
};

void NextPlatformSettingsManagerSplitterStatePortTests::initTestCase()
{
    QVERIFY(m_settingsRoot.isValid());
    QVERIFY(qputenv(
        "CLASSMNGR_SETTINGS_ROOT",
        m_settingsRoot.path().toUtf8()
        ));
    SettingsManager::instance().clear();
    SettingsManager::instance().sync();
}

void NextPlatformSettingsManagerSplitterStatePortTests::cleanup()
{
    SettingsManager::instance().clear();
    SettingsManager::instance().sync();
}

void NextPlatformSettingsManagerSplitterStatePortTests::
mapsEveryStableSplitterToAnIndependentNamespacedKey()
{
    const std::array<std::pair<SplitterStateKey, QString>, 4> expectedKeys{{
        {SplitterStateKey::ClassesNavigatorContent,
            keyFor(QStringLiteral("classesNavigatorContent"))},
        {SplitterStateKey::TestingClassesManager,
            keyFor(QStringLiteral("testingClassesManager"))},
        {SplitterStateKey::SpeakingEvaluationContent,
            keyFor(QStringLiteral("speakingEvaluationContent"))},
        {SplitterStateKey::ClassTransferWorkflow,
            keyFor(QStringLiteral("classTransferWorkflow"))}
    }};

    const SettingsManagerSplitterStatePort port;
    for (std::size_t index = 0; index < expectedKeys.size(); ++index)
    {
        const std::string payload = "state-" + std::to_string(index);
        port.write(expectedKeys[index].first, payload);
        QCOMPARE(
            SettingsManager::instance().get(expectedKeys[index].second)
                .toByteArray(),
            QByteArray::fromStdString(payload)
            );
        QCOMPARE(port.read(expectedKeys[index].first), payload);
    }

    for (std::size_t left = 0; left < expectedKeys.size(); ++left)
    {
        for (std::size_t right = left + 1; right < expectedKeys.size(); ++right)
        {
            QVERIFY(expectedKeys[left].second != expectedKeys[right].second);
        }
    }
}

void NextPlatformSettingsManagerSplitterStatePortTests::
roundTripsBinaryPayloadWithoutUsingTheLegacyKey()
{
    const std::string expectedPayload("\0\x01\x7f\x80\xffx\0", 7);
    const SettingsManagerSplitterStatePort port;

    SettingsManager::instance().set(
        QStringLiteral("splitterState"),
        QByteArray("legacy-state")
        );
    port.write(SplitterStateKey::ClassesNavigatorContent, expectedPayload);

    QCOMPARE(port.read(SplitterStateKey::ClassesNavigatorContent), expectedPayload);
    QCOMPARE(
        SettingsManager::instance().get(
            keyFor(QStringLiteral("classesNavigatorContent"))
            ).toByteArray(),
        QByteArray(expectedPayload.data(),
            static_cast<qsizetype>(expectedPayload.size()))
        );
    QCOMPARE(
        SettingsManager::instance().get(QStringLiteral("splitterState"))
            .toByteArray(),
        QByteArray("legacy-state")
        );
}

void NextPlatformSettingsManagerSplitterStatePortTests::
missingEmptyAndNonBinaryValuesReadAsEmpty()
{
    const SettingsManagerSplitterStatePort port;
    const QString settingKey = keyFor(QStringLiteral("testingClassesManager"));

    QVERIFY(port.read(SplitterStateKey::TestingClassesManager).empty());
    SettingsManager::instance().set(settingKey, QByteArray{});
    QVERIFY(port.read(SplitterStateKey::TestingClassesManager).empty());
    SettingsManager::instance().set(settingKey, QStringLiteral("stale text"));
    QVERIFY(port.read(SplitterStateKey::TestingClassesManager).empty());
}

QTEST_APPLESS_MAIN(NextPlatformSettingsManagerSplitterStatePortTests)

#include "next_platform_settings_manager_splitter_state_port_tests.moc"
