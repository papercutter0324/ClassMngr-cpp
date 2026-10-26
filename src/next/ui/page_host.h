#pragma once

#include "next/ui/page.h"

#include <QHash>
#include <QString>
#include <QWidget>

#include <expected>
#include <functional>
#include <memory>
#include <optional>

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
    ReentrantOperation
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
    using PageFactory = std::function<
        PageOperationResult<std::unique_ptr<Page>>(const PageContext&)>;

    // The resource loader is borrowed and must outlive this host.
    explicit PageHost(
        Resources::ResourceLoader& resourceLoader,
        QWidget* parent = nullptr
        );
    ~PageHost() override;

    PageHost(const PageHost&) = delete;
    PageHost& operator=(const PageHost&) = delete;

    [[nodiscard]] PageHostResult<void> registerPage(
        QString pageKey,
        PageFactory factory
        );
    [[nodiscard]] PageHostResult<void> activatePage(const QString& pageKey);
    [[nodiscard]] PageHostResult<void> releasePage(const QString& pageKey);

    [[nodiscard]] std::optional<QString> activePageKey() const;
    [[nodiscard]] bool isPageRegistered(const QString& pageKey) const;
    [[nodiscard]] bool hasPageInstance(const QString& pageKey) const;
    // Returns a borrowed pointer. PageHost remains the widget owner.
    [[nodiscard]] Page* pageInstance(const QString& pageKey) const;

private:
    struct PageEntry final
    {
        PageFactory factory;
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
    QHash<QString, PageEntry> m_pages;
    std::optional<QString> m_activePageKey;
    bool m_transitionInProgress = false;
};

}  // namespace ClassMngr::Next::Ui
