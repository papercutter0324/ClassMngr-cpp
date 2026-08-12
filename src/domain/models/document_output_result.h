#pragma once

#include <QString>
#include <QStringList>

enum class DocumentOutputStatus
{
    Completed,
    Sent = Completed,
    Canceled,
    Failed,
    InternalRendererFailed
};

struct DocumentOutputResult
{
    DocumentOutputStatus status = DocumentOutputStatus::Failed;
    QString message;
    QStringList savedPdfPaths;
    QString savedArchivePath;

    [[nodiscard]] bool succeeded() const
    {
        return status == DocumentOutputStatus::Completed;
    }
};
