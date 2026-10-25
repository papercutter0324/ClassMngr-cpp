#include "next/resources/resource_root_resolver.h"

#include <QCoreApplication>
#include <QFileInfo>
#include <QStringList>

#include <iostream>

using namespace ClassMngr::Next::Resources;

int main(int argc, char* argv[])
{
    QCoreApplication application(argc, argv);
    const QStringList arguments = application.arguments();
    if (arguments.size() != 2)
    {
        std::cerr << "usage: ClassMngrNextResourceRootLayoutProbe <expected-resource-root>\n";
        return 2;
    }

    const QString expectedRoot = QFileInfo(arguments.at(1)).canonicalFilePath();
    if (expectedRoot.isEmpty())
    {
        std::cerr << "expected resource root does not exist: "
                  << arguments.at(1).toStdString() << '\n';
        return 1;
    }

    const auto resolved = ResourceRootResolver::resolve();
    if (!resolved)
    {
        std::cerr << "resource root resolution failed: "
                  << resolved.error().message.toStdString() << '\n';
        return 1;
    }

    if (resolved->resourceRoot != expectedRoot)
    {
        std::cerr << "resolver selected " << resolved->resourceRoot.toStdString()
                  << " but the installed component is at "
                  << expectedRoot.toStdString() << '\n';
        return 1;
    }

    std::cout << "configured_relative_root="
              << QStringLiteral(CLASSMNGR_NEXT_RESOURCE_RELATIVE_ROOT).toStdString()
              << '\n'
              << "resolved_resource_root=" << resolved->resourceRoot.toStdString()
              << '\n';
    return 0;
}
