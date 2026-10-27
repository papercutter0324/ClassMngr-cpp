#pragma once

#include "next/resources/resource_loader.h"
#include "next/application/preferences_model.h"
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
    [[nodiscard]] Application::PreferencesModel& preferences() const noexcept
    {
        return *m_preferences;
    }

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
        Application::PreferencesModel& preferences,
        Resources::ResourceScope pageScope,
        QString pageKey
        )
        : m_resourceLoader(&resourceLoader)
        , m_preferences(&preferences)
        , m_pageScope(std::move(pageScope))
        , m_pageKey(std::move(pageKey))
    {
    }

    Resources::ResourceLoader* m_resourceLoader;
    Application::PreferencesModel* m_preferences;
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

    // Editable pages override these methods to expose and resolve their
    // in-memory draft. The default page is clean; a dirty page without an
    // explicit save/discard implementation is rejected safely.
    [[nodiscard]] virtual bool hasUnsavedChanges() const noexcept
    {
        return m_hasUnsavedChanges;
    }

    // Editable page handlers call this when their in-memory draft diverges
    // from the page's last saved representation.
    void markUnsavedChanges() noexcept
    {
        setHasUnsavedChanges(true);
    }

    [[nodiscard]] virtual PageOperationResult<void> saveChanges()
    {
        if (hasUnsavedChanges())
        {
            return std::unexpected(PageOperationError{
                QStringLiteral("This page does not provide a save operation.")
            });
        }
        return {};
    }

    [[nodiscard]] virtual PageOperationResult<void> discardChanges()
    {
        if (hasUnsavedChanges())
        {
            return std::unexpected(PageOperationError{
                QStringLiteral("This page does not provide a discard operation.")
            });
        }
        return {};
    }

    // Resource scopes label requests only. Pages own their scoped handles;
    // clear feature-scoped handles here while retaining page-scoped handles
    // until this page is released.
    virtual void onSuspending() noexcept
    {
    }

protected:
    void setHasUnsavedChanges(const bool unsaved) noexcept
    {
        m_hasUnsavedChanges = unsaved;
    }

private:
    PageContext m_resourceContext;
    bool m_hasUnsavedChanges = false;
};

}  // namespace ClassMngr::Next::Ui
