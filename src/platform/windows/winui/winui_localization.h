#pragma once

#include "pch.h"

#include <winrt/Microsoft.Windows.ApplicationModel.Resources.h>

#include <optional>
#include <string>
#include <string_view>

namespace winrt::ClassMngrWinUI
{

// WinUILocalizer is the presentation adapter between the shared Qt Linguist
// catalog and Windows App SDK's MRT string resources. The .ts files remain the
// source of truth; the build bridge emits locale-qualified .resw files.
class WinUILocalizer final
{
public:
    WinUILocalizer();
    explicit WinUILocalizer(std::wstring_view languageTag);

    [[nodiscard]] std::wstring getString(
        std::wstring_view context,
        std::wstring_view source
        ) const;
    [[nodiscard]] bool hasString(
        std::wstring_view context,
        std::wstring_view source
        ) const;

    [[nodiscard]] std::wstring_view languageTag() const noexcept
    {
        return m_languageTag;
    }

    [[nodiscard]] static std::wstring makeResourceId(
        std::wstring_view context,
        std::wstring_view source
        );

private:
    [[nodiscard]] std::optional<std::wstring> tryGetString(
        std::wstring_view context,
        std::wstring_view source
        ) const;

    Microsoft::Windows::ApplicationModel::Resources::ResourceManager
        m_resourceManager{nullptr};
    Microsoft::Windows::ApplicationModel::Resources::ResourceContext
        m_resourceContext{nullptr};
    Microsoft::Windows::ApplicationModel::Resources::ResourceMap
        m_stringMap{nullptr};
    std::wstring m_languageTag;
};

} // namespace winrt::ClassMngrWinUI
