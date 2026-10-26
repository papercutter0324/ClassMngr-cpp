#pragma once

#include "next/resources/resource_loader.h"
#include "next/resources/resource_scope.h"

#include <QWidget>

#include <expected>
#include <memory>
#include <utility>

namespace ClassMngr::Next::Ui
{

class PageHost;

struct PageOperationError final
{
    QString message;
};

template <typename Value>
using PageOperationResult = std::expected<Value, PageOperationError>;

class PageContext final
{
public:
    [[nodiscard]] Resources::ResourceLoader& resourceLoader() const noexcept
    {
        return *m_resourceLoader;
    }

    [[nodiscard]] const Resources::ResourceScope& pageScope() const noexcept
    {
        return m_pageScope;
    }

    [[nodiscard]] const QString& pageKey() const noexcept
    {
        return m_pageKey;
    }

private:
    friend class PageHost;

    PageContext(
        Resources::ResourceLoader& resourceLoader,
        Resources::ResourceScope pageScope,
        QString pageKey
        )
        : m_resourceLoader(&resourceLoader)
        , m_pageScope(std::move(pageScope))
        , m_pageKey(std::move(pageKey))
    {
    }

    Resources::ResourceLoader* m_resourceLoader;
    Resources::ResourceScope m_pageScope;
    QString m_pageKey;
};

class Page : public QWidget
{
public:
    explicit Page(const PageContext& context, QWidget* parent = nullptr)
        : QWidget(parent)
        , m_resourceContext(context)
    {
    }

    ~Page() override = default;

    [[nodiscard]] const PageContext& resourceContext() const noexcept
    {
        return m_resourceContext;
    }

    // This runs before the host commits the page switch, so a failure leaves
    // the previous active page untouched.
    [[nodiscard]] virtual PageOperationResult<void> onActivating()
    {
        return {};
    }

    // Resource scopes label requests only. Pages own their scoped handles;
    // clear feature-scoped handles here while retaining page-scoped handles
    // until this page is released.
    virtual void onSuspending() noexcept
    {
    }

private:
    PageContext m_resourceContext;
};

}  // namespace ClassMngr::Next::Ui
