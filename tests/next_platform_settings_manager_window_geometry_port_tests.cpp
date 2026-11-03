#include "core/settingsmanager.h"
#include "next/platform/settings_manager_window_geometry_port.h"

#include <QTemporaryDir>
#include <QtTest/QtTest>

#include <cstddef>
#include <string>

using namespace ClassMngr::Next::Platform;

class NextPlatformSettingsManagerWindowGeometryPortTests final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanup();
    void usesExistingKeyAndRoundTripsBinaryGeometry();

private:
    QTemporaryDir m_settingsRoot;
};

void NextPlatformSettingsManagerWindowGeometryPortTests::initTestCase()
{
    QVERIFY(m_settingsRoot.isValid());
    QVERIFY(qputenv(
        "CLASSMNGR_SETTINGS_ROOT",
        m_settingsRoot.path().toUtf8()
        ));
    SettingsManager::instance().clear();
    SettingsManager::instance().sync();
}

void NextPlatformSettingsManagerWindowGeometryPortTests::cleanup()
{
    SettingsManager::instance().clear();
    SettingsManager::instance().sync();
}

void NextPlatformSettingsManagerWindowGeometryPortTests::
usesExistingKeyAndRoundTripsBinaryGeometry()
{
    QCOMPARE(
        QString::fromLatin1(SettingsManager::Keys::WINDOW_GEOMETRY),
        QStringLiteral("ui/windowGeometry")
        );

    const std::string expectedPayload("\0\x01\x7f\x80\xffg\0", 7);
    SettingsManager::instance().set(
        QStringLiteral("ui/windowGeometry"),
        QByteArray(
            expectedPayload.data(),
            static_cast<qsizetype>(expectedPayload.size())
            )
        );

    const SettingsManagerWindowGeometryPort port;
    QCOMPARE(port.read(), expectedPayload);

    const std::string writtenPayload("\xff\0geometry\x80", 11);
    port.write(writtenPayload);
    QCOMPARE(port.read(), writtenPayload);
    QCOMPARE(
        SettingsManager::instance().get(
            QStringLiteral("ui/windowGeometry")
            ).toByteArray(),
        QByteArray(
            writtenPayload.data(),
            static_cast<qsizetype>(writtenPayload.size())
            )
        );
}

QTEST_APPLESS_MAIN(NextPlatformSettingsManagerWindowGeometryPortTests)

#include "next_platform_settings_manager_window_geometry_port_tests.moc"
