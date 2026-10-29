#include "next/ui/document_viewer_route.h"

#include "next/ui/application_shell.h"
#include "next/ui/document_viewer_page.h"

#include <QCoreApplication>
#include <QDebug>

#include <memory>

namespace ClassMngr::Next::Ui
{

PageHostResult<void> registerDocumentViewerPage(PageHost& pageHost)
{
    const auto pageId = PageId::fromString(
        QString::fromLatin1(kDocumentViewerPageKey)
        );
    if (!pageId)
    {
        return std::unexpected(PageHostError{
            PageHostErrorCode::InvalidPageDescriptor,
            QString::fromLatin1(kDocumentViewerPageKey),
            QStringLiteral("the document viewer page key is invalid")
        });
    }

    return pageHost.registerPage(PageDescriptor{
        .id = *pageId,
        .label = QCoreApplication::translate("MenuBuilder", "Documents"),
        .iconResourceId = std::nullopt,
        .requiredPermissions = {},
        .factory = [](const PageContext& context)
            -> PageOperationResult<std::unique_ptr<Page>> {
            std::unique_ptr<Page> page =
                std::make_unique<DocumentViewerPage>(context);
            return std::move(page);
        },
        .resourcePolicy = PageResourcePolicy::PageScoped,
        .initialDataPolicy = PageInitialDataPolicy::None
    });
}

void bindDocumentViewerSelection(ApplicationShell& shell)
{
    ApplicationShell* const shellPointer = &shell;
    shell.setDocumentCatalogSelectionHandler(
        [shellPointer](const Domain::DocumentId& documentId) {
            const PageHostResult<void> activation =
                shellPointer->pageHost().activatePage(
                    QString::fromLatin1(kDocumentViewerPageKey)
                    );
            if (!activation)
            {
                qWarning().noquote()
                    << "Could not activate the Next document viewer:"
                    << activation.error().message;
                return;
            }

            auto* const viewer = dynamic_cast<DocumentViewerPage*>(
                shellPointer->pageHost().pageInstance(
                    QString::fromLatin1(kDocumentViewerPageKey)
                    )
                );
            const Application::DocumentCatalogProjection* const projection =
                shellPointer->documentCatalogProjection();
            if (!viewer || !projection)
            {
                qWarning("The Next document viewer route is missing its active page or catalog.");
                return;
            }
            static_cast<void>(viewer->openDocument(documentId, *projection));
        }
        );
    shell.setDocumentCatalogSelectionClearedHandler([shellPointer] {
        auto* const viewer = dynamic_cast<DocumentViewerPage*>(
            shellPointer->pageHost().pageInstance(
                QString::fromLatin1(kDocumentViewerPageKey)
                )
            );
        if (viewer)
        {
            static_cast<void>(viewer->closeDocument());
        }
    });
}

}  // namespace ClassMngr::Next::Ui
