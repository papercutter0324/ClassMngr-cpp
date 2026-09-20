#include "core/settingsmanager.h"
#include "next/application/dialog_geometry_preferences_port.h"
#include "next/platform/settings_manager_dialog_geometry_preferences_port.h"

#include <QTemporaryDir>
#include <QtTest/QtTest>

#include <cstddef>
#include <string>

using namespace ClassMngr::Next;
using namespace ClassMngr::Next::Application;
using namespace ClassMngr::Next::Platform;

namespace
{

QString geometryKey(
    const QString& normalizedDialogKey
    )
{
    return QStringLiteral("ui/dialogs/%1/geometry")
        .arg(normalizedDialogKey);
}

QByteArray bytes(
    const std::string& value
    )
{
    return QByteArray(
        value.data(),
        static_cast<qsizetype>(value.size())
        );
}

} // namespace

class NextPlatformSettingsManagerDialogGeometryPreferencesPortTests final
    : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanup();
    void usesTheExactDynamicGeometryKey();
    void missingAndEmptyValuesReadAsEmpty();
    void roundTripsBinaryGeometryPayload();
    void isolatesEachDialogKey();
    void preservesDialogShellKeyNormalization();
    void emptyDialogKeyDoesNotWrite();

private:
    QTemporaryDir m_settingsRoot;
};

void NextPlatformSettingsManagerDialogGeometryPreferencesPortTests::
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

void NextPlatformSettingsManagerDialogGeometryPreferencesPortTests::cleanup()
{
    SettingsManager::instance().clear();
    SettingsManager::instance().sync();
}

void NextPlatformSettingsManagerDialogGeometryPreferencesPortTests::
usesTheExactDynamicGeometryKey()
{
    const QString expectedKey =
        QStringLiteral("ui/dialogs/geometryPolicy/geometry");
    QCOMPARE(geometryKey(QStringLiteral("geometryPolicy")), expectedKey);

    SettingsManager::instance().set(
        expectedKey,
        QByteArray("legacy-geometry")
        );

    const SettingsManagerDialogGeometryPreferencesPort port;
    QCOMPARE(
        bytes(port.read("geometryPolicy")),
        QByteArray("legacy-geometry")
        );
}

void NextPlatformSettingsManagerDialogGeometryPreferencesPortTests::
missingAndEmptyValuesReadAsEmpty()
{
    SettingsManager& settings = SettingsManager::instance();
    const SettingsManagerDialogGeometryPreferencesPort port;

    settings.remove(geometryKey(QStringLiteral("missingGeometry")));
    QVERIFY(port.read("missingGeometry").empty());

    settings.set(
        geometryKey(QStringLiteral("emptyGeometry")),
        QByteArray()
        );
    QVERIFY(port.read("emptyGeometry").empty());
}

void NextPlatformSettingsManagerDialogGeometryPreferencesPortTests::
roundTripsBinaryGeometryPayload()
{
    const std::string expectedPayload("\0\x01\x7f\x80\xffg\0", 7);

    const SettingsManagerDialogGeometryPreferencesPort port;
    port.write("binaryGeometry", expectedPayload);

    QCOMPARE(port.read("binaryGeometry"), expectedPayload);
    QCOMPARE(
        SettingsManager::instance().get(
            geometryKey(QStringLiteral("binaryGeometry"))
            ).toByteArray(),
        bytes(expectedPayload)
        );
}

void NextPlatformSettingsManagerDialogGeometryPreferencesPortTests::
isolatesEachDialogKey()
{
    const SettingsManagerDialogGeometryPreferencesPort port;
    port.write("firstDialog", "first-payload");
    port.write("secondDialog", "second-payload");

    QCOMPARE(port.read("firstDialog"), std::string("first-payload"));
    QCOMPARE(port.read("secondDialog"), std::string("second-payload"));
}

void NextPlatformSettingsManagerDialogGeometryPreferencesPortTests::
preservesDialogShellKeyNormalization()
{
    const std::string rawKey = "  report/dialog # 1?  ";
    const QString normalizedKey = QStringLiteral("report_dialog___1_");
    const std::string expectedPayload = "normalized-payload";

    const SettingsManagerDialogGeometryPreferencesPort port;
    port.write(rawKey, expectedPayload);

    QCOMPARE(port.read(rawKey), expectedPayload);
    QCOMPARE(
        SettingsManager::instance().get(geometryKey(normalizedKey)).toByteArray(),
        bytes(expectedPayload)
        );
}

void NextPlatformSettingsManagerDialogGeometryPreferencesPortTests::
emptyDialogKeyDoesNotWrite()
{
    const SettingsManagerDialogGeometryPreferencesPort port;
    port.write("", "must-not-be-stored");

    QVERIFY(
        !SettingsManager::instance()
             .get(QStringLiteral("ui/dialogs//geometry"))
             .isValid()
        );
    QVERIFY(port.read("").empty());
}

QTEST_APPLESS_MAIN(
    NextPlatformSettingsManagerDialogGeometryPreferencesPortTests
    )

#include "next_platform_settings_manager_dialog_geometry_preferences_port_tests.moc"
