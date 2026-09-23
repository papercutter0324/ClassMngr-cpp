#include "next/application/my_info_campus_directory_query_port.h"

#include <QtTest/QtTest>

#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace
{
class FakeMyInfoCampusDirectoryQuery final
    : public ClassMngr::Next::Application::
          MyInfoCampusDirectoryQueryPort
{
public:
    explicit FakeMyInfoCampusDirectoryQuery(
        std::vector<
            ClassMngr::Next::Application::MyInfoCampusMetadata
            > campuses
        )
        : m_campuses(std::move(campuses))
    {
    }

    [[nodiscard]] std::vector<
        ClassMngr::Next::Application::MyInfoCampusMetadata
        > loadCampuses() const override
    {
        return m_campuses;
    }

private:
    std::vector<ClassMngr::Next::Application::MyInfoCampusMetadata>
        m_campuses;
};
}

class NextApplicationMyInfoCampusDirectoryQueryPortTests final
    : public QObject
{
    Q_OBJECT

private slots:
    void portUsesOnlyOwningUtf8CampusMetadata();
};

void NextApplicationMyInfoCampusDirectoryQueryPortTests::
portUsesOnlyOwningUtf8CampusMetadata()
{
    using Metadata =
        ClassMngr::Next::Application::MyInfoCampusMetadata;
    using Port =
        ClassMngr::Next::Application::MyInfoCampusDirectoryQueryPort;
    static_assert(
        std::is_same_v<
            decltype(std::declval<const Port&>().loadCampuses()),
            std::vector<Metadata>
            >
        );
    static_assert(std::is_same_v<decltype(Metadata::id), std::string>);
    static_assert(
        std::is_same_v<decltype(Metadata::displayName), std::string>
        );

    const std::string id = "id-\xEC\x84\x9C\xEC\x9A\xB8";
    const std::string displayName =
        "\xEC\x84\x9C\xEC\x9A\xB8 \xEC\xBA\xA0\xED\x8D\xBC\xEC\x8A\xA4";
    const FakeMyInfoCampusDirectoryQuery query({{id, displayName}});
    const auto campuses = query.loadCampuses();

    QCOMPARE(campuses.size(), std::size_t(1));
    QCOMPARE(campuses.front().id, id);
    QCOMPARE(campuses.front().displayName, displayName);
}

QTEST_APPLESS_MAIN(NextApplicationMyInfoCampusDirectoryQueryPortTests)

#include "next_application_my_info_campus_directory_query_port_tests.moc"
