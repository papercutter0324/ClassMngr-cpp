#pragma once

#include "core/font_registration_port.h"
#include "next/resources/resource_loader.h"

#include <memory>
#include <utility>

namespace ClassMngr::Next::Platform
{

class ResourceLoaderFontRegistrationAdapter final
    : public Core::FontRegistrationPort
{
public:
    ResourceLoaderFontRegistrationAdapter(
        Resources::ResourceLoader& loader,
        Resources::ResourceScopeKind scopeKind,
        QString ownerLabel)
        : m_loader(&loader)
        , m_scopeKind(scopeKind)
        , m_ownerLabel(std::move(ownerLabel))
    {
    }

    [[nodiscard]] std::expected<
        std::shared_ptr<const Core::FontRegistrationLease>, QString>
    loadFont(Core::FontResource resource) override
    {
        const auto id = Resources::ResourceId::fromString(resourceId(resource));
        if (!id)
        {
            return std::unexpected(
                QStringLiteral("The requested font resource ID is invalid."));
        }
        const auto request = Resources::ResourceRequest::create(
            *id, m_scopeKind, m_ownerLabel);
        if (!request)
        {
            return std::unexpected(request.error().message);
        }
        auto registration = m_loader->loadFont(*request);
        if (!registration)
        {
            return std::unexpected(registration.error().message);
        }
        auto lease = std::make_shared<ResourceLoaderFontLease>(
            std::move(*registration));
        if (lease->familyName().isEmpty())
        {
            return std::unexpected(
                QStringLiteral("The registered font has no family name."));
        }
        return std::shared_ptr<const Core::FontRegistrationLease>(
            std::move(lease));
    }

private:
    class ResourceLoaderFontLease final : public Core::FontRegistrationLease
    {
    public:
        explicit ResourceLoaderFontLease(
            Resources::ScopedResource<Resources::RegisteredFont> registration)
            : m_registration(std::move(registration))
            , m_familyName(m_registration.get().familyNames().value(0))
        {
        }

        [[nodiscard]] const QString& familyName() const noexcept override
        {
            return m_familyName;
        }

    private:
        Resources::ScopedResource<Resources::RegisteredFont> m_registration;
        QString m_familyName;
    };

    [[nodiscard]] static QString resourceId(const Core::FontResource resource)
    {
        switch (resource)
        {
        case Core::FontResource::Inter:
            return QStringLiteral("resource:assets/fonts/Inter.ttc");
        case Core::FontResource::Pretendard:
            return QStringLiteral("resource:assets/fonts/PretendardVariable.ttf");
        case Core::FontResource::JustAnotherHand:
            return QStringLiteral("resource:assets/fonts/JustAnotherHand-Regular.ttf");
        case Core::FontResource::DancingScript:
            return QStringLiteral("resource:assets/fonts/DancingScript-wght.ttf");
        case Core::FontResource::GreatVibes:
            return QStringLiteral("resource:assets/fonts/GreatVibes-Regular.ttf");
        case Core::FontResource::Caveat:
            return QStringLiteral("resource:assets/fonts/Caveat-wght.ttf");
        }
        return {};
    }

    Resources::ResourceLoader* m_loader;
    Resources::ResourceScopeKind m_scopeKind;
    QString m_ownerLabel;
};

}  // namespace ClassMngr::Next::Platform
