#include "core/qt_font_registration_adapter.h"

#include "core/resource_paths.h"

#include <QFontDatabase>
#include <QGuiApplication>
#include <QStringList>

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
    QtFontRegistrationLease(int registrationId, QString familyName)
        : m_registrationId(registrationId)
        , m_familyName(std::move(familyName))
    {
    }

    ~QtFontRegistrationLease() override
    {
        if (m_registrationId >= 0 && QGuiApplication::instance())
        {
            QFontDatabase::removeApplicationFont(m_registrationId);
        }
    }

    [[nodiscard]] const QString& familyName() const noexcept override
    {
        return m_familyName;
    }

private:
    int m_registrationId = -1;
    QString m_familyName;
};
}  // namespace

std::expected<std::shared_ptr<const FontRegistrationLease>, QString>
QtFontRegistrationAdapter::loadFont(const FontResource resource)
{
    const QString path = resourcePath(resource);
    if (path.isEmpty())
    {
        return std::unexpected(QStringLiteral("The requested font is unavailable."));
    }

    const int registrationId = QFontDatabase::addApplicationFont(path);
    if (registrationId < 0)
    {
        return std::unexpected(QStringLiteral("Qt could not register the bundled font."));
    }

    const QStringList families = QFontDatabase::applicationFontFamilies(registrationId);
    if (families.isEmpty())
    {
        QFontDatabase::removeApplicationFont(registrationId);
        return std::unexpected(QStringLiteral("The registered font has no family name."));
    }

    return std::shared_ptr<const FontRegistrationLease>(
        std::make_shared<QtFontRegistrationLease>(registrationId, families.first()));
}

}  // namespace ClassMngr::Core
