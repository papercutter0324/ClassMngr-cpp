#pragma once

#include "core/resource_packs/resource_pack_manager.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QString>
#include <QStringList>

#ifndef CLASSMNGR_SOURCE_DIR
#define CLASSMNGR_SOURCE_DIR ""
#endif

namespace ResourcePaths::Detail
{
inline Result<ResourcePackLease> acquirePack(
    const QString& packId
    )
{
    return ResourcePackManager::instance().acquire(packId);
}

inline QString leasedPath(
    const ResourcePackLease& lease,
    const QString& relativePath = QString()
    )
{
    return relativePath.isEmpty()
        ? lease.root()
        : QDir(lease.root()).filePath(relativePath);
}

inline QString activePackPath(
    const QString& packId,
    const QString& relativePath = QString()
    )
{
    const QString root =
        ResourcePackManager::instance().activeRoot(packId);

    if (root.isEmpty())
    {
        return QString();
    }

    if (relativePath.isEmpty())
    {
        return root;
    }

    return QDir(root).filePath(relativePath);
}

inline QString resolveResourcePath(
    const QString& resourcePath
    )
{
    if (QFile::exists(resourcePath))
    {
        return resourcePath;
    }

    QString relativePath =
        resourcePath;

    if (relativePath.startsWith(":/"))
    {
        relativePath.remove(0, 2);
    }
    else if (relativePath.startsWith("qrc:/"))
    {
        relativePath.remove(0, 5);
    }

    QStringList candidates;

    const QString appDir =
        QCoreApplication::applicationDirPath();

    if (!appDir.isEmpty())
    {
        candidates.append(
            QDir::cleanPath(
                appDir + "/resources/" + relativePath
                )
            );

        candidates.append(
            QDir::cleanPath(
                appDir + "/../resources/" + relativePath
                )
            );

        candidates.append(
            QDir::cleanPath(
                appDir + "/../../resources/" + relativePath
                )
            );
    }

    candidates.append(
        QDir::cleanPath(
            QDir::currentPath() + "/resources/" + relativePath
            )
        );

    const QString sourceDir =
        QStringLiteral(CLASSMNGR_SOURCE_DIR);

    if (!sourceDir.isEmpty())
    {
        candidates.append(
            QDir::cleanPath(
                sourceDir + "/resources/" + relativePath
                )
            );
    }

    for (const QString& candidate : candidates)
    {
        if (QFile::exists(candidate))
        {
            return candidate;
        }
    }

    return resourcePath;
}
}

namespace ResourcePaths::Icons
{
inline constexpr auto AppWindows =
    ":/assets/icons/app_icon.ico";

inline constexpr auto AppDefault =
    ":/assets/icons/icon_256x256.png";

inline QString appWindows()
{
    return Detail::resolveResourcePath(
        QString::fromUtf8(AppWindows)
        );
}

inline QString appDefault()
{
    return Detail::resolveResourcePath(
        QString::fromUtf8(AppDefault)
        );
}
}

namespace ResourcePaths::Images
{
inline constexpr auto Splash =
    ":/assets/splash/splash.png";

inline QString splash()
{
    return Detail::resolveResourcePath(
        QString::fromUtf8(Splash)
        );
}
}

namespace ResourcePaths::Splash
{
inline Result<ResourcePackLease> acquire()
{
    return Detail::acquirePack(QStringLiteral("splash"));
}

inline QString imagePath(const ResourcePackLease& lease)
{
    return Detail::leasedPath(lease, QStringLiteral("splash.png"));
}
}

namespace ResourcePaths::DynamicImages
{
inline Result<ResourcePackLease> acquire()
{
    return Detail::acquirePack(QStringLiteral("images"));
}

inline QString filePath(
    const ResourcePackLease& lease,
    const QString& relativePath
    )
{
    return Detail::leasedPath(lease, relativePath);
}
}

namespace ResourcePaths::Files
{
inline Result<ResourcePackLease> acquire()
{
    return Detail::acquirePack(QStringLiteral("files"));
}

inline QString filePath(
    const ResourcePackLease& lease,
    const QString& relativePath
    )
{
    return Detail::leasedPath(lease, relativePath);
}
}

namespace ResourcePaths::Fonts
{
inline constexpr auto Inter =
    ":/assets/fonts/Inter.ttc";

inline constexpr auto Pretendard =
    ":/assets/fonts/PretendardVariable.ttf";

inline constexpr auto JustAnotherHand =
    ":/assets/fonts/JustAnotherHand-Regular.ttf";

inline constexpr auto DancingScript =
    ":/assets/fonts/DancingScript-wght.ttf";

inline constexpr auto GreatVibes =
    ":/assets/fonts/GreatVibes-Regular.ttf";

inline constexpr auto Caveat =
    ":/assets/fonts/Caveat-wght.ttf";

inline QString inter()
{
    return Detail::resolveResourcePath(
        QString::fromUtf8(Inter)
        );
}

inline QString pretendard()
{
    return Detail::resolveResourcePath(
        QString::fromUtf8(Pretendard)
        );
}

inline QString justAnotherHand()
{
    return Detail::resolveResourcePath(
        QString::fromUtf8(JustAnotherHand)
        );
}

inline QString dancingScript()
{
    return Detail::resolveResourcePath(
        QString::fromUtf8(DancingScript)
        );
}

inline QString greatVibes()
{
    return Detail::resolveResourcePath(
        QString::fromUtf8(GreatVibes)
        );
}

inline QString caveat()
{
    return Detail::resolveResourcePath(
        QString::fromUtf8(Caveat)
        );
}
}

namespace ResourcePaths::Templates
{
inline constexpr auto Report =
    ":/assets/templates/report.html";

inline constexpr auto Certificate =
    ":/assets/templates/certificate.html";

inline QString report()
{
    const QString packPath =
        Detail::activePackPath(
            QStringLiteral("templates"),
            QStringLiteral("report.html")
            );

    if (!packPath.isEmpty() && QFile::exists(packPath))
    {
        return packPath;
    }

    return Detail::resolveResourcePath(
        QString::fromUtf8(Report)
        );
}

inline QString certificate()
{
    const QString packPath =
        Detail::activePackPath(
            QStringLiteral("templates"),
            QStringLiteral("certificate.html")
            );

    if (!packPath.isEmpty() && QFile::exists(packPath))
    {
        return packPath;
    }

    return Detail::resolveResourcePath(
        QString::fromUtf8(Certificate)
        );
}

inline QString directory()
{
    const QString packPath =
        Detail::activePackPath(
            QStringLiteral("templates")
            );

    return packPath.isEmpty()
        ? QStringLiteral(":/assets/templates")
        : packPath;
}

inline Result<ResourcePackLease> acquireSpeakingEval()
{
    return Detail::acquirePack(QStringLiteral("templates"));
}

inline QString speakingEvalDirectory(const ResourcePackLease& lease)
{
    return Detail::leasedPath(lease, QStringLiteral("speaking-eval"));
}
}

namespace ResourcePaths::Campuses
{
inline constexpr auto Directory =
    ":/assets/campuses";

inline QString directory()
{
    const QString packPath =
        Detail::activePackPath(
            QStringLiteral("campuses")
            );

    if (!packPath.isEmpty())
    {
        return packPath;
    }

    const QString relativePath =
        QStringLiteral("assets/campuses");

    QStringList candidates;

    const QString sourceDir =
        QStringLiteral(CLASSMNGR_SOURCE_DIR);

    if (!sourceDir.isEmpty())
    {
        candidates.append(
            QDir::cleanPath(
                sourceDir + "/resources/" + relativePath
                )
            );
    }

    candidates.append(
        QDir::cleanPath(
            QDir::currentPath() + "/resources/" + relativePath
            )
        );

    const QString appDir =
        QCoreApplication::applicationDirPath();

    if (!appDir.isEmpty())
    {
        candidates.append(
            QDir::cleanPath(
                appDir + "/resources/" + relativePath
                )
            );

        candidates.append(
            QDir::cleanPath(
                appDir + "/../resources/" + relativePath
                )
            );

        candidates.append(
            QDir::cleanPath(
                appDir + "/../../resources/" + relativePath
                )
            );
    }

    for (const QString& candidate : candidates)
    {
        if (QDir(candidate).exists())
        {
            return candidate;
        }
    }

    return QString::fromUtf8(Directory);
}

inline Result<ResourcePackLease> acquire()
{
    return Detail::acquirePack(QStringLiteral("campuses"));
}

inline QString directory(const ResourcePackLease& lease)
{
    return Detail::leasedPath(lease);
}
}

namespace ResourcePaths::RosterDesigns
{
inline QString directory()
{
    const QString packPath =
        Detail::activePackPath(
            QStringLiteral("roster-designs")
            );

    return packPath.isEmpty()
        ? QStringLiteral(":/assets/roster-designs")
        : packPath;
}
}

namespace ResourcePaths::Documents
{
inline QString directory()
{
    const QString packPath =
        Detail::activePackPath(
            QStringLiteral("documents")
            );

    if (!packPath.isEmpty())
    {
        return packPath;
    }

    return Detail::resolveResourcePath(
        QStringLiteral(":/assets/documents")
        );
}

inline QString filePath(
    const QString& relativePath
    )
{
    const QString activePath =
        Detail::activePackPath(
            QStringLiteral("documents"),
            relativePath
            );

    if (!activePath.isEmpty() && QFile::exists(activePath))
    {
        return activePath;
    }

    return Detail::resolveResourcePath(
        QDir(QStringLiteral(":/assets/documents"))
            .filePath(relativePath)
        );
}

inline Result<ResourcePackLease> acquire()
{
    return Detail::acquirePack(QStringLiteral("documents"));
}

inline QString directory(const ResourcePackLease& lease)
{
    return Detail::leasedPath(lease);
}

inline QString filePath(
    const ResourcePackLease& lease,
    const QString& relativePath
    )
{
    return Detail::leasedPath(lease, relativePath);
}
}

namespace ResourcePaths::Styles
{
inline constexpr auto Dark =
    ":/assets/styles/dark.qss";

inline constexpr auto Light =
    ":/assets/styles/light.qss";

inline QString dark()
{
    return Detail::resolveResourcePath(
        QString::fromUtf8(Dark)
        );
}

inline QString light()
{
    return Detail::resolveResourcePath(
        QString::fromUtf8(Light)
        );
}
}
