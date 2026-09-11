#include "classmngr/engine/document_output.h"

#include <iostream>
#include <string>
#include <string_view>
#include <type_traits>

namespace
{
using namespace classmngr::engine;

bool expect(bool condition, std::string_view message)
{
    if (condition)
    {
        return true;
    }

    std::cerr << "ClassMngrEngineDocumentOutputResultTests: "
              << message
              << '\n';
    return false;
}
} // namespace

static_assert(
    static_cast<int>(DocumentOutputStatus::Sent)
        == static_cast<int>(DocumentOutputStatus::Completed)
    );
static_assert(std::is_default_constructible_v<DocumentOutputResult>);
static_assert(std::is_copy_constructible_v<DocumentOutputResult>);
static_assert(std::is_copy_assignable_v<DocumentOutputResult>);
static_assert(std::is_move_constructible_v<DocumentOutputResult>);
static_assert(std::is_move_assignable_v<DocumentOutputResult>);

int main()
{
    bool passed = true;

    const DocumentOutputResult defaultResult;
    passed &= expect(
        defaultResult.status == DocumentOutputStatus::Failed,
        "default status was not Failed"
        );
    passed &= expect(
        !defaultResult.succeeded(),
        "default result unexpectedly succeeded"
        );

    DocumentOutputResult completed;
    completed.status = DocumentOutputStatus::Completed;
    passed &= expect(
        completed.succeeded(),
        "Completed result did not succeed"
        );

    DocumentOutputResult sent;
    sent.status = DocumentOutputStatus::Sent;
    passed &= expect(
        sent.succeeded(),
        "Sent result did not succeed"
        );

    for (const DocumentOutputStatus status : {
             DocumentOutputStatus::Canceled,
             DocumentOutputStatus::Failed,
             DocumentOutputStatus::InternalRendererFailed
         })
    {
        DocumentOutputResult failed;
        failed.status = status;
        passed &= expect(
            !failed.succeeded(),
            "a failure status unexpectedly succeeded"
            );
    }

    const std::string message =
        "\xEC\xB6\x9C\xEB\xA0\xA5 \xEC\x8B\xA4\xED\x8C\xA8 \xE2\x80\x94 \xE6\x97\xA5\xE6\x9C\xAC\xE8\xAA\x9E";
    const std::string firstPdfPath =
        "reports/\xEB\xB3\xB4\xEA\xB3\xA0-1.pdf";
    const std::string secondPdfPath =
        "reports/\xEB\xB3\xB4\xEA\xB3\xA0-2.pdf";
    const std::string archivePath =
        "reports/\xE6\x9C\x88\xE6\x9C\xAB-archive.zip";

    DocumentOutputResult populated;
    populated.status = DocumentOutputStatus::Completed;
    populated.message = message;
    populated.savedPdfPaths = {firstPdfPath, secondPdfPath};
    populated.savedArchivePath = archivePath;
    passed &= expect(
        populated.succeeded()
            && populated.message == message
            && populated.savedArchivePath == archivePath,
        "direct result fields or UTF-8 text were not preserved"
        );
    passed &= expect(
        populated.savedPdfPaths.size() == 2
            && populated.savedPdfPaths[0] == firstPdfPath
            && populated.savedPdfPaths[1] == secondPdfPath,
        "PDF paths were not preserved in order"
        );

    return passed ? 0 : 1;
}
