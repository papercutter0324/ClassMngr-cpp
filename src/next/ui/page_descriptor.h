#pragma once

#include "next/resources/resource_id.h"
#include "next/ui/page.h"

#include <QString>
#include <QStringList>

#include <functional>
#include <memory>
#include <optional>
#include <utility>

namespace ClassMngr::Next::Ui
{

class PageId final
{
public:
    PageId(const PageId&) = default;
    PageId(PageId&&) noexcept = default;
    PageId& operator=(const PageId&) = default;
    PageId& operator=(PageId&&) noexcept = default;

    [[nodiscard]] static std::optional<PageId> fromString(QString value)
    {
        value = value.trimmed();
        if (value.isEmpty())
        {
            return std::nullopt;
        }

        return PageId(std::move(value));
    }

    [[nodiscard]] const QString& value() const noexcept
    {
        return m_value;
    }

    friend bool operator==(const PageId&, const PageId&) = default;

private:
    explicit PageId(QString value)
        : m_value(std::move(value))
    {
    }

    QString m_value;
};

// Describes the intended scope for page-owned resources. PageHost currently
// creates a page scope for every page and retains this value as metadata.
enum class PageResourcePolicy
{
    PageScoped
};

// Declares whether a page expects navigation to provide initial data. This
// value is metadata only; the current PageHost does not supply initial data.
enum class PageInitialDataPolicy
{
    None
};

using PageFactory = std::function<
    PageOperationResult<std::unique_ptr<Page>>(const PageContext&)>;

struct PageDescriptor final
{
    PageId id;
    QString label;
    // Source text for retranslation when the active Qt language changes.
    // Empty labels remain literal, such as names loaded from user data.
    QString labelTranslationSource;
    // Absent while the page has no icon in the resource catalog.
    std::optional<Resources::ResourceId> iconResourceId;
    // Stable permission identifiers; PageHost stores but does not enforce them.
    QStringList requiredPermissions;
    PageFactory factory;
    PageResourcePolicy resourcePolicy = PageResourcePolicy::PageScoped;
    PageInitialDataPolicy initialDataPolicy = PageInitialDataPolicy::None;
};

}  // namespace ClassMngr::Next::Ui
