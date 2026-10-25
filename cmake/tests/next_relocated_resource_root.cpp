#include "next/resources/resource_catalog.h"
#include "next/resources/resource_loader.h"
#include "next/resources/resource_root_resolver.h"

#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonObject>
#include <QProcess>
#include <QTemporaryDir>
#include <QTextStream>

#include <cstdio>
#include <utility>

using namespace ClassMngr::Next::Resources;

namespace
{

constexpr int ExpectedMissingRootExitCode = 32;

int fail(const QString& message)
{
    const QByteArray utf8 = message.toUtf8();
    std::fprintf(stderr, "%s\n", utf8.constData());
    return 1;
}

bool failTest(const QString& message)
{
    fail(message);
    return false;
}

QString configuredRelativeRoot()
{
    return QStringLiteral(CLASSMNGR_NEXT_RESOURCE_RELATIVE_ROOT);
}

QString selectedRootFor(const QString& applicationDirectory)
{
    return QDir::cleanPath(QDir(applicationDirectory).filePath(configuredRelativeRoot()));
}

QString canonicalDirectory(const QString& path)
{
    return QFileInfo(path).canonicalFilePath();
}

bool stageFile(const QString& sourcePath, const QString& destinationPath)
{
    if (!QDir().mkpath(QFileInfo(destinationPath).absolutePath()))
    {
        return false;
    }
    QFile::remove(destinationPath);
    return QFile::copy(sourcePath, destinationPath);
}

bool stagePackage(const QString& packageRoot)
{
    const QString catalogSource = QString::fromUtf8(CLASSMNGR_NEXT_TEST_RESOURCE_CATALOG);
    const QString resourceSource = QDir(
        QString::fromUtf8(CLASSMNGR_NEXT_TEST_RESOURCE_ROOT)
        ).filePath(QStringLiteral("assets/campuses/bundang.json"));
    return stageFile(
               catalogSource,
               QDir(packageRoot).filePath(QStringLiteral("resource-catalog.json"))
               )
        && stageFile(
            resourceSource,
            QDir(packageRoot).filePath(QStringLiteral("assets/campuses/bundang.json"))
            );
}

bool copyProbeExecutable(const QString& executablePath, const QString& destinationPath)
{
    if (!QDir().mkpath(QFileInfo(destinationPath).absolutePath()))
    {
        return false;
    }
    QFile::remove(destinationPath);
    if (!QFile::copy(executablePath, destinationPath))
    {
        return false;
    }

    const QFileDevice::Permissions executablePermissions = QFile::permissions(executablePath)
        | QFileDevice::ExeOwner
        | QFileDevice::ExeGroup
        | QFileDevice::ExeOther;
    return QFile::setPermissions(destinationPath, executablePermissions);
}

int runProbe(bool expectMissingRoot)
{
    QTextStream output(stdout);
    const QString applicationDirectory = canonicalDirectory(
        QCoreApplication::applicationDirPath()
        );
    const QString selectedRoot = selectedRootFor(applicationDirectory);
    output << "PROBE_APPLICATION_DIRECTORY=" << applicationDirectory << Qt::endl;
    output << "PROBE_SELECTED_ROOT=" << selectedRoot << Qt::endl;

    const auto resolved = ResourceRootResolver::resolve();
    if (expectMissingRoot)
    {
        constexpr auto expectedMessage =
            "installed resource root is missing at the selected package location";
        if (resolved)
        {
            output << "PROBE_UNEXPECTED_RESOLUTION=" << resolved->resourceRoot << Qt::endl;
            return 30;
        }
        if (resolved.error().code != ResourceErrorCode::MissingFile
            || resolved.error().message != QString::fromLatin1(expectedMessage))
        {
            output << "PROBE_WRONG_FAILURE=" << resolved.error().message << Qt::endl;
            return 31;
        }
        output << "PROBE_EXPECTED_MISSING_SELECTED_ROOT=1" << Qt::endl;
        return ExpectedMissingRootExitCode;
    }

    if (!resolved)
    {
        output << "PROBE_RESOLVER_ERROR=" << resolved.error().message << Qt::endl;
        return 20;
    }

    const QString expectedRoot = canonicalDirectory(selectedRoot);
    const QString expectedCatalog = QDir(expectedRoot).filePath(
        QStringLiteral("resource-catalog.json")
        );
    if (expectedRoot.isEmpty() || resolved->resourceRoot != expectedRoot
        || resolved->catalogFilePath != expectedCatalog)
    {
        output << "PROBE_RESOLVED_ROOT=" << resolved->resourceRoot << Qt::endl;
        output << "PROBE_RESOLVED_CATALOG=" << resolved->catalogFilePath << Qt::endl;
        return 21;
    }

    auto catalog = ResourceCatalog::load(resolved->catalogFilePath, resolved->resourceRoot);
    if (!catalog)
    {
        output << "PROBE_CATALOG_ERROR=" << catalog.error().message << Qt::endl;
        return 22;
    }

    const auto resourceId = ResourceId::fromString(
        QStringLiteral("resource:assets/campuses/bundang.json")
        );
    if (!resourceId)
    {
        output << "PROBE_RESOURCE_ID_ERROR=resource ID was rejected" << Qt::endl;
        return 23;
    }
    const ResourceDescriptor* descriptor = catalog->find(*resourceId);
    if (descriptor == nullptr || descriptor->location() != ResourceLocation::PackageFile
        || descriptor->expectedType() != ResourceType::Json
        || !descriptor->expectedBytes().has_value()
        || !descriptor->expectedSha256().has_value())
    {
        output << "PROBE_CATALOG_ENTRY_ERROR=real resource entry or integrity metadata is absent"
               << Qt::endl;
        return 24;
    }
    const qint64 expectedBytes = *descriptor->expectedBytes();
    const QByteArray expectedSha256 = *descriptor->expectedSha256();

    ResourceLoader loader(std::move(*catalog));
    auto request = ResourceRequest::create(
        *resourceId,
        ResourceScopeKind::Startup,
        QStringLiteral("RelocatedResourceRootProbe")
        );
    if (!request)
    {
        output << "PROBE_REQUEST_ERROR=" << request.error().message << Qt::endl;
        return 25;
    }
    auto resource = loader.loadJson(*request);
    if (!resource)
    {
        output << "PROBE_RESOURCE_ERROR=" << resource.error().message << Qt::endl;
        return 26;
    }
    if (!resource->get().isObject() || resource->get().object().isEmpty())
    {
        output << "PROBE_RESOURCE_CONTENT_ERROR=loaded JSON object is empty" << Qt::endl;
        return 27;
    }

    output << "PROBE_CATALOG_ROOT=" << loader.catalog().resourceRoot() << Qt::endl;
    output << "PROBE_RESOURCE_LOADED=" << resourceId->value() << Qt::endl;
    output << "PROBE_RESOURCE_BYTES=" << expectedBytes << Qt::endl;
    output << "PROBE_RESOURCE_SHA256="
           << QString::fromLatin1(expectedSha256) << Qt::endl;
    return 0;
}

struct ProbeRun final
{
    bool started = false;
    bool finished = false;
    QProcess::ExitStatus exitStatus = QProcess::CrashExit;
    int exitCode = -1;
    QByteArray standardOutput;
    QByteArray standardError;
    QString error;
};

ProbeRun launchProbe(
    const QString& executablePath,
    const QString& workingDirectory,
    const QStringList& arguments
    )
{
    QProcess process;
    process.setProgram(executablePath);
    process.setArguments(arguments);
    process.setWorkingDirectory(workingDirectory);
    process.start();

    ProbeRun result;
    result.started = process.waitForStarted(5000);
    if (!result.started)
    {
        result.error = process.errorString();
        return result;
    }
    result.finished = process.waitForFinished(30000);
    if (!result.finished)
    {
        process.kill();
        process.waitForFinished();
        result.error = QStringLiteral("probe did not exit within 30 seconds");
    }
    result.exitStatus = process.exitStatus();
    result.exitCode = process.exitCode();
    result.standardOutput = process.readAllStandardOutput();
    result.standardError = process.readAllStandardError();
    return result;
}

bool hasOutputLine(const QByteArray& output, const QString& expectedLine)
{
    QByteArray normalized = output;
    normalized.replace("\r\n", "\n");
    const QList<QByteArray> lines = normalized.split('\n');
    return lines.contains(expectedLine.toUtf8());
}

bool hasOutputPrefix(const QByteArray& output, const QString& prefix)
{
    QByteArray normalized = output;
    normalized.replace("\r\n", "\n");
    const QByteArray expectedPrefix = prefix.toUtf8();
    for (const QByteArray& line : normalized.split('\n'))
    {
        if (line.startsWith(expectedPrefix) && line.size() > expectedPrefix.size())
        {
            return true;
        }
    }
    return false;
}

bool containsOnlyPackageFiles(const QString& packageRoot)
{
    QStringList files;
    QDirIterator iterator(
        packageRoot,
        QDir::Files | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot,
        QDirIterator::Subdirectories
        );
    while (iterator.hasNext())
    {
        files.append(QDir(packageRoot).relativeFilePath(iterator.next()));
    }
    files.sort();
    return files == QStringList{
        QStringLiteral("assets/campuses/bundang.json"),
        QStringLiteral("resource-catalog.json")
    };
}

bool testRelocatedApplicationDirectoryAndSelectedResource()
{
    if (configuredRelativeRoot() != QStringLiteral("../Resources"))
    {
        return failTest(QStringLiteral(
            "macOS probe was not compiled with the product's expected ../Resources layout"
            ));
    }

    QTemporaryDir temporaryDirectory(
        QDir::tempPath() + QStringLiteral("/ClassMngrNextRelocatedResources-XXXXXX")
        );
    if (!temporaryDirectory.isValid())
    {
        return failTest(QStringLiteral("could not create the positive relocation fixture"));
    }

    const QString applicationDirectory = temporaryDirectory.filePath(
        QStringLiteral("relocated-positive.app/Contents/MacOS")
        );
    const QString executablePath = QDir(applicationDirectory).filePath(
        QStringLiteral("ClassMngrNextResourceProbe")
        );
    if (!copyProbeExecutable(QCoreApplication::applicationFilePath(), executablePath))
    {
        return failTest(QStringLiteral("could not copy the probe executable into the app layout"));
    }

    const QString selectedRoot = selectedRootFor(applicationDirectory);
    const QString expectedSelectedRoot = QDir::cleanPath(
        QDir(temporaryDirectory.path()).filePath(
            QStringLiteral("relocated-positive.app/Contents/Resources")
            )
        );
    if (!stagePackage(selectedRoot))
    {
        return failTest(QStringLiteral("could not stage the generated catalog and selected resource under ")
            + selectedRoot);
    }
    if (canonicalDirectory(selectedRoot) != canonicalDirectory(expectedSelectedRoot))
    {
        return failTest(QStringLiteral("configured resource root did not select Contents/Resources: ")
            + canonicalDirectory(selectedRoot));
    }
    if (!containsOnlyPackageFiles(selectedRoot))
    {
        return failTest(QStringLiteral("positive package contains files beyond its catalog and selected resource"));
    }

    const QString unrelatedWorkingDirectory = temporaryDirectory.filePath(
        QStringLiteral("unrelated-current-directory")
        );
    if (!QDir().mkpath(unrelatedWorkingDirectory))
    {
        return failTest(QStringLiteral("could not create an unrelated current directory"));
    }

    const ProbeRun probe = launchProbe(
        executablePath,
        unrelatedWorkingDirectory,
        {QStringLiteral("--probe")}
        );
    const QString expectedApplicationDirectory = canonicalDirectory(applicationDirectory);
    const QString expectedRoot = canonicalDirectory(selectedRoot);
    if (!probe.started || !probe.finished || probe.exitStatus != QProcess::NormalExit
        || probe.exitCode != 0)
    {
        return failTest(QStringLiteral("positive relocated probe failed: %1\nstdout: %2\nstderr: %3")
            .arg(probe.error,
                QString::fromUtf8(probe.standardOutput),
                QString::fromUtf8(probe.standardError)));
    }

    if (hasOutputLine(
               probe.standardOutput,
               QStringLiteral("PROBE_APPLICATION_DIRECTORY=") + expectedApplicationDirectory
               )
        && hasOutputLine(
            probe.standardOutput,
            QStringLiteral("PROBE_SELECTED_ROOT=") + expectedRoot
            )
        && hasOutputLine(
            probe.standardOutput,
            QStringLiteral("PROBE_CATALOG_ROOT=") + expectedRoot
            )
        && hasOutputLine(
            probe.standardOutput,
            QStringLiteral("PROBE_RESOURCE_LOADED=resource:assets/campuses/bundang.json")
            )
        && hasOutputPrefix(probe.standardOutput, QStringLiteral("PROBE_RESOURCE_BYTES="))
        && hasOutputPrefix(probe.standardOutput, QStringLiteral("PROBE_RESOURCE_SHA256=")))
    {
        return true;
    }
    return failTest(QStringLiteral("positive probe did not report its relocated catalog and loaded resource: %1")
        .arg(QString::fromUtf8(probe.standardOutput)));
}

bool testMissingSelectedRootIgnoresValidDecoyPackages()
{
    QTemporaryDir temporaryDirectory(
        QDir::tempPath() + QStringLiteral("/ClassMngrNextMissingResources-XXXXXX")
        );
    if (!temporaryDirectory.isValid())
    {
        return failTest(QStringLiteral("could not create the negative relocation fixture"));
    }

    const QString applicationDirectory = temporaryDirectory.filePath(
        QStringLiteral("relocated-negative.app/Contents/MacOS")
        );
    const QString executablePath = QDir(applicationDirectory).filePath(
        QStringLiteral("ClassMngrNextResourceProbe")
        );
    if (!copyProbeExecutable(QCoreApplication::applicationFilePath(), executablePath))
    {
        return failTest(QStringLiteral("could not copy the probe executable into the negative app layout"));
    }

    const QString selectedRoot = selectedRootFor(applicationDirectory);
    const QString unrelatedWorkingDirectory = temporaryDirectory.filePath(
        QStringLiteral("unrelated-current-directory")
        );
    if (!QDir().mkpath(unrelatedWorkingDirectory))
    {
        return failTest(QStringLiteral("could not create the unrelated negative-test current directory"));
    }

    const QStringList decoyRoots{
        unrelatedWorkingDirectory,
        QDir(unrelatedWorkingDirectory).filePath(QStringLiteral("resources")),
        QDir(unrelatedWorkingDirectory).filePath(QStringLiteral("Resources")),
        applicationDirectory,
        QDir(applicationDirectory).filePath(QStringLiteral("resources")),
        QDir(applicationDirectory).filePath(QStringLiteral("Resources")),
        QDir(applicationDirectory).filePath(QStringLiteral("../../Resources"))
    };
    for (const QString& decoyRoot : decoyRoots)
    {
        if (QDir::cleanPath(decoyRoot) == QDir::cleanPath(selectedRoot)
            || !stagePackage(decoyRoot))
        {
            return failTest(QStringLiteral("could not stage a valid decoy package"));
        }
    }
    if (QFileInfo::exists(selectedRoot))
    {
        return failTest(QStringLiteral("negative fixture accidentally created the selected root"));
    }

    const ProbeRun probe = launchProbe(
        executablePath,
        unrelatedWorkingDirectory,
        {QStringLiteral("--expect-missing-root")}
        );
    const QString expectedApplicationDirectory = canonicalDirectory(applicationDirectory);
    const QString expectedRoot = selectedRootFor(expectedApplicationDirectory);
    if (!probe.started || !probe.finished || probe.exitStatus != QProcess::NormalExit
        || probe.exitCode != ExpectedMissingRootExitCode)
    {
        return failTest(QStringLiteral("negative relocated probe did not reject the missing selected root: %1\nstdout: %2\nstderr: %3")
            .arg(probe.error,
                QString::fromUtf8(probe.standardOutput),
                QString::fromUtf8(probe.standardError)));
    }

    if (hasOutputLine(
               probe.standardOutput,
               QStringLiteral("PROBE_APPLICATION_DIRECTORY=") + expectedApplicationDirectory
               )
        && hasOutputLine(
            probe.standardOutput,
            QStringLiteral("PROBE_SELECTED_ROOT=") + expectedRoot
            )
        && hasOutputLine(
            probe.standardOutput,
            QStringLiteral("PROBE_EXPECTED_MISSING_SELECTED_ROOT=1")
            ))
    {
        return true;
    }
    return failTest(QStringLiteral("negative probe did not report the missing app-relative root: %1")
        .arg(QString::fromUtf8(probe.standardOutput)));
}

}

int main(int argc, char* argv[])
{
    QCoreApplication application(argc, argv);
    const QStringList arguments = application.arguments();
    if (arguments.size() == 2 && arguments.at(1) == QStringLiteral("--expect-missing-root"))
    {
        return runProbe(true);
    }
    if (arguments.size() == 2 && arguments.at(1) == QStringLiteral("--probe"))
    {
        return runProbe(false);
    }
    if (arguments.size() == 1)
    {
        if (!testRelocatedApplicationDirectoryAndSelectedResource()
            || !testMissingSelectedRootIgnoresValidDecoyPackages())
        {
            return 1;
        }
        return 0;
    }
    return fail(QStringLiteral("unexpected relocated resource probe arguments"));
}
