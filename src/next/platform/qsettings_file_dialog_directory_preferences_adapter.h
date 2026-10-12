#pragma once

#include "next/application/file_dialog_directory_preferences.h"

#include <QSettings>
#include <QString>
#include <QVariant>

#include <cstddef>
#include <string>
#include <string_view>

namespace ClassMngr::Next::Platform
{

// Keeps the established QSettings key names at the outer platform boundary.
// A supplied QSettings object is non-owning and supports isolated INI-backed
// tests; otherwise each operation uses the application's default settings.
class QSettingsFileDialogDirectoryPreferencesAdapter final
    : public Application::FileDialogDirectoryPreferencesPort
{
public:
    explicit QSettingsFileDialogDirectoryPreferencesAdapter(
        QSettings* settings = nullptr
        ) noexcept
        : m_settings(settings)
    {
    }

    [[nodiscard]] std::string readDirectory(
        const Application::FileDialogPurpose purpose
        ) const override
    {
        const QString key = settingsKey(purpose);
        const QString directory = m_settings
            ? m_settings->value(key).toString()
            : defaultSettingsValue(key);
        const QByteArray encodedDirectory = directory.toUtf8();
        return std::string(
            encodedDirectory.constData(),
            static_cast<std::size_t>(encodedDirectory.size())
            );
    }

    [[nodiscard]] Domain::Result<void> writeDirectory(
        const Application::FileDialogPurpose purpose,
        const std::string_view utf8Directory
        ) const override
    {
        const QString key = settingsKey(purpose);
        const QString directory = QString::fromUtf8(
            utf8Directory.data(),
            static_cast<qsizetype>(utf8Directory.size())
            );

        if (m_settings)
        {
            return writeChecked(*m_settings, key, directory);
        }

        // Keep default operations isolated so each accepted dialog operation
        // gets a fresh QSettings instance and can retry after a failed write.
        QSettings settings;
        return writeChecked(settings, key, directory);
    }

private:
    [[nodiscard]] static Domain::Result<void> writeChecked(
        QSettings& settings,
        const QString& key,
        const QString& directory
        )
    {
        const bool hadPreviousValue = settings.contains(key);
        const QVariant previousValue = settings.value(key);

        settings.setValue(key, directory);
        settings.sync();
        const QSettings::Status status = settings.status();
        if (status == QSettings::NoError)
        {
            return Domain::Result<void>::success();
        }

        if (hadPreviousValue)
        {
            settings.setValue(key, previousValue);
        }
        else
        {
            settings.remove(key);
        }

        const char* statusName = status == QSettings::AccessError
            ? "AccessError (settings could not be written)"
            : "FormatError (settings contain invalid data)";
        return Domain::Result<void>::failure({
            .code = Domain::ErrorCode::Technical,
            .message = std::string(
                "Unable to save file dialog directory preference: "
                "QSettings "
                ) + statusName + ".",
            .recoverable = true
        });
    }

    [[nodiscard]] static QString defaultSettingsValue(
        const QString& key
        )
    {
        QSettings settings;
        return settings.value(key).toString();
    }

    [[nodiscard]] static QString settingsKey(
        const Application::FileDialogPurpose purpose
        )
    {
        QString purposeSlug;
        switch (purpose)
        {
        case Application::FileDialogPurpose::General:
            purposeSlug = QStringLiteral("general");
            break;
        case Application::FileDialogPurpose::TeacherProfile:
            purposeSlug = QStringLiteral("teacher-profile");
            break;
        case Application::FileDialogPurpose::ImportWorkbook:
            purposeSlug = QStringLiteral("import-workbook");
            break;
        case Application::FileDialogPurpose::ExportReport:
            purposeSlug = QStringLiteral("export-report");
            break;
        case Application::FileDialogPurpose::SignatureImage:
            purposeSlug = QStringLiteral("signature-image");
            break;
        case Application::FileDialogPurpose::GeneratedPdf:
            purposeSlug = QStringLiteral("generated-pdf");
            break;
        case Application::FileDialogPurpose::ClassTransfer:
            purposeSlug = QStringLiteral("class-transfer");
            break;
        case Application::FileDialogPurpose::SubPrepPackage:
            purposeSlug = QStringLiteral("sub-prep-package");
            break;
        default:
            purposeSlug = QStringLiteral("general");
            break;
        }

        return QStringLiteral("file-dialog/directories/%1")
            .arg(purposeSlug);
    }

    QSettings* m_settings = nullptr;
};

} // namespace ClassMngr::Next::Platform
