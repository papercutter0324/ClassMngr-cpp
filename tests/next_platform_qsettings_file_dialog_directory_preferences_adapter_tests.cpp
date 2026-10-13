#include "next/application/file_dialog_directory_preferences.h"
#include "next/platform/qsettings_file_dialog_directory_preferences_adapter.h"

#include <QSettings>
#include <QCoreApplication>
#include <QFileInfo>
#include <QIODevice>
#include <QTemporaryDir>
#include <QtTest/QtTest>

#include <array>
#include <string>

using ClassMngr::Next::Application::FileDialogPurpose;
using ClassMngr::Next::Platform::
    QSettingsFileDialogDirectoryPreferencesAdapter;

namespace
{

QSettings::SettingsMap customSettingsValues;
int customSettingsWriteAttempts = 0;
int customSettingsFailuresRemaining = 0;

bool readCustomSettings(
    QIODevice& device,
    QSettings::SettingsMap& values
    )
{
    if (device.readAll() != QByteArrayLiteral("F689"))
    {
        return false;
    }
    values = customSettingsValues;
    return true;
}

bool writeCustomSettings(
    QIODevice& device,
    const QSettings::SettingsMap& values
    )
{
    ++customSettingsWriteAttempts;
    if (customSettingsFailuresRemaining > 0)
    {
        --customSettingsFailuresRemaining;
        return false;
    }

    if (device.write(QByteArrayLiteral("F689")) != 4)
    {
        return false;
    }
    customSettingsValues = values;
    return true;
}

QSettings::Format customSettingsFormat()
{
    static const QSettings::Format format = QSettings::registerFormat(
        QStringLiteral("f689settings"),
        readCustomSettings,
        writeCustomSettings
        );
    return format;
}

class ScopedDefaultSettingsConfiguration final
{
public:
    ScopedDefaultSettingsConfiguration()
        : m_previousFormat(QSettings::defaultFormat()),
          m_previousOrganization(QCoreApplication::organizationName()),
          m_previousApplication(QCoreApplication::applicationName()),
          m_format(customSettingsFormat())
    {
        QCoreApplication::setOrganizationName(
            QStringLiteral("ClassMngrF689SettingsTests")
            );
        QCoreApplication::setApplicationName(
            QStringLiteral("DirectoryPreferenceRetry")
            );
        QSettings::setDefaultFormat(m_format);
        {
            const QSettings previousPathSettings(QSettings::UserScope);
            m_previousPath = QFileInfo(
                previousPathSettings.fileName()
                ).absolutePath();
        }

        QSettings::setPath(
            m_format,
            QSettings::UserScope,
            m_temporaryDirectory.path()
            );
    }

    ~ScopedDefaultSettingsConfiguration()
    {
        QSettings::setDefaultFormat(m_previousFormat);
        QSettings::setPath(
            m_format,
            QSettings::UserScope,
            m_previousPath
            );
        QCoreApplication::setOrganizationName(m_previousOrganization);
        QCoreApplication::setApplicationName(m_previousApplication);
    }

    [[nodiscard]] bool isValid() const
    {
        return m_temporaryDirectory.isValid();
    }

    [[nodiscard]] QSettings::Format format() const
    {
        return m_format;
    }

private:
    QSettings::Format m_previousFormat;
    QString m_previousOrganization;
    QString m_previousApplication;
    QSettings::Format m_format;
    QString m_previousPath;
    QTemporaryDir m_temporaryDirectory;
};

void resetCustomSettingsStore()
{
    customSettingsValues.clear();
    customSettingsWriteAttempts = 0;
    customSettingsFailuresRemaining = 0;
}

}

class NextPlatformQSettingsFileDialogDirectoryPreferencesAdapterTests final
    : public QObject
{
    Q_OBJECT

private slots:
    void mapsEveryPurposeToItsStableKeyAndRoundTripsUtf8();
    void invalidPurposeWriteIsRejectedWithoutChangingSettings();
    void missingAndEmptyValuesReadAsAbsent();
    void failedWriteRestoresExactPriorValueAndLeavesOtherKeysUntouched();
    void failedWriteRestoresAnAbsentPurposeKey();
    void aLaterDefaultSettingsOperationCanRetry();
};

void NextPlatformQSettingsFileDialogDirectoryPreferencesAdapterTests::
mapsEveryPurposeToItsStableKeyAndRoundTripsUtf8()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    QSettings settings(
        temporaryDirectory.filePath(QStringLiteral("settings.ini")),
        QSettings::IniFormat
        );
    const QSettingsFileDialogDirectoryPreferencesAdapter adapter(&settings);

    struct Mapping
    {
        FileDialogPurpose purpose;
        QString key;
    };
    const std::array<Mapping, 8> mappings{{
        {FileDialogPurpose::General,
         QStringLiteral("file-dialog/directories/general")},
        {FileDialogPurpose::TeacherProfile,
         QStringLiteral("file-dialog/directories/teacher-profile")},
        {FileDialogPurpose::ImportWorkbook,
         QStringLiteral("file-dialog/directories/import-workbook")},
        {FileDialogPurpose::ExportReport,
         QStringLiteral("file-dialog/directories/export-report")},
        {FileDialogPurpose::SignatureImage,
         QStringLiteral("file-dialog/directories/signature-image")},
        {FileDialogPurpose::GeneratedPdf,
         QStringLiteral("file-dialog/directories/generated-pdf")},
        {FileDialogPurpose::ClassTransfer,
         QStringLiteral("file-dialog/directories/class-transfer")},
        {FileDialogPurpose::SubPrepPackage,
         QStringLiteral("file-dialog/directories/sub-prep-package")}
    }};

    for (std::size_t index = 0; index < mappings.size(); ++index)
    {
        const QString value = QStringLiteral("D:/stored/%1").arg(index);
        settings.setValue(mappings[index].key, value);
        const QByteArray expectedUtf8 = value.toUtf8();
        const std::string actual = adapter.readDirectory(
            mappings[index].purpose
            );
        QVERIFY(
            actual
                == std::string(
                       expectedUtf8.constData(),
                       static_cast<std::size_t>(expectedUtf8.size())
                       )
            );
    }

    for (std::size_t index = 0; index < mappings.size(); ++index)
    {
        const QString expected = QString::fromUtf8(
            "D:/수업 자료/保存/%1"
            ).arg(index);
        const QByteArray expectedUtf8 = expected.toUtf8();
        const auto writeResult = adapter.writeDirectory(
            mappings[index].purpose,
            std::string_view(
                expectedUtf8.constData(),
                static_cast<std::size_t>(expectedUtf8.size())
                )
            );
        QVERIFY(writeResult);
        QCOMPARE(settings.value(mappings[index].key).toString(), expected);

        const std::string actual = adapter.readDirectory(
            mappings[index].purpose
            );
        QVERIFY(
            actual
                == std::string(
                       expectedUtf8.constData(),
                       static_cast<std::size_t>(expectedUtf8.size())
                       )
            );
    }

    QStringList expectedKeys;
    for (const Mapping& mapping : mappings)
    {
        expectedKeys.append(mapping.key);
    }
    expectedKeys.sort();
    QStringList actualKeys = settings.allKeys();
    actualKeys.sort();
    QCOMPARE(actualKeys, expectedKeys);
}

void NextPlatformQSettingsFileDialogDirectoryPreferencesAdapterTests::
invalidPurposeWriteIsRejectedWithoutChangingSettings()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    QSettings settings(
        temporaryDirectory.filePath(QStringLiteral("settings.ini")),
        QSettings::IniFormat
        );
    const QString generalKey = QStringLiteral(
        "file-dialog/directories/general"
        );
    const QString otherKey = QStringLiteral(
        "file-dialog/directories/import-workbook"
        );
    settings.setValue(generalKey, QStringLiteral("D:/old general"));
    settings.setValue(otherKey, QStringLiteral("D:/keep import"));
    settings.sync();
    QCOMPARE(settings.status(), QSettings::NoError);
    const QVariant previousGeneralValue = settings.value(generalKey);
    const QVariant previousOtherValue = settings.value(otherKey);
    QStringList expectedKeys = settings.allKeys();
    expectedKeys.sort();

    const QSettingsFileDialogDirectoryPreferencesAdapter adapter(&settings);
    const QByteArray newDirectory = QStringLiteral("D:/new invalid").toUtf8();
    const auto result = adapter.writeDirectory(
        static_cast<FileDialogPurpose>(17),
        std::string_view(
            newDirectory.constData(),
            static_cast<std::size_t>(newDirectory.size())
            )
        );

    QVERIFY(!result);
    QCOMPARE(
        result.error().code,
        ClassMngr::Next::Domain::ErrorCode::InvalidInput
        );
    QVERIFY(!result.error().recoverable);
    QVERIFY(!result.error().message.empty());
    QCOMPARE(settings.value(generalKey), previousGeneralValue);
    QCOMPARE(settings.value(otherKey), previousOtherValue);
    QStringList actualKeys = settings.allKeys();
    actualKeys.sort();
    QCOMPARE(actualKeys, expectedKeys);
    QCOMPARE(
        adapter.readDirectory(static_cast<FileDialogPurpose>(17)),
        std::string("D:/old general")
        );
    QCOMPARE(settings.status(), QSettings::NoError);
}

void NextPlatformQSettingsFileDialogDirectoryPreferencesAdapterTests::
missingAndEmptyValuesReadAsAbsent()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    QSettings settings(
        temporaryDirectory.filePath(QStringLiteral("settings.ini")),
        QSettings::IniFormat
        );
    const QSettingsFileDialogDirectoryPreferencesAdapter adapter(&settings);
    const QString key = QStringLiteral(
        "file-dialog/directories/teacher-profile"
        );

    QVERIFY(
        adapter.readDirectory(FileDialogPurpose::TeacherProfile).empty()
        );

    settings.setValue(key, QString());
    QVERIFY(
        adapter.readDirectory(FileDialogPurpose::TeacherProfile).empty()
        );
}

void NextPlatformQSettingsFileDialogDirectoryPreferencesAdapterTests::
failedWriteRestoresExactPriorValueAndLeavesOtherKeysUntouched()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    resetCustomSettingsStore();
    const QSettings::Format format = customSettingsFormat();
    const QString settingsPath = temporaryDirectory.filePath(
        QStringLiteral("settings.f689settings")
        );
    const QString purposeKey = QStringLiteral(
        "file-dialog/directories/teacher-profile"
        );
    const QString unrelatedKey = QStringLiteral("recent-files/legacy");
    const QVariant previousValue = QByteArray("prior typed bytes");
    const QVariant unrelatedValue = QStringLiteral("keep this value");

    QSettings settings(settingsPath, format);
    settings.setValue(purposeKey, previousValue);
    settings.setValue(unrelatedKey, unrelatedValue);
    settings.sync();
    QCOMPARE(settings.status(), QSettings::NoError);

    customSettingsFailuresRemaining = 1;
    const QSettingsFileDialogDirectoryPreferencesAdapter adapter(&settings);
    const QByteArray newDirectory = QStringLiteral("D:/new directory").toUtf8();
    const auto result = adapter.writeDirectory(
        FileDialogPurpose::TeacherProfile,
        std::string_view(
            newDirectory.constData(),
            static_cast<std::size_t>(newDirectory.size())
            )
        );

    QVERIFY(!result);
    QCOMPARE(result.error().code, ClassMngr::Next::Domain::ErrorCode::Technical);
    QVERIFY(result.error().recoverable);
    QVERIFY(!result.error().message.empty());
    QCOMPARE(customSettingsWriteAttempts, 2);
    QCOMPARE(settings.value(purposeKey), previousValue);
    QCOMPARE(settings.value(unrelatedKey), unrelatedValue);

    QSettings reopened(settingsPath, format);
    QCOMPARE(reopened.value(purposeKey), previousValue);
    QCOMPARE(reopened.value(purposeKey).metaType(), previousValue.metaType());
    QCOMPARE(reopened.value(unrelatedKey), unrelatedValue);
    QCOMPARE(reopened.status(), QSettings::NoError);
}

void NextPlatformQSettingsFileDialogDirectoryPreferencesAdapterTests::
failedWriteRestoresAnAbsentPurposeKey()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    resetCustomSettingsStore();
    const QSettings::Format format = customSettingsFormat();
    const QString settingsPath = temporaryDirectory.filePath(
        QStringLiteral("settings.f689settings")
        );
    const QString purposeKey = QStringLiteral(
        "file-dialog/directories/general"
        );
    const QString unrelatedKey = QStringLiteral("last-file/path");
    const QVariant unrelatedValue = QByteArray("preserve legacy key");

    QSettings settings(settingsPath, format);
    settings.setValue(unrelatedKey, unrelatedValue);
    settings.sync();
    QCOMPARE(settings.status(), QSettings::NoError);
    QVERIFY(!settings.contains(purposeKey));

    customSettingsFailuresRemaining = 1;
    const QSettingsFileDialogDirectoryPreferencesAdapter adapter(&settings);
    const QByteArray newDirectory = QStringLiteral("D:/new").toUtf8();
    const auto result = adapter.writeDirectory(
        FileDialogPurpose::General,
        std::string_view(
            newDirectory.constData(),
            static_cast<std::size_t>(newDirectory.size())
            )
        );

    QVERIFY(!result);
    QCOMPARE(result.error().code, ClassMngr::Next::Domain::ErrorCode::Technical);
    QVERIFY(result.error().recoverable);
    QCOMPARE(customSettingsWriteAttempts, 2);
    QVERIFY(!settings.contains(purposeKey));
    QCOMPARE(settings.value(unrelatedKey), unrelatedValue);

    QSettings reopened(settingsPath, format);
    QVERIFY(!reopened.contains(purposeKey));
    QCOMPARE(reopened.value(unrelatedKey), unrelatedValue);
    QCOMPARE(reopened.status(), QSettings::NoError);
}

void NextPlatformQSettingsFileDialogDirectoryPreferencesAdapterTests::
aLaterDefaultSettingsOperationCanRetry()
{
    resetCustomSettingsStore();
    ScopedDefaultSettingsConfiguration defaultSettings;
    QVERIFY(defaultSettings.isValid());
    customSettingsFailuresRemaining = 1;

    const QSettingsFileDialogDirectoryPreferencesAdapter adapter;
    const QByteArray failedDirectory = QStringLiteral("D:/first").toUtf8();
    const auto firstResult = adapter.writeDirectory(
        FileDialogPurpose::ImportWorkbook,
        std::string_view(
            failedDirectory.constData(),
            static_cast<std::size_t>(failedDirectory.size())
            )
        );
    QVERIFY(!firstResult);
    QCOMPARE(
        firstResult.error().code,
        ClassMngr::Next::Domain::ErrorCode::Technical
        );
    QVERIFY(firstResult.error().recoverable);

    const QByteArray retryDirectory = QStringLiteral("D:/retry").toUtf8();
    const auto retryResult = adapter.writeDirectory(
        FileDialogPurpose::ImportWorkbook,
        std::string_view(
            retryDirectory.constData(),
            static_cast<std::size_t>(retryDirectory.size())
            )
        );
    QVERIFY(retryResult);
    QCOMPARE(customSettingsWriteAttempts, 2);
    QCOMPARE(customSettingsFailuresRemaining, 0);

    QSettings reopened(QSettings::UserScope);
    QCOMPARE(
        reopened.value(
            QStringLiteral("file-dialog/directories/import-workbook")
            ).toString(),
        QStringLiteral("D:/retry")
        );
    QCOMPARE(reopened.status(), QSettings::NoError);
}

QTEST_APPLESS_MAIN(
    NextPlatformQSettingsFileDialogDirectoryPreferencesAdapterTests
    )

#include "next_platform_qsettings_file_dialog_directory_preferences_adapter_tests.moc"
