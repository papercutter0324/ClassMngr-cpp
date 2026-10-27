#pragma once

#include "next/application/page_changes_port.h"
#include "next/ui/page_host.h"

namespace ClassMngr::Next::Ui
{

// Adapts the host-owned active page to the application dirty-resolution
// contract without exposing widget types to Application.
class PageHostActivePageChangesAdapter final
    : public Application::ActivePageChangesPort
{
public:
    explicit PageHostActivePageChangesAdapter(PageHost& pageHost) noexcept
        : m_pageHost(pageHost)
    {
    }

    [[nodiscard]] bool hasUnsavedChanges() const noexcept override
    {
        Page* const page = activePage();
        return page && page->hasUnsavedChanges();
    }

    [[nodiscard]] Domain::Result<void> saveChanges() override
    {
        Page* const page = activePage();
        if (!page)
        {
            return Domain::Result<void>::success();
        }

        const PageOperationResult<void> result = page->saveChanges();
        if (!result)
        {
            return Domain::Result<void>::failure(operationError(result.error().message));
        }
        if (page->hasUnsavedChanges())
        {
            return Domain::Result<void>::failure(operationError(
                QStringLiteral("The active page remained dirty after saving.")
                ));
        }
        return Domain::Result<void>::success();
    }

    [[nodiscard]] Domain::Result<void> discardChanges() override
    {
        Page* const page = activePage();
        if (!page)
        {
            return Domain::Result<void>::success();
        }

        const PageOperationResult<void> result = page->discardChanges();
        if (!result)
        {
            return Domain::Result<void>::failure(operationError(result.error().message));
        }
        if (page->hasUnsavedChanges())
        {
            return Domain::Result<void>::failure(operationError(
                QStringLiteral("The active page remained dirty after discarding.")
                ));
        }
        return Domain::Result<void>::success();
    }

private:
    [[nodiscard]] Page* activePage() const noexcept
    {
        const std::optional<QString> key = m_pageHost.activePageKey();
        return key ? m_pageHost.pageInstance(*key) : nullptr;
    }

    [[nodiscard]] static Domain::OperationError operationError(
        const QString& message
        )
    {
        return {
            .code = Domain::ErrorCode::Technical,
            .message = message.toStdString(),
            .recoverable = true
        };
    }

    PageHost& m_pageHost;
};

} // namespace ClassMngr::Next::Ui
