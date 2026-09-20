#include "core/settingsmanager.h"
#include "next/application/ai_comment_custom_website_port.h"
#include "next/platform/settings_manager_ai_comment_custom_website_port.h"

#include <QTemporaryDir>
#include <QtTest/QtTest>

#include <cstddef>
#include <string>

using namespace ClassMngr::Next;
using namespace ClassMngr::Next::Application;
using namespace ClassMngr::Next::Platform;

namespace
{

QString websiteKey()
{
    return QString::fromUtf8(
        OptionKeys::AiCommentCustomWebsiteUrl
        );
}

std::string utf8(
    const QString& value
    )
{
    const QByteArray encoded = value.toUtf8();
    return std::string(
        encoded.constData(),
        static_cast<std::size_t>(encoded.size())
        );
}

QString fromUtf8(
    const std::string& value
    )
{
    return QString::fromUtf8(
        value.data(),
        static_cast<qsizetype>(value.size())
        );
}

} // namespace

class NextPlatformSettingsManagerAiCommentCustomWebsitePortTests final
    : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanup();
    void usesTheExactLegacyKey();
    void missingAndEmptyValuesReadAsEmpty();
    void roundTripsUtf8WebsiteText();
    void writeAndClearUseTypedValues();
    void unavailableSettingsReadAsEmpty();

private:
    QTemporaryDir m_settingsRoot;
};

void NextPlatformSettingsManagerAiCommentCustomWebsitePortTests::
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

void NextPlatformSettingsManagerAiCommentCustomWebsitePortTests::cleanup()
{
    SettingsManager::instance().clear();
    SettingsManager::instance().sync();
}

void NextPlatformSettingsManagerAiCommentCustomWebsitePortTests::
usesTheExactLegacyKey()
{
    QCOMPARE(
        websiteKey(),
        QStringLiteral("options/aiCommentCustomWebsiteUrl")
        );

    SettingsManager& settings = SettingsManager::instance();
    settings.set(websiteKey(), QStringLiteral("https://legacy.example"));

    const SettingsManagerAiCommentCustomWebsitePort port;
    QCOMPARE(
        fromUtf8(port.read()),
        QStringLiteral("https://legacy.example")
        );
}

void NextPlatformSettingsManagerAiCommentCustomWebsitePortTests::
missingAndEmptyValuesReadAsEmpty()
{
    SettingsManager& settings = SettingsManager::instance();
    settings.remove(websiteKey());

    const SettingsManagerAiCommentCustomWebsitePort port;
    QVERIFY(port.read().empty());

    settings.set(websiteKey(), QString());
    QVERIFY(port.read().empty());
}

void NextPlatformSettingsManagerAiCommentCustomWebsitePortTests::
roundTripsUtf8WebsiteText()
{
    const QString expectedWebsite = QString::fromUtf8(
        "https://example.ai/\xED\x95\x99\xEA\xB5\x90-\xF0\x9F\x93\x9A"
        );

    const SettingsManagerAiCommentCustomWebsitePort port;
    port.write(utf8(expectedWebsite));

    QCOMPARE(fromUtf8(port.read()), expectedWebsite);
    QCOMPARE(
        SettingsManager::instance().get(websiteKey()).toString(),
        expectedWebsite
        );
}

void NextPlatformSettingsManagerAiCommentCustomWebsitePortTests::
writeAndClearUseTypedValues()
{
    const SettingsManagerAiCommentCustomWebsitePort port;
    port.write(std::string("https://example.ai/chat"));
    QCOMPARE(
        fromUtf8(port.read()),
        QStringLiteral("https://example.ai/chat")
        );

    port.clear();
    QVERIFY(port.read().empty());
    QVERIFY(!SettingsManager::instance().get(websiteKey()).isValid());
}

void NextPlatformSettingsManagerAiCommentCustomWebsitePortTests::
unavailableSettingsReadAsEmpty()
{
    SettingsManager::instance().set(websiteKey(), QVariant());

    const SettingsManagerAiCommentCustomWebsitePort port;
    QVERIFY(port.read().empty());
    QVERIFY(!SettingsManager::instance().get(websiteKey()).isValid());
}

QTEST_APPLESS_MAIN(
    NextPlatformSettingsManagerAiCommentCustomWebsitePortTests
    )

#include "next_platform_settings_manager_ai_comment_custom_website_port_tests.moc"
