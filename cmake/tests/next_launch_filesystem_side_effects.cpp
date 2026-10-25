#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QProcess>
#include <QProcessEnvironment>
#include <QStringList>
#include <QTemporaryDir>

#include <cstdio>

namespace {

int fail(const QString& message)
{
    const QByteArray utf8 = message.toUtf8();
    std::fprintf(stderr, "%s\n", utf8.constData());
    return 1;
}

bool hasEntries(const QString& directoryPath, QString* firstEntry)
{
    QDirIterator iterator(
        directoryPath,
        QDir::AllEntries | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot,
        QDirIterator::Subdirectories
    );
    if (!iterator.hasNext()) {
        return false;
    }

    if (firstEntry != nullptr) {
        *firstEntry = iterator.next();
    }
    return true;
}

}  // namespace

int main(int argc, char* argv[])
{
    if (argc != 3) {
        return fail(QStringLiteral(
            "usage: ClassMngrNextLaunchFilesystemSideEffectsTests <app> <version>"
        ));
    }

    const QString applicationPath = QString::fromLocal8Bit(argv[1]);
    const QString version = QString::fromLocal8Bit(argv[2]);
    QTemporaryDir isolatedRoot(
        QDir::tempPath() + QStringLiteral("/ClassMngrNextLaunch-XXXXXX")
    );
    if (!isolatedRoot.isValid()) {
        return fail(QStringLiteral("Unable to create isolated launch-test directory."));
    }

    const QString workingDirectory = isolatedRoot.filePath(QStringLiteral("working"));
    if (!QDir().mkpath(workingDirectory)) {
        return fail(QStringLiteral("Unable to create isolated working directory."));
    }

    const QString home = isolatedRoot.filePath(QStringLiteral("home"));
    const QString config = isolatedRoot.filePath(QStringLiteral("config"));
    const QString data = isolatedRoot.filePath(QStringLiteral("data"));
    const QString cache = isolatedRoot.filePath(QStringLiteral("cache"));
    const QString state = isolatedRoot.filePath(QStringLiteral("state"));

    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    environment.insert(QStringLiteral("HOME"), home);
    environment.insert(QStringLiteral("XDG_CONFIG_HOME"), config);
    environment.insert(QStringLiteral("XDG_DATA_HOME"), data);
    environment.insert(QStringLiteral("XDG_CACHE_HOME"), cache);
    environment.insert(QStringLiteral("XDG_STATE_HOME"), state);
#ifdef Q_OS_WIN
    environment.insert(QStringLiteral("USERPROFILE"), home);
    environment.insert(QStringLiteral("APPDATA"), config);
    environment.insert(QStringLiteral("LOCALAPPDATA"), data);
#endif

    QProcess process;
    process.setProgram(applicationPath);
    process.setWorkingDirectory(workingDirectory);
    process.setProcessEnvironment(environment);
    process.start();
    if (!process.waitForStarted(5000)) {
        return fail(QStringLiteral("ClassMngrNext did not start: %1")
            .arg(process.errorString()));
    }
    if (!process.waitForFinished(15000)) {
        process.kill();
        process.waitForFinished();
        return fail(QStringLiteral("ClassMngrNext did not exit within 15 seconds."));
    }

    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        return fail(QStringLiteral("ClassMngrNext exited abnormally (status=%1, code=%2).")
            .arg(static_cast<int>(process.exitStatus()))
            .arg(process.exitCode()));
    }

    const QByteArray expectedOutput = QStringLiteral(
        "ClassMngrNext launch version=%1\n"
    ).arg(version).toUtf8();
    QByteArray standardOutput = process.readAllStandardOutput();
    standardOutput.replace("\r\n", "\n");
    if (standardOutput != expectedOutput) {
        return fail(QStringLiteral("Unexpected ClassMngrNext output: '%1'.")
            .arg(QString::fromUtf8(standardOutput)));
    }

    const QStringList isolatedStorageRoots{home, config, data, cache, state};
    for (const QString& path : isolatedStorageRoots) {
        if (QFileInfo::exists(path)) {
            return fail(QStringLiteral(
                "ClassMngrNext created an isolated settings/storage root: %1"
            ).arg(path));
        }
    }

    QString firstWorkingEntry;
    if (hasEntries(workingDirectory, &firstWorkingEntry)) {
        return fail(QStringLiteral(
            "ClassMngrNext created an entry in its isolated working directory: %1"
        ).arg(firstWorkingEntry));
    }

    return 0;
}
