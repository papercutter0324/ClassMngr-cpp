#include "core/qt_font_registration_adapter.h"

#include "core/resource_paths.h"

#include <QFontDatabase>
#include <QGuiApplication>
#include <QStringList>

#include <algorithm>
#include <vector>
#include <utility>

namespace ClassMngr::Core
{
namespace
{
QString resourcePath(const FontResource resource)
{
    switch (resource)
    {
    case FontResource::Inter:
        return ResourcePaths::Fonts::inter();
    case FontResource::Pretendard:
        return ResourcePaths::Fonts::pretendard();
    case FontResource::JustAnotherHand:
        return ResourcePaths::Fonts::justAnotherHand();
    case FontResource::DancingScript:
        return ResourcePaths::Fonts::dancingScript();
    case FontResource::GreatVibes:
        return ResourcePaths::Fonts::greatVibes();
    case FontResource::Caveat:
        return ResourcePaths::Fonts::caveat();
    }
    return {};
}

class QtFontRegistrationLease final : public FontRegistrationLease
{
public:
    QtFontRegistrationLease(std::vector<int> registrationIds, QString familyName)
        : m_registrationIds(std::move(registrationIds))
        , m_familyName(std::move(familyName))
    {
    }

    ~QtFontRegistrationLease() override
    {
        if (QGuiApplication::instance())
        {
            for (auto registrationId = m_registrationIds.crbegin();
                 registrationId != m_registrationIds.crend();
                 ++registrationId)
            {
                if (*registrationId >= 0)
                {
                    QFontDatabase::removeApplicationFont(*registrationId);
                }
            }
        }
    }

    [[nodiscard]] const QString& familyName() const noexcept override
    {
        return m_familyName;
    }

private:
    std::vector<int> m_registrationIds;
    QString m_familyName;
};
}  // namespace

std::expected<std::shared_ptr<const FontRegistrationLease>, QString>
QtFontRegistrationAdapter::loadFont(const FontResource resource)
{
    QStringList paths;
    if (resource == FontResource::Inter)
    {
        const auto staticFaces = ResourcePaths::Fonts::interStaticFaces();
        paths.reserve(static_cast<qsizetype>(staticFaces.size()));
        for (const QString& path : staticFaces)
        {
            paths.append(path);
        }
    }
    else
    {
        paths.append(resourcePath(resource));
    }
    if (paths.isEmpty() || std::any_of(paths.cbegin(), paths.cend(), [](const QString& path) {
            return path.isEmpty();
        }))
    {
        return std::unexpected(QStringLiteral("The requested font is unavailable."));
    }

    std::vector<int> registrationIds;
    registrationIds.reserve(static_cast<std::size_t>(paths.size()));
    QString familyName;
    for (const QString& path : paths)
    {
        const int registrationId = QFontDatabase::addApplicationFont(path);
        if (registrationId < 0)
        {
            for (auto registered = registrationIds.crbegin();
                 registered != registrationIds.crend();
                 ++registered)
            {
                QFontDatabase::removeApplicationFont(*registered);
            }
            return std::unexpected(QStringLiteral("Qt could not register the bundled font."));
        }
        registrationIds.push_back(registrationId);

        const QStringList families = QFontDatabase::applicationFontFamilies(registrationId);
        if (families.isEmpty())
        {
            for (auto registered = registrationIds.crbegin();
                 registered != registrationIds.crend();
                 ++registered)
            {
                QFontDatabase::removeApplicationFont(*registered);
            }
            return std::unexpected(QStringLiteral("The registered font has no family name."));
        }
        if (familyName.isEmpty())
        {
            familyName = families.first();
        }
        else if (families.first() != familyName)
        {
            for (auto registered = registrationIds.crbegin();
                 registered != registrationIds.crend();
                 ++registered)
            {
                QFontDatabase::removeApplicationFont(*registered);
            }
            return std::unexpected(QStringLiteral("The bundled font faces do not share one family."));
        }
    }

    return std::shared_ptr<const FontRegistrationLease>(
        std::make_shared<QtFontRegistrationLease>(std::move(registrationIds), std::move(familyName)));
}

}  // namespace ClassMngr::Core
