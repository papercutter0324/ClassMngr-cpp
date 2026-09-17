#include <QCoreApplication>
#include <QString>

#include <cstdio>

int main(int argc, char *argv[])
{
    QCoreApplication application(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("ClassMngrNext"));
    QCoreApplication::setApplicationVersion(
        QStringLiteral(CLASSMNGR_NEXT_VERSION));

    const auto version = QCoreApplication::applicationVersion().toUtf8();
    std::printf("ClassMngrNext launch version=%s\n", version.constData());

    return 0;
}
