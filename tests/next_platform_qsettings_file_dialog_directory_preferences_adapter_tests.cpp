#include "next/application/file_dialog_directory_preferences.h"
#include "next/platform/qsettings_file_dialog_directory_preferences_adapter.h"

#include <QSettings>
#include <QTemporaryDir>
#include <QtTest/QtTest>

#include <array>
#include <string>

using ClassMngr::Next::Application::FileDialogPurpose;
using ClassMngr::Next::Platform::
    QSettingsFileDialogDirectoryPreferencesAdapter;

class NextPlatformQSettingsFileDialogDirectoryPreferencesAdapterTests final
    : public QObject
{
    Q_OBJECT

private slots:
    void mapsEveryPurposeToItsStableKeyAndRoundTripsUtf8();
    void missingAndEmptyValuesReadAsAbsent();
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
        adapter.writeDirectory(
            mappings[index].purpose,
            std::string_view(
                expectedUtf8.constData(),
                static_cast<std::size_t>(expectedUtf8.size())
                )
            );
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

QTEST_APPLESS_MAIN(
    NextPlatformQSettingsFileDialogDirectoryPreferencesAdapterTests
    )

#include "next_platform_qsettings_file_dialog_directory_preferences_adapter_tests.moc"
