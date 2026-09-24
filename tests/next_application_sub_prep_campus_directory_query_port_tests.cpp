#include "next/application/sub_prep_campus_directory_query_port.h"

#include <QtTest/QtTest>

#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace
{
class FakeSubPrepCampusDirectoryQuery final
    : public ClassMngr::Next::Application::
          SubPrepCampusDirectoryQueryPort
{
public:
    explicit FakeSubPrepCampusDirectoryQuery(
        std::vector<
            ClassMngr::Next::Application::SubPrepCampusMetadata
            > campuses
        )
        : m_campuses(std::move(campuses))
    {
    }

    [[nodiscard]] std::vector<
        ClassMngr::Next::Application::SubPrepCampusMetadata
        > loadCampuses() const override
    {
        return m_campuses;
    }

private:
    std::vector<ClassMngr::Next::Application::SubPrepCampusMetadata>
        m_campuses;
};
}

class NextApplicationSubPrepCampusDirectoryQueryPortTests final
    : public QObject
{
    Q_OBJECT

private slots:
    void portExposesOnlyOwningUtf8CampusMetadata();
};

void NextApplicationSubPrepCampusDirectoryQueryPortTests::
portExposesOnlyOwningUtf8CampusMetadata()
{
    using Metadata =
        ClassMngr::Next::Application::SubPrepCampusMetadata;
    using Port =
        ClassMngr::Next::Application::SubPrepCampusDirectoryQueryPort;
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
    static_assert(
        std::is_same_v<decltype(Metadata::officeNumber), std::string>
        );
    static_assert(
        std::is_same_v<decltype(Metadata::wifiName), std::string>
        );
    static_assert(
        std::is_same_v<decltype(Metadata::wifiPassword), std::string>
        );
    static_assert(
        std::is_same_v<decltype(Metadata::photocopierCode), std::string>
        );

    const Metadata expected{
        "campus-id",
        "Campus Name",
        "Office 418",
        "Campus Wi-Fi",
        "wifi-password",
        "copier-01"
    };
    const FakeSubPrepCampusDirectoryQuery query({expected});
    const auto campuses = query.loadCampuses();

    QCOMPARE(campuses.size(), std::size_t(1));
    QCOMPARE(campuses.front().id, expected.id);
    QCOMPARE(campuses.front().displayName, expected.displayName);
    QCOMPARE(campuses.front().officeNumber, expected.officeNumber);
    QCOMPARE(campuses.front().wifiName, expected.wifiName);
    QCOMPARE(campuses.front().wifiPassword, expected.wifiPassword);
    QCOMPARE(campuses.front().photocopierCode, expected.photocopierCode);
}

QTEST_APPLESS_MAIN(NextApplicationSubPrepCampusDirectoryQueryPortTests)

#include "next_application_sub_prep_campus_directory_query_port_tests.moc"
