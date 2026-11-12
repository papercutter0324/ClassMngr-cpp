#include "next/platform/qt_update_package_action_port.h"

#include <QDesktopServices>
#include <QFile>
#include <QFileInfo>
#include <QList>
#include <QObject>
#include <QTemporaryDir>
#include <QUrl>
#include <QtGlobal>
#include <QtTest/QtTest>

#include <string>

using namespace ClassMngr::Next;

namespace
{

class CapturedFileUrlHandler final : public QObject
{
    Q_OBJECT

public:
    QList<QUrl> urls;

public slots:
    void capture(const QUrl& url)
    {
        urls.append(url);
    }
};

class FileUrlHandlerRegistration final
{
public:
    explicit FileUrlHandlerRegistration(CapturedFileUrlHandler* receiver)
    {
        QDesktopServices::setUrlHandler(
            QStringLiteral("file"),
            receiver,
            "capture"
            );
    }

    ~FileUrlHandlerRegistration()
    {
        QDesktopServices::unsetUrlHandler(QStringLiteral("file"));
    }

    FileUrlHandlerRegistration(const FileUrlHandlerRegistration&) = delete;
    FileUrlHandlerRegistration& operator=(
        const FileUrlHandlerRegistration&
        ) = delete;
};

} // namespace

class NextPlatformLinuxUpdatePackageActionPortTests final : public QObject
{
    Q_OBJECT

private slots:
    void rejectsMissingPackageWithoutOpeningFolder();
    void revealsExistingPackageParentFolder();
};

void NextPlatformLinuxUpdatePackageActionPortTests::
rejectsMissingPackageWithoutOpeningFolder()
{
#if !defined(Q_OS_LINUX)
    QSKIP("The package action expectation covers Linux behavior only.");
#else
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());

    CapturedFileUrlHandler handler;
    FileUrlHandlerRegistration registration(&handler);
    Platform::QtUpdatePackageActionPort port;

    QCOMPARE(
        port.primaryAction(),
        Application::UpdatePackageActionKind::RevealInFolder
        );

    const QString missingPackagePath = temporaryDirectory.filePath(
        QStringLiteral("missing-update-package")
        );
    const Application::UpdatePackageActionResult result =
        port.revealInFolder(missingPackagePath.toStdString());

    QVERIFY(!result.succeeded);
    QVERIFY(!result.message.empty());
    QVERIFY(handler.urls.isEmpty());
#endif
}

void NextPlatformLinuxUpdatePackageActionPortTests::
revealsExistingPackageParentFolder()
{
#if !defined(Q_OS_LINUX)
    QSKIP("The package action expectation covers Linux behavior only.");
#else
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());

    const QString packagePath = temporaryDirectory.filePath(
        QStringLiteral("update-package.deb")
        );
    QFile package(packagePath);
    QVERIFY(package.open(QIODevice::WriteOnly));
    QCOMPARE(package.write(QByteArrayLiteral("package fixture")), qint64(15));
    package.close();

    CapturedFileUrlHandler handler;
    FileUrlHandlerRegistration registration(&handler);
    Platform::QtUpdatePackageActionPort port;

    const Application::UpdatePackageActionResult result =
        port.revealInFolder(packagePath.toStdString());

    QVERIFY2(result.succeeded, result.message.c_str());
    QCOMPARE(handler.urls.size(), 1);
    QCOMPARE(
        handler.urls.constFirst(),
        QUrl::fromLocalFile(QFileInfo(packagePath).absolutePath())
        );
#endif
}

QTEST_MAIN(NextPlatformLinuxUpdatePackageActionPortTests)

#include "next_platform_linux_update_package_action_port_tests.moc"
