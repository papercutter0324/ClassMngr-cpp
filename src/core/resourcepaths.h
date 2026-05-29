#ifndef RESOURCEPATHS_H
#define RESOURCEPATHS_H

#include <QString>
#include <QUrl>

namespace ResourcePaths
{
// =====================================================
// Asset Accessors
// =====================================================

QString image(const QString& name);
QString templateFile(const QString& name);
QString style(const QString& name);

// =====================================================
// WebEngine / QML base URL
// =====================================================

QUrl baseUrl();
}

#endif // RESOURCEPATHS_H