#pragma once

#include "next/application/file_dialog_directory_preferences.h"

#include <QSettings>
#include <QString>

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

    void writeDirectory(
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
            m_settings->setValue(key, directory);
            return;
        }

        QSettings settings;
        settings.setValue(key, directory);
    }

private:
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
