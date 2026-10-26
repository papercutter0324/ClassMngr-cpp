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
    PageDescriptor descriptor
    )
{
    const QString pageKey = descriptor.id.value();
    const auto normalizedKey = normalizedPageKey(pageKey);
    if (!normalizedKey)
    {
        return std::unexpected(error(
            PageHostErrorCode::InvalidPageDescriptor,
            pageKey,
            QStringLiteral("page descriptor ID must not be empty")
            ));
    }
    const auto normalizedId = PageId::fromString(*normalizedKey);
    if (!normalizedId)
    {
        return std::unexpected(error(
            PageHostErrorCode::InvalidPageDescriptor,
            pageKey,
            QStringLiteral("page descriptor ID must not be empty")
            ));
    }
    descriptor.id = *normalizedId;

    if (descriptor.label.trimmed().isEmpty())
    {
        return std::unexpected(error(
            PageHostErrorCode::InvalidPageDescriptor,
            *normalizedKey,
            QStringLiteral("page descriptor label must not be empty")
            ));
    }
    if (!descriptor.factory)
    {
        return std::unexpected(error(
            PageHostErrorCode::InvalidFactory,
            *normalizedKey,
            QStringLiteral("page descriptor factory is empty")
            ));
    }
    for (const QString& permission : descriptor.requiredPermissions)
    {
        if (permission.trimmed().isEmpty())
        {
            return std::unexpected(error(
                PageHostErrorCode::InvalidPageDescriptor,
                *normalizedKey,
                QStringLiteral("page descriptor permission ID must not be empty")
                ));
        }
    }
    if (descriptor.resourcePolicy != PageResourcePolicy::PageScoped
        || descriptor.initialDataPolicy != PageInitialDataPolicy::None)
    {
        return std::unexpected(error(
            PageHostErrorCode::InvalidPageDescriptor,
            *normalizedKey,
            QStringLiteral("page descriptor contains an unsupported policy value")
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
            QStringLiteral("a page descriptor is already registered for this ID")
            ));
    }

    m_pages.insert(
        *normalizedKey,
        PageEntry{std::move(descriptor), nullptr}
        );
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
            factoryResult = entry.descriptor.factory(context);
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
        setLifecycleState(
            *normalizedKey,
            entry,
            PageLifecycleState::Created
            );
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
        if (newlyCreatedPage)
        {
            newlyCreatedPage.reset();
        }
        else
        {
            destroyPage(entry);
        }
        setLifecycleState(
            *normalizedKey,
            entry,
            PageLifecycleState::Released
            );
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

    QString previousPageKey;
    PageEntry* previousEntry = nullptr;
    if (m_activePageKey.has_value())
    {
        previousPageKey = *m_activePageKey;
        auto previous = m_pages.find(previousPageKey);
        if (previous != m_pages.end() && previous->page)
        {
            previous->page->onSuspending();
            previousEntry = &previous.value();
        }
    }

    m_stack->setCurrentWidget(targetPage);
    m_activePageKey = *normalizedKey;

    // Update both states before notifying observers so a signal handler sees
    // the complete committed transition when it queries the host.
    const bool previousStateChanged = previousEntry
        && previousEntry->lifecycleState != PageLifecycleState::Suspended;
    if (previousStateChanged)
    {
        previousEntry->lifecycleState = PageLifecycleState::Suspended;
    }
    const bool targetStateChanged =
        entry.lifecycleState != PageLifecycleState::Activated;
    if (targetStateChanged)
    {
        entry.lifecycleState = PageLifecycleState::Activated;
    }
    if (previousStateChanged)
    {
        Q_EMIT pageLifecycleStateChanged(
            previousPageKey,
            PageLifecycleState::Suspended
            );
    }
    if (targetStateChanged)
    {
        Q_EMIT pageLifecycleStateChanged(
            *normalizedKey,
            PageLifecycleState::Activated
            );
    }
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
        setLifecycleState(
            *normalizedKey,
            entry.value(),
            PageLifecycleState::Suspended
            );
    }
    destroyPage(entry.value());
    setLifecycleState(
        *normalizedKey,
        entry.value(),
        PageLifecycleState::Released
        );
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

std::optional<PageLifecycleState> PageHost::pageLifecycleState(
    const PageId& pageId
    ) const
{
    const auto entry = m_pages.constFind(pageId.value());
    if (entry == m_pages.cend())
    {
        return std::nullopt;
    }

    return entry->lifecycleState;
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

std::optional<PageDescriptor> PageHost::pageDescriptor(
    const PageId& pageId
    ) const
{
    const auto entry = m_pages.constFind(pageId.value());
    if (entry == m_pages.cend())
    {
        return std::nullopt;
    }

    return entry->descriptor;
}

std::vector<PageDescriptor> PageHost::pageDescriptors() const
{
    std::vector<PageDescriptor> descriptors;
    descriptors.reserve(static_cast<std::size_t>(m_pages.size()));
    for (auto entry = m_pages.cbegin(); entry != m_pages.cend(); ++entry)
    {
        descriptors.push_back(entry->descriptor);
    }
    return descriptors;
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

void PageHost::setLifecycleState(
    const QString& pageKey,
    PageEntry& entry,
    const PageLifecycleState state
    )
{
    if (entry.lifecycleState == state)
    {
        return;
    }

    entry.lifecycleState = state;
    Q_EMIT pageLifecycleStateChanged(pageKey, state);
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
