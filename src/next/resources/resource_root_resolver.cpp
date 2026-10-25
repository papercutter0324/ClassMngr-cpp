#include "resource_root_resolver.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>

#include <utility>

#ifndef CLASSMNGR_NEXT_RESOURCE_RELATIVE_ROOT
#error "ClassMngrNext resource install layout must be supplied by CMake"
#endif

namespace ClassMngr::Next::Resources
{
namespace
{

ResourceError invalidArgument(QString message)
{
    return {
        ResourceErrorCode::InvalidArgument,
        std::move(message)
    };
}

ResourceError missingResource(QString message)
{
    return {
        ResourceErrorCode::MissingFile,
        std::move(message)
    };
}

}

ResourceResult<ResourcePackagePaths> ResourceRootResolver::resolve()
{
    const QString applicationDirectory = QCoreApplication::applicationDirPath();
    if (applicationDirectory.isEmpty())
    {
        return std::unexpected(invalidArgument(
            QStringLiteral("the current application's executable directory is unavailable")
            ));
    }
    return resolveFromApplicationDirectory(applicationDirectory);
}

ResourceResult<ResourcePackagePaths> ResourceRootResolver::resolveFromApplicationDirectory(
    const QString& applicationDirectory
    )
{
    if (!QDir::isAbsolutePath(applicationDirectory))
    {
        return std::unexpected(invalidArgument(
            QStringLiteral("application directory must be an absolute path")
            ));
    }

    const QFileInfo applicationInfo(QDir::cleanPath(applicationDirectory));
    if (!applicationInfo.exists() || !applicationInfo.isDir())
    {
        return std::unexpected(invalidArgument(
            QStringLiteral("application directory must be an existing directory")
            ));
    }

    const QString relativeRoot = QStringLiteral(CLASSMNGR_NEXT_RESOURCE_RELATIVE_ROOT);
    if (relativeRoot.isEmpty() || QDir::isAbsolutePath(relativeRoot)
        || relativeRoot.contains(QChar::Null) || relativeRoot.contains(QLatin1Char('\\')))
    {
        return std::unexpected(invalidArgument(
            QStringLiteral("configured resource root must be a relative POSIX path")
            ));
    }

    const QString selectedRootPath = QDir(applicationInfo.absoluteFilePath()).filePath(
        relativeRoot
        );
    const QFileInfo selectedRootInfo(selectedRootPath);
    const QString canonicalRoot = selectedRootInfo.canonicalFilePath();
    if (canonicalRoot.isEmpty() || !selectedRootInfo.isDir())
    {
        return std::unexpected(missingResource(
            QStringLiteral("installed resource root is missing at the selected package location")
            ));
    }

    const QString catalogPath = QDir(canonicalRoot).filePath(
        QStringLiteral("resource-catalog.json")
        );
    const QFileInfo catalogInfo(catalogPath);
    if (catalogInfo.isSymLink() || !catalogInfo.exists() || !catalogInfo.isFile())
    {
        return std::unexpected(missingResource(
            QStringLiteral("installed resource catalog is missing at the selected package location")
            ));
    }

    return ResourcePackagePaths{
        canonicalRoot,
        catalogInfo.absoluteFilePath()
    };
}

}
