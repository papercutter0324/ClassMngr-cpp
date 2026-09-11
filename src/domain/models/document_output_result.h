#pragma once

#include "classmngr/engine/document_output.h"

#include <QString>
#include <QStringList>

#include <cstddef>
#include <string>

using DocumentOutputStatus = classmngr::engine::DocumentOutputStatus;

struct DocumentOutputResult
{
    DocumentOutputStatus status = DocumentOutputStatus::Failed;
    QString message;
    QStringList savedPdfPaths;
    QString savedArchivePath;

    [[nodiscard]] classmngr::engine::DocumentOutputResult toEngine() const
    {
        classmngr::engine::DocumentOutputResult result;
        result.status = status;
        result.message = message.toStdString();
        result.savedPdfPaths.reserve(
            static_cast<std::size_t>(savedPdfPaths.size())
            );
        for (const QString& path : savedPdfPaths)
        {
            result.savedPdfPaths.push_back(path.toStdString());
        }
        result.savedArchivePath = savedArchivePath.toStdString();
        return result;
    }

    [[nodiscard]] static DocumentOutputResult fromEngine(
        const classmngr::engine::DocumentOutputResult& source
        )
    {
        DocumentOutputResult result;
        result.status = source.status;
        result.message = QString::fromUtf8(
            source.message.data(),
            static_cast<qsizetype>(source.message.size())
            );
        result.savedPdfPaths.reserve(
            static_cast<qsizetype>(source.savedPdfPaths.size())
            );
        for (const std::string& path : source.savedPdfPaths)
        {
            result.savedPdfPaths.append(QString::fromUtf8(
                path.data(),
                static_cast<qsizetype>(path.size())
                ));
        }
        result.savedArchivePath = QString::fromUtf8(
            source.savedArchivePath.data(),
            static_cast<qsizetype>(source.savedArchivePath.size())
            );
        return result;
    }

    [[nodiscard]] bool succeeded() const
    {
        return toEngine().succeeded();
    }
};
