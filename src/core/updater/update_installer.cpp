#include "update_installer.h"

#include <QDesktopServices>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QUrl>

Status UpdateInstaller::launch(
    const QString& filePath
    )
{
    QFileInfo fileInfo(filePath);

    if (!fileInfo.exists() || !fileInfo.isFile())
    {
        return std::unexpected(
            QStringLiteral("Update installer file was not found.")
            );
    }

#if defined(Q_OS_WIN)
    const bool launched =
        QProcess::startDetached(
            fileInfo.absoluteFilePath(),
            {}
            );

    if (!launched)
    {
        return std::unexpected(
            QStringLiteral("Unable to launch the update installer.")
            );
    }

    return {};
#elif defined(Q_OS_MACOS)
    const bool launched =
        QProcess::startDetached(
            QStringLiteral("open"),
            {fileInfo.absoluteFilePath()}
            );

    if (!launched)
    {
        return std::unexpected(
            QStringLiteral("Unable to open the update package.")
            );
    }

    return {};
#else
    QFile file(
        fileInfo.absoluteFilePath()
        );

    QFileDevice::Permissions permissions =
        file.permissions();

    permissions |= QFileDevice::ExeOwner
        | QFileDevice::ExeUser
        | QFileDevice::ExeGroup
        | QFileDevice::ExeOther;

    file.setPermissions(permissions);

    const bool launched =
        QDesktopServices::openUrl(
            QUrl::fromLocalFile(
                fileInfo.absoluteFilePath()
                )
            );

    if (!launched)
    {
        return std::unexpected(
            QStringLiteral("Unable to open the update package.")
            );
    }

    return {};
#endif
}
