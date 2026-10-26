#pragma once

#include "next/ui/page.h"
#include "next/ui/page_descriptor.h"

#include <QMap>
#include <QString>
#include <QWidget>

#include <expected>
#include <memory>
#include <optional>
#include <vector>

class QStackedWidget;

namespace ClassMngr::Next::Ui
{

enum class PageHostErrorCode
{
    InvalidPageKey,
    DuplicatePageKey,
    PageNotRegistered,
    InvalidFactory,
    ResourceScopeCreationFailed,
    FactoryFailed,
    FactoryReturnedNull,
    PageAlreadyParented,
    ActivationFailed,
    ReentrantOperation,
    InvalidPageDescriptor
};

struct PageHostError final
{
    PageHostErrorCode code;
    QString pageKey;
    QString message;
};

template <typename Value>
using PageHostResult = std::expected<Value, PageHostError>;

enum class PageLifecycleState
{
    // The descriptor is registered and has no host-managed page instance.
    Declared,
    // A valid factory page exists while the activation attempt is in progress.
    Created,
    // Activation committed and this page is current in the host.
    Activated,
    // Activation completed previously; the page is retained for reuse.
    Suspended,
    // The managed page instance was destroyed after release or activation
    // failure; its descriptor remains registered and may be activated again.
    Released
};

class PageHost final : public QWidget
{
    Q_OBJECT

public:
    using PageFactory = Ui::PageFactory;

    // The resource loader and preferences model are borrowed and must outlive this host.
    explicit PageHost(
        Resources::ResourceLoader& resourceLoader,
        Application::PreferencesModel& preferences,
        QWidget* parent = nullptr
        );
    ~PageHost() override;

    PageHost(const PageHost&) = delete;
    PageHost& operator=(const PageHost&) = delete;

    [[nodiscard]] PageHostResult<void> registerPage(
        PageDescriptor descriptor
        );
    [[nodiscard]] PageHostResult<void> activatePage(const QString& pageKey);
    [[nodiscard]] PageHostResult<void> releasePage(const QString& pageKey);

    [[nodiscard]] std::optional<QString> activePageKey() const;
    [[nodiscard]] bool isPageRegistered(const QString& pageKey) const;
    // Returns empty for an unregistered ID. An activation failure after a
    // valid instance is created leaves the descriptor Released after cleanup.
    [[nodiscard]] std::optional<PageLifecycleState> pageLifecycleState(
        const PageId& pageId
        ) const;
    [[nodiscard]] bool hasPageInstance(const QString& pageKey) const;
    // Returns a borrowed pointer. PageHost remains the widget owner.
    [[nodiscard]] Page* pageInstance(const QString& pageKey) const;
    [[nodiscard]] std::optional<PageDescriptor> pageDescriptor(
        const PageId& pageId
        ) const;
    // Returns descriptor values in stable, sorted page-ID order.
    [[nodiscard]] std::vector<PageDescriptor> pageDescriptors() const;

Q_SIGNALS:
    // Emitted for lifecycle state changes while the host rejects reentrant
    // mutations. Declared is the initial registration state and is available
    // through pageLifecycleState() without an event.
    void pageLifecycleStateChanged(
        QString pageKey,
        PageLifecycleState state
        );

private:
    struct PageEntry final
    {
        PageDescriptor descriptor;
        Page* page = nullptr;
        PageLifecycleState lifecycleState = PageLifecycleState::Declared;
    };

    [[nodiscard]] static std::optional<QString> normalizedPageKey(
        const QString& pageKey
        );
    [[nodiscard]] static PageHostError error(
        PageHostErrorCode code,
        const QString& pageKey,
        QString message
        );
    void setLifecycleState(
        const QString& pageKey,
        PageEntry& entry,
        PageLifecycleState state
        );
    void destroyPage(PageEntry& entry) noexcept;

    Resources::ResourceLoader& m_resourceLoader;
    Application::PreferencesModel& m_preferences;
    QStackedWidget* m_stack = nullptr;
    QWidget* m_emptyPage = nullptr;
    QMap<QString, PageEntry> m_pages;
    std::optional<QString> m_activePageKey;
    bool m_transitionInProgress = false;
};

}  // namespace ClassMngr::Next::Ui
