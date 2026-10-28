#include "next/platform/calendar_page_resource_campus_directory_query_adapter.h"

#include <QJsonObject>

#include <algorithm>
#include <utility>

namespace ClassMngr::Next::Platform
{

CalendarPageResourceCampusDirectoryQueryAdapter::
    CalendarPageResourceCampusDirectoryQueryAdapter(
        Resources::ResourceLoader& resourceLoader,
        Resources::ResourceScope resourceScope
        )
    : m_resourceLoader(&resourceLoader),
      m_resourceScope(std::move(resourceScope))
{
}

std::vector<Application::CalendarPageCampusMetadata>
CalendarPageResourceCampusDirectoryQueryAdapter::loadCampuses() const
{
    constexpr auto campusResourcePrefix = "assets/campuses/";
    const QString prefix = QString::fromLatin1(campusResourcePrefix);
    std::vector<Application::CalendarPageCampusMetadata> campuses;

    for (const Resources::ResourceDescriptor& descriptor :
         m_resourceLoader->catalog().records())
    {
        const QString path = descriptor.packageRelativePath();
        if (descriptor.expectedType() != Resources::ResourceType::Json
            || !path.startsWith(prefix)
            || path.indexOf(QLatin1Char('/'), prefix.size()) >= 0
            || !path.endsWith(QStringLiteral(".json"), Qt::CaseInsensitive))
        {
            continue;
        }

        const auto request = Resources::ResourceRequest::create(
            descriptor.id(), m_resourceScope);
        if (!request)
        {
            continue;
        }
        const auto resource = m_resourceLoader->loadJson(*request);
        if (!resource)
        {
            continue;
        }

        const QJsonObject object = resource->get().object();
        const QString id = object.value(QStringLiteral("id"))
                               .toString().trimmed();
        const QString name = object.value(QStringLiteral("campus_name"))
                                 .toString().trimmed();
        if (id.isEmpty() || name.isEmpty()
            || id.compare(QStringLiteral("default"), Qt::CaseInsensitive) == 0
            || name.compare(QStringLiteral("Default"), Qt::CaseInsensitive) == 0)
        {
            continue;
        }

        const auto typedId = Domain::CampusId::fromString(
            id.toUtf8().toStdString());
        if (!typedId)
        {
            continue;
        }

        Application::CalendarPageCampusMetadata metadata{
            .id = *typedId,
            .campusName = name.toUtf8().toStdString()
        };
        const QString code = object.value(QStringLiteral("campus_code"))
                                 .toString().trimmed();
        if (!code.isEmpty())
        {
            metadata.campusCode.emplace(code.toUtf8().toStdString());
        }
        campuses.push_back(std::move(metadata));
    }

    std::ranges::sort(campuses,
        [](const auto& left, const auto& right)
        {
            const QString leftName = QString::fromUtf8(
                left.campusName.data(),
                static_cast<qsizetype>(left.campusName.size()));
            const QString rightName = QString::fromUtf8(
                right.campusName.data(),
                static_cast<qsizetype>(right.campusName.size()));
            const int comparison = QString::compare(
                leftName, rightName, Qt::CaseInsensitive);
            return comparison == 0
                ? left.id.value() < right.id.value()
                : comparison < 0;
        });

    return campuses;
}

} // namespace ClassMngr::Next::Platform
