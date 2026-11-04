#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QThread>
#include <QStringList>
#include <cstdio>

namespace
{
int runPowerPointProcessFixture(const QStringList& arguments)
{
    if (arguments.size() != 8) return 2;
    const QString mode = arguments.at(2);
    if (mode == QStringLiteral("nonzero"))
    {
        QFile diagnostic;
        if (diagnostic.open(stderr, QIODevice::WriteOnly)) diagnostic.write("fixture process diagnostic");
        return 7;
    }
    if (mode == QStringLiteral("timeout"))
    {
        QThread::msleep(5000);
        return 0;
    }
    if (mode == QStringLiteral("missing-pdf")) return 0;
    for (int index = 0; index < 2; ++index)
    {
        QFile pdf(arguments.at(4 + index * 2));
        if (!pdf.open(QIODevice::WriteOnly)) return 3;
        if (mode != QStringLiteral("empty-pdf")) pdf.write("%PDF-process-fixture");
        pdf.close();
        QFile marker(arguments.at(5 + index * 2));
        if (!marker.open(QIODevice::WriteOnly)) return 4;
        marker.close();
        if (mode == QStringLiteral("canceled"))
        {
            for (int attempt = 0; attempt < 100; ++attempt)
            {
                if (QFileInfo::exists(arguments.at(3))) return 0;
                QThread::msleep(50);
            }
            return 5;
        }
    }
    return 0;
}
}

int main(int argc, char** argv)
{
    QCoreApplication application(argc, argv);
    return runPowerPointProcessFixture(application.arguments());
}
