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

class PageHost final : public QWidget
{
public:
    using PageFactory = Ui::PageFactory;

    // The resource loader is borrowed and must outlive this host.
    explicit PageHost(
        Resources::ResourceLoader& resourceLoader,
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
    [[nodiscard]] bool hasPageInstance(const QString& pageKey) const;
    // Returns a borrowed pointer. PageHost remains the widget owner.
    [[nodiscard]] Page* pageInstance(const QString& pageKey) const;
    [[nodiscard]] std::optional<PageDescriptor> pageDescriptor(
        const PageId& pageId
        ) const;
    // Returns descriptor values in stable, sorted page-ID order.
    [[nodiscard]] std::vector<PageDescriptor> pageDescriptors() const;

private:
    struct PageEntry final
    {
        PageDescriptor descriptor;
        Page* page = nullptr;
    };

    [[nodiscard]] static std::optional<QString> normalizedPageKey(
        const QString& pageKey
        );
    [[nodiscard]] static PageHostError error(
        PageHostErrorCode code,
        const QString& pageKey,
        QString message
        );
    void destroyPage(PageEntry& entry) noexcept;

    Resources::ResourceLoader& m_resourceLoader;
    QStackedWidget* m_stack = nullptr;
    QWidget* m_emptyPage = nullptr;
    QMap<QString, PageEntry> m_pages;
    std::optional<QString> m_activePageKey;
    bool m_transitionInProgress = false;
};

}  // namespace ClassMngr::Next::Ui
