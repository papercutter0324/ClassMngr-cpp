#pragma once

#include <string>
#include <vector>

namespace classmngr::engine
{

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
    std::string message;
    std::vector<std::string> savedPdfPaths;
    std::string savedArchivePath;

    [[nodiscard]] bool succeeded() const noexcept
    {
        return status == DocumentOutputStatus::Completed;
    }
};

} // namespace classmngr::engine
