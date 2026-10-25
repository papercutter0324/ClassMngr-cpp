#include "next/resources/resource_root_resolver.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>

#include <iostream>

using namespace ClassMngr::Next::Resources;

namespace
{

bool fail(const QString& message)
{
    std::cerr << message.toStdString() << '\n';
    return false;
}

QString configuredRelativeRoot()
{
    return QStringLiteral(CLASSMNGR_NEXT_RESOURCE_RELATIVE_ROOT);
}

QString resourceRootFor(const QString& applicationDirectory)
{
    return QDir::cleanPath(QDir(applicationDirectory).filePath(configuredRelativeRoot()));
}

bool createPackageFixture(const QString& applicationDirectory)
{
    if (!QDir().mkpath(applicationDirectory))
    {
        return false;
    }

    const QString resourceRoot = resourceRootFor(applicationDirectory);
    if (!QDir().mkpath(resourceRoot))
    {
        return false;
    }

    QFile catalog(QDir(resourceRoot).filePath(QStringLiteral("resource-catalog.json")));
    return catalog.open(QIODevice::WriteOnly | QIODevice::Truncate)
        && catalog.write("{}\n") == 3;
}

bool testExpectedApplicationDirectoryLayout()
{
    QTemporaryDir temporaryDirectory;
    if (!temporaryDirectory.isValid())
    {
        return fail(QStringLiteral("could not create application-directory fixture"));
    }

    const QString applicationDirectory = QDir(temporaryDirectory.path()).filePath(
        QStringLiteral("install/bin")
        );
    if (!createPackageFixture(applicationDirectory))
    {
        return fail(QStringLiteral("could not create the selected installed resource tree"));
    }

    const auto resolved = ResourceRootResolver::resolveFromApplicationDirectory(
        applicationDirectory
        );
    if (!resolved)
    {
        return fail(QStringLiteral("resolver rejected the configured installed layout: ")
            + resolved.error().message);
    }

    const QString expectedRoot = QFileInfo(resourceRootFor(applicationDirectory))
                                     .canonicalFilePath();
    const QString expectedCatalog = QDir(expectedRoot).filePath(
        QStringLiteral("resource-catalog.json")
        );
    if (resolved->resourceRoot != expectedRoot
        || resolved->catalogFilePath != expectedCatalog)
    {
        return fail(QStringLiteral("resolver returned paths outside the configured layout"));
    }
    return true;
}

bool testMissingRootAndCatalogFailWithoutSearching()
{
    QTemporaryDir temporaryDirectory;
    if (!temporaryDirectory.isValid())
    {
        return fail(QStringLiteral("could not create missing-resource fixture"));
    }

    const QString applicationDirectory = QDir(temporaryDirectory.path()).filePath(
        QStringLiteral("install/bin")
        );
    if (!QDir().mkpath(applicationDirectory))
    {
        return fail(QStringLiteral("could not create empty application directory"));
    }

    auto missingRoot = ResourceRootResolver::resolveFromApplicationDirectory(
        applicationDirectory
        );
    if (missingRoot || missingRoot.error().code != ResourceErrorCode::MissingFile)
    {
        return fail(QStringLiteral("resolver did not fail when the selected root was missing"));
    }

    const QString resourceRoot = resourceRootFor(applicationDirectory);
    if (!QDir().mkpath(resourceRoot))
    {
        return fail(QStringLiteral("could not create empty resource root"));
    }
    auto missingCatalog = ResourceRootResolver::resolveFromApplicationDirectory(
        applicationDirectory
        );
    if (missingCatalog || missingCatalog.error().code != ResourceErrorCode::MissingFile)
    {
        return fail(QStringLiteral("resolver did not fail when the selected catalog was missing"));
    }

    const QString unrelatedCurrentDirectory = QDir(temporaryDirectory.path()).filePath(
        QStringLiteral("unrelated-current-directory")
        );
    const QString fallbackRoot = QDir(unrelatedCurrentDirectory).filePath(
        QStringLiteral("resources")
        );
    if (!QDir().mkpath(unrelatedCurrentDirectory)
        || !QDir().mkpath(fallbackRoot))
    {
        return fail(QStringLiteral("could not create unrelated current-directory fallback"));
    }
    QFile fallbackCatalog(QDir(fallbackRoot).filePath(QStringLiteral("resource-catalog.json")));
    if (!fallbackCatalog.open(QIODevice::WriteOnly) || fallbackCatalog.write("{}\n") != 3)
    {
        return fail(QStringLiteral("could not stage current-directory fallback catalog"));
    }

    const QString previousCurrentDirectory = QDir::currentPath();
    if (!QDir::setCurrent(unrelatedCurrentDirectory))
    {
        return fail(QStringLiteral("could not set unrelated current directory for check"));
    }
    auto stillMissing = ResourceRootResolver::resolveFromApplicationDirectory(
        applicationDirectory
        );
    const bool restoredCurrentDirectory = QDir::setCurrent(previousCurrentDirectory);
    if (!restoredCurrentDirectory)
    {
        return fail(QStringLiteral("could not restore current directory after check"));
    }
    if (stillMissing || stillMissing.error().code != ResourceErrorCode::MissingFile)
    {
        return fail(QStringLiteral("resolver used an unrelated current-directory resource tree"));
    }
    return true;
}

bool testRelocatedPackageTreeResolvesFromNewExecutableDirectory()
{
    QTemporaryDir temporaryDirectory;
    if (!temporaryDirectory.isValid())
    {
        return fail(QStringLiteral("could not create relocation fixture"));
    }

    const QString originalApplicationDirectory = QDir(temporaryDirectory.path()).filePath(
        QStringLiteral("package-a/bin")
        );
    if (!createPackageFixture(originalApplicationDirectory))
    {
        return fail(QStringLiteral("could not create original installed resource tree"));
    }

    const auto original = ResourceRootResolver::resolveFromApplicationDirectory(
        originalApplicationDirectory
        );
    if (!original)
    {
        return fail(QStringLiteral("resolver rejected original installed tree: ")
            + original.error().message);
    }

    if (!QDir(temporaryDirectory.path()).rename(
            QStringLiteral("package-a"),
            QStringLiteral("package-b")
            ))
    {
        return fail(QStringLiteral("could not relocate installed fixture tree"));
    }

    const QString relocatedApplicationDirectory = QDir(temporaryDirectory.path()).filePath(
        QStringLiteral("package-b/bin")
        );
    const auto relocated = ResourceRootResolver::resolveFromApplicationDirectory(
        relocatedApplicationDirectory
        );
    if (!relocated)
    {
        return fail(QStringLiteral("resolver failed after package relocation: ")
            + relocated.error().message);
    }

    const QString relocatedExpectedRoot = QFileInfo(
        resourceRootFor(relocatedApplicationDirectory)
        ).canonicalFilePath();
    if (relocated->resourceRoot != relocatedExpectedRoot
        || relocated->resourceRoot == original->resourceRoot)
    {
        return fail(QStringLiteral("resolver retained the pre-relocation absolute resource path"));
    }
    return true;
}

}

int main()
{
    if (!testExpectedApplicationDirectoryLayout()
        || !testMissingRootAndCatalogFailWithoutSearching()
        || !testRelocatedPackageTreeResolvesFromNewExecutableDirectory())
    {
        return 1;
    }
    return 0;
}
