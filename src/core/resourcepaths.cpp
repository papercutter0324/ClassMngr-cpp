#include "resourcepaths.h"

#include <QUrl>

namespace
{
// Central prefix for all resources
constexpr auto ROOT = ":/assets/";
}

QString ResourcePaths::image(const QString& name)
{
    return QString(ROOT) + "images/" + name;
}

QString ResourcePaths::templateFile(const QString& name)
{
    return QString(ROOT) + "templates/" + name;
}

QString ResourcePaths::style(const QString& name)
{
    return QString(ROOT) + "styles/" + name;
}

QUrl ResourcePaths::baseUrl()
{
    // Qt resource URL form (important for WebEngine)
    return QUrl("qrc:/assets/");
}