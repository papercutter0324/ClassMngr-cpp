#pragma once

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

namespace ResourcePaths::Fonts
{
inline constexpr auto Inter =
    ":/assets/fonts/Inter.ttc";

inline constexpr auto Pretendard =
    ":/assets/fonts/PretendardVariable.ttf";

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
}

namespace ResourcePaths::Templates
{
inline constexpr auto Report =
    ":/assets/templates/report.html";

inline constexpr auto Certificate =
    ":/assets/templates/certificate.html";

inline QString report()
{
    return Detail::resolveResourcePath(
        QString::fromUtf8(Report)
        );
}

inline QString certificate()
{
    return Detail::resolveResourcePath(
        QString::fromUtf8(Certificate)
        );
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
