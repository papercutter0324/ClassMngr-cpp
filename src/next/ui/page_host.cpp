#include "next/ui/page_host.h"

#include <QStackedWidget>
#include <QVBoxLayout>

#include <exception>
#include <utility>

namespace ClassMngr::Next::Ui
{
namespace
{

class TransitionGuard final
{
public:
    explicit TransitionGuard(bool& transitionInProgress) noexcept
        : m_transitionInProgress(transitionInProgress)
    {
        m_transitionInProgress = true;
    }

    ~TransitionGuard()
    {
        m_transitionInProgress = false;
    }

    TransitionGuard(const TransitionGuard&) = delete;
    TransitionGuard& operator=(const TransitionGuard&) = delete;

private:
    bool& m_transitionInProgress;
};

QString callbackErrorMessage(const QString& message, const QString& fallback)
{
    const QString trimmed = message.trimmed();
    return trimmed.isEmpty() ? fallback : trimmed;
}

}

PageHost::PageHost(
    Resources::ResourceLoader& resourceLoader,
    QWidget* parent
    )
    : QWidget(parent)
    , m_resourceLoader(resourceLoader)
    , m_stack(new QStackedWidget(this))
    , m_emptyPage(new QWidget(m_stack))
{
    setObjectName(QStringLiteral("classMngrNextPageHost"));
    m_emptyPage->setObjectName(QStringLiteral("classMngrNextPageHostEmpty"));
    m_stack->addWidget(m_emptyPage);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_stack);
}

PageHost::~PageHost()
{
    if (m_activePageKey.has_value())
    {
        const auto active = m_pages.find(*m_activePageKey);
        if (active != m_pages.end() && active->page)
        {
            active->page->onSuspending();
        }
    }
}

PageHostResult<void> PageHost::registerPage(
    QString pageKey,
    PageFactory factory
    )
{
    const auto normalizedKey = normalizedPageKey(pageKey);
    if (!normalizedKey)
    {
        return std::unexpected(error(
            PageHostErrorCode::InvalidPageKey,
            pageKey,
            QStringLiteral("page key must not be empty")
            ));
    }
    if (!factory)
    {
        return std::unexpected(error(
            PageHostErrorCode::InvalidFactory,
            *normalizedKey,
            QStringLiteral("page factory is empty")
            ));
    }
    if (m_transitionInProgress)
    {
        return std::unexpected(error(
            PageHostErrorCode::ReentrantOperation,
            *normalizedKey,
            QStringLiteral("page registration is unavailable during a lifecycle callback")
            ));
    }
    if (m_pages.contains(*normalizedKey))
    {
        return std::unexpected(error(
            PageHostErrorCode::DuplicatePageKey,
            *normalizedKey,
            QStringLiteral("a page factory is already registered for this key")
            ));
    }

    m_pages.insert(*normalizedKey, PageEntry{std::move(factory), nullptr});
    return {};
}

PageHostResult<void> PageHost::activatePage(const QString& pageKey)
{
    const auto normalizedKey = normalizedPageKey(pageKey);
    if (!normalizedKey)
    {
        return std::unexpected(error(
            PageHostErrorCode::InvalidPageKey,
            pageKey,
            QStringLiteral("page key must not be empty")
            ));
    }
    if (m_transitionInProgress)
    {
        return std::unexpected(error(
            PageHostErrorCode::ReentrantOperation,
            *normalizedKey,
            QStringLiteral("page activation is unavailable during a lifecycle callback")
            ));
    }

    auto targetEntry = m_pages.find(*normalizedKey);
    if (targetEntry == m_pages.end())
    {
        return std::unexpected(error(
            PageHostErrorCode::PageNotRegistered,
            *normalizedKey,
            QStringLiteral("no page factory is registered for this key")
            ));
    }
    if (m_activePageKey == normalizedKey)
    {
        return {};
    }

    TransitionGuard transition(m_transitionInProgress);
    PageEntry& entry = targetEntry.value();
    std::unique_ptr<Page> newlyCreatedPage;
    Page* targetPage = entry.page;

    if (!targetPage)
    {
        auto pageScope = Resources::ResourceScope::create(
            Resources::ResourceScopeKind::Page,
            *normalizedKey
            );
        if (!pageScope)
        {
            return std::unexpected(error(
                PageHostErrorCode::ResourceScopeCreationFailed,
                *normalizedKey,
                pageScope.error().message
                ));
        }

        const PageContext context(
            m_resourceLoader,
            std::move(*pageScope),
            *normalizedKey
            );
        PageOperationResult<std::unique_ptr<Page>> factoryResult =
            std::unexpected(PageOperationError{
                QStringLiteral("page factory did not return a result")
            });
        try
        {
            factoryResult = entry.factory(context);
        }
        catch (const std::exception& exception)
        {
            return std::unexpected(error(
                PageHostErrorCode::FactoryFailed,
                *normalizedKey,
                QString::fromUtf8(exception.what())
                ));
        }
        catch (...)
        {
            return std::unexpected(error(
                PageHostErrorCode::FactoryFailed,
                *normalizedKey,
                QStringLiteral("page factory threw an unknown exception")
                ));
        }

        if (!factoryResult)
        {
            return std::unexpected(error(
                PageHostErrorCode::FactoryFailed,
                *normalizedKey,
                callbackErrorMessage(
                    factoryResult.error().message,
                    QStringLiteral("page factory failed")
                    )
                ));
        }
        newlyCreatedPage = std::move(*factoryResult);
        if (!newlyCreatedPage)
        {
            return std::unexpected(error(
                PageHostErrorCode::FactoryReturnedNull,
                *normalizedKey,
                QStringLiteral("page factory returned no page")
                ));
        }
        if (newlyCreatedPage->parentWidget())
        {
            return std::unexpected(error(
                PageHostErrorCode::PageAlreadyParented,
                *normalizedKey,
                QStringLiteral("page factory must return an unparented page")
                ));
        }
        targetPage = newlyCreatedPage.get();
    }

    PageOperationResult<void> activationResult = std::unexpected(
        PageOperationError{QStringLiteral("page activation did not return a result")}
        );
    try
    {
        activationResult = targetPage->onActivating();
    }
    catch (const std::exception& exception)
    {
        activationResult = std::unexpected(PageOperationError{
            QString::fromUtf8(exception.what())
        });
    }
    catch (...)
    {
        activationResult = std::unexpected(PageOperationError{
            QStringLiteral("page activation threw an unknown exception")
        });
    }

    if (!activationResult)
    {
        targetPage->onSuspending();
        if (!newlyCreatedPage)
        {
            destroyPage(entry);
        }
        return std::unexpected(error(
            PageHostErrorCode::ActivationFailed,
            *normalizedKey,
            callbackErrorMessage(
                activationResult.error().message,
                QStringLiteral("page activation failed")
                )
            ));
    }

    if (newlyCreatedPage)
    {
        m_stack->addWidget(newlyCreatedPage.get());
        entry.page = newlyCreatedPage.release();
    }

    if (m_activePageKey.has_value())
    {
        const auto previous = m_pages.find(*m_activePageKey);
        if (previous != m_pages.end() && previous->page)
        {
            previous->page->onSuspending();
        }
    }

    m_stack->setCurrentWidget(targetPage);
    m_activePageKey = *normalizedKey;
    return {};
}

PageHostResult<void> PageHost::releasePage(const QString& pageKey)
{
    const auto normalizedKey = normalizedPageKey(pageKey);
    if (!normalizedKey)
    {
        return std::unexpected(error(
            PageHostErrorCode::InvalidPageKey,
            pageKey,
            QStringLiteral("page key must not be empty")
            ));
    }
    if (m_transitionInProgress)
    {
        return std::unexpected(error(
            PageHostErrorCode::ReentrantOperation,
            *normalizedKey,
            QStringLiteral("page release is unavailable during a lifecycle callback")
            ));
    }

    auto entry = m_pages.find(*normalizedKey);
    if (entry == m_pages.end())
    {
        return std::unexpected(error(
            PageHostErrorCode::PageNotRegistered,
            *normalizedKey,
            QStringLiteral("no page factory is registered for this key")
            ));
    }

    TransitionGuard transition(m_transitionInProgress);
    if (m_activePageKey == normalizedKey)
    {
        if (entry->page)
        {
            entry->page->onSuspending();
        }
        m_activePageKey.reset();
        m_stack->setCurrentWidget(m_emptyPage);
    }
    destroyPage(entry.value());
    return {};
}

std::optional<QString> PageHost::activePageKey() const
{
    return m_activePageKey;
}

bool PageHost::isPageRegistered(const QString& pageKey) const
{
    const auto normalizedKey = normalizedPageKey(pageKey);
    return normalizedKey && m_pages.contains(*normalizedKey);
}

bool PageHost::hasPageInstance(const QString& pageKey) const
{
    return pageInstance(pageKey) != nullptr;
}

Page* PageHost::pageInstance(const QString& pageKey) const
{
    const auto normalizedKey = normalizedPageKey(pageKey);
    if (!normalizedKey)
    {
        return nullptr;
    }

    const auto entry = m_pages.constFind(*normalizedKey);
    return entry == m_pages.cend() ? nullptr : entry->page;
}

std::optional<QString> PageHost::normalizedPageKey(const QString& pageKey)
{
    const QString normalizedKey = pageKey.trimmed();
    if (normalizedKey.isEmpty())
    {
        return std::nullopt;
    }
    return normalizedKey;
}

PageHostError PageHost::error(
    const PageHostErrorCode code,
    const QString& pageKey,
    QString message
    )
{
    return {code, pageKey, std::move(message)};
}

void PageHost::destroyPage(PageEntry& entry) noexcept
{
    if (!entry.page)
    {
        return;
    }

    Page* page = entry.page;
    entry.page = nullptr;
    m_stack->removeWidget(page);
    delete page;
}

}  // namespace ClassMngr::Next::Ui
