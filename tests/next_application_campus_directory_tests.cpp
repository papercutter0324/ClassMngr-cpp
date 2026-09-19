#include "next/application/campus_directory_projection.h"

#include <QtTest/QtTest>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>

using namespace ClassMngr::Next;
using namespace ClassMngr::Next::Application;
using namespace ClassMngr::Next::Domain;

namespace
{

CampusId campusId(
    std::string value
    )
{
    return *CampusId::fromString(value);
}

CampusSummary campusSummary(
    std::string id,
    std::string key,
    const std::int32_t order = 0,
    const bool active = true,
    std::optional<std::string> notes = std::string("Campus notes")
    )
{
    CampusSummary campus{
        campusId(std::move(id)),
        "Campus display name",
        std::move(key),
        "123 Campus Street",
        std::move(notes),
        order,
        active
    };
    return campus;
}

CampusDirectoryProjectionInput validInput()
{
    CampusDirectoryProjectionInput input;
    input.campuses = {
        campusSummary("campus-1", "main", 4, true, "Main notes"),
        campusSummary("campus-2", "west", 8, false, std::nullopt)
    };
    input.campuses[0].displayName = "Main Campus";
    input.campuses[0].address = "1 Main Campus Road";
    input.campuses[1].displayName = "West Campus";
    input.campuses[1].address = "2 West Campus Road";
    return input;
}

void verifyInvalid(
    const Result<CampusDirectoryProjection>& result
    )
{
    QVERIFY(!result);
    QVERIFY(!result.hasValue());
    QCOMPARE(result.error().code, ErrorCode::InvalidInput);
    QVERIFY(!result.error().message.empty());
    QVERIFY(!result.error().recoverable);
}

void verifyInvalidValidation(
    const Result<void>& result
    )
{
    QVERIFY(!result);
    QVERIFY(!result.hasValue());
    QCOMPARE(result.error().code, ErrorCode::InvalidInput);
    QVERIFY(!result.error().message.empty());
}

template <typename Value>
concept HasRawSourceAccessor = requires(const Value& value)
{
    value.rawSource();
};

}

class NextApplicationCampusDirectoryTests final : public QObject
{
    Q_OBJECT

private slots:
    void valid96ScaleDirectoryRetainsTypedMetadataAndBounds();
    void campusIdentifierAndRecordFieldsRemainTypedAndExplicit();
    void emptyDirectoryIsValidAndLookupMissesAreExplicit();
    void lookupsByIdAndKeyReturnIndependentValueCopies();
    void duplicateIdsAndKeysAreRejected();
    void exactIdentifierAndTextCapsAreAccepted();
    void exactNotesCapIsAcceptedByCreateAndRetained();
    void blankAndOversizedFieldsAreRejected();
    void negativeOrderAndOptionalNotesBoundsAreRejected();
    void entryCapRejectsOverflowAndAcceptsExactCap();
    void recordsAndProjectionAreCopyableEqualAndReleasable();
    void contractHasNoMutablePointerOrRichRecordSurface();
};

void NextApplicationCampusDirectoryTests::valid96ScaleDirectoryRetainsTypedMetadataAndBounds()
{
    CampusDirectoryProjectionInput input;
    input.campuses.reserve(96);
    for (std::size_t index = 0; index < 96; ++index)
    {
        auto campus = campusSummary(
            "campus-" + std::to_string(index + 1),
            "code-" + std::to_string(index + 1),
            static_cast<std::int32_t>(index * 2),
            index % 3 != 0,
            index % 4 == 0
                ? std::optional<std::string>{
                      "Notes for campus " + std::to_string(index + 1)
                  }
                : std::nullopt
            );
        campus.displayName = "Campus " + std::to_string(index + 1);
        campus.address = "Address " + std::to_string(index + 1);
        input.campuses.push_back(std::move(campus));
    }

    const auto result = CampusDirectoryProjection::create(std::move(input));

    QVERIFY(result);
    const auto& projection = result.value();
    QCOMPARE(projection.campusCount(), std::size_t(96));
    QCOMPARE(projection.size(), std::size_t(96));
    QCOMPARE(projection.campuses().size(), std::size_t(96));
    QCOMPARE(projection.summaries().size(), std::size_t(96));
    QCOMPARE(projection.entries().size(), std::size_t(96));
    QVERIFY(!projection.empty());
    QCOMPARE(
        projection.campuses().front().id.value(),
        std::string("campus-1")
        );
    QCOMPARE(projection.campuses().front().key, std::string("code-1"));
    QCOMPARE(projection.campuses().front().code(), std::string("code-1"));
    QCOMPARE(projection.campuses().front().location(), std::string("Address 1"));
    QCOMPARE(projection.campuses().front().order, std::int32_t(0));
    QVERIFY(!projection.campuses().front().active);
    QVERIFY(projection.campuses().front().hasNotes());
    QVERIFY(projection.campuses().at(1).active);
    QVERIFY(!projection.campuses().at(1).notes.has_value());
    QCOMPARE(projection.campuses().back().order, std::int32_t(190));
}

void NextApplicationCampusDirectoryTests::campusIdentifierAndRecordFieldsRemainTypedAndExplicit()
{
    static_assert(!std::is_same_v<CampusId, ClassId>);
    static_assert(!std::is_convertible_v<CampusId, ClassId>);
    static_assert(!std::is_convertible_v<ClassId, CampusId>);
    static_assert(std::is_same_v<decltype(CampusSummary::id), CampusId>);
    static_assert(std::is_same_v<decltype(CampusSummary::notes), std::optional<std::string>>);
    static_assert(std::is_same_v<decltype(CampusSummary::order), std::int32_t>);
    static_assert(std::is_same_v<decltype(CampusSummary::active), bool>);

    const auto result = CampusDirectoryProjection::create(validInput());

    QVERIFY(result);
    const auto campus = result.value().findCampus(campusId("campus-1"));
    QVERIFY(campus.has_value());
    QCOMPARE(campus->id.value(), std::string("campus-1"));
    QCOMPARE(campus->displayName, std::string("Main Campus"));
    QCOMPARE(campus->key, std::string("main"));
    QCOMPARE(campus->address, std::string("1 Main Campus Road"));
    QCOMPARE(campus->notes.value(), std::string("Main notes"));
    QCOMPARE(campus->order, std::int32_t(4));
    QVERIFY(campus->active);
    QVERIFY(campus->isActive());
}

void NextApplicationCampusDirectoryTests::emptyDirectoryIsValidAndLookupMissesAreExplicit()
{
    const CampusDirectoryProjectionInput emptyInput;
    QVERIFY(CampusDirectoryProjection::validate(emptyInput));

    const auto result = CampusDirectoryProjection::create(emptyInput);

    QVERIFY(result);
    const auto& projection = result.value();
    QVERIFY(projection.empty());
    QCOMPARE(projection.campusCount(), std::size_t(0));
    QVERIFY(projection.campuses().empty());
    QVERIFY(!projection.findCampus(campusId("missing")).has_value());
    QVERIFY(!projection.lookupCampus(campusId("missing")).has_value());
    QVERIFY(!projection.findById(campusId("missing")).has_value());
    QVERIFY(!projection.findCampus("missing").has_value());
    QVERIFY(!projection.lookupByKey("missing").has_value());

    const CampusDirectoryProjection defaultProjection;
    QVERIFY(defaultProjection.empty());
    QVERIFY(defaultProjection == projection);
}

void NextApplicationCampusDirectoryTests::lookupsByIdAndKeyReturnIndependentValueCopies()
{
    const auto result = CampusDirectoryProjection::create(validInput());
    QVERIFY(result);
    const CampusDirectoryProjection projection = result.value();

    auto idCopy = projection.findCampus(campusId("campus-1"));
    QVERIFY(idCopy.has_value());
    idCopy->displayName = "Changed outside projection";
    idCopy->key = "changed-key";
    idCopy->notes = "Changed notes";
    idCopy->active = false;

    auto keyCopy = projection.lookupByKey("west");
    QVERIFY(keyCopy.has_value());
    keyCopy->address = "Changed address";
    keyCopy->order = 99;

    const auto storedById = projection.lookupCampus(campusId("campus-1"));
    const auto storedByKey = projection.findCampus("west");
    QVERIFY(storedById.has_value());
    QVERIFY(storedByKey.has_value());
    QCOMPARE(storedById->displayName, std::string("Main Campus"));
    QCOMPARE(storedById->key, std::string("main"));
    QCOMPARE(storedById->notes.value(), std::string("Main notes"));
    QVERIFY(storedById->active);
    QCOMPARE(storedByKey->address, std::string("2 West Campus Road"));
    QCOMPARE(storedByKey->order, std::int32_t(8));
    QVERIFY(!storedByKey->active);
    QVERIFY(!projection.findByKey("not-present").has_value());
    QVERIFY(!projection.findCampus(campusId("not-present")).has_value());
}

void NextApplicationCampusDirectoryTests::duplicateIdsAndKeysAreRejected()
{
    auto duplicateIds = validInput();
    duplicateIds.campuses.push_back(
        campusSummary("campus-1", "new-key")
        );
    verifyInvalid(CampusDirectoryProjection::create(std::move(duplicateIds)));

    auto duplicateKeys = validInput();
    duplicateKeys.campuses.push_back(
        campusSummary("campus-3", "main")
        );
    verifyInvalid(CampusDirectoryProjection::create(std::move(duplicateKeys)));

    auto duplicateIdsValidation = validInput();
    duplicateIdsValidation.campuses.push_back(
        campusSummary("campus-1", "another-key")
        );
    verifyInvalidValidation(
        CampusDirectoryProjection::validate(duplicateIdsValidation)
        );
}

void NextApplicationCampusDirectoryTests::exactIdentifierAndTextCapsAreAccepted()
{
    auto exactCaps = validInput();
    auto& campus = exactCaps.campuses.front();
    campus.id = campusId(
        std::string(kCampusDirectoryMaxIdentifierLength, 'i')
        );
    campus.key = std::string(kCampusDirectoryMaxKeyLength, 'k');
    campus.displayName = std::string(
        kCampusDirectoryMaxDisplayNameLength,
        'd'
        );
    campus.address = std::string(
        kCampusDirectoryMaxAddressLength,
        'a'
        );

    const auto result = CampusDirectoryProjection::create(std::move(exactCaps));

    QVERIFY(result);
    const auto& storedCampus = result.value().campuses().front();
    QCOMPARE(
        storedCampus.id.value().size(),
        kCampusDirectoryMaxIdentifierLength
        );
    QCOMPARE(storedCampus.key.size(), kCampusDirectoryMaxKeyLength);
    QCOMPARE(
        storedCampus.displayName.size(),
        kCampusDirectoryMaxDisplayNameLength
        );
    QCOMPARE(storedCampus.address.size(), kCampusDirectoryMaxAddressLength);
}

void NextApplicationCampusDirectoryTests::exactNotesCapIsAcceptedByCreateAndRetained()
{
    auto exactNotes = validInput();
    exactNotes.campuses.front().notes = std::string(
        kCampusSummaryMaxNotesLength,
        'n'
        );

    const auto result = CampusDirectoryProjection::create(std::move(exactNotes));

    QVERIFY(result);
    QCOMPARE(
        result.value().campuses().front().notes.value(),
        std::string(kCampusSummaryMaxNotesLength, 'n')
        );
}

void NextApplicationCampusDirectoryTests::blankAndOversizedFieldsAreRejected()
{
    auto blankDisplayName = validInput();
    blankDisplayName.campuses.front().displayName = " \t";
    verifyInvalid(
        CampusDirectoryProjection::create(std::move(blankDisplayName))
        );

    auto blankKey = validInput();
    blankKey.campuses.front().key = "\n";
    verifyInvalid(CampusDirectoryProjection::create(std::move(blankKey)));

    auto blankAddress = validInput();
    blankAddress.campuses.front().address.clear();
    verifyInvalid(CampusDirectoryProjection::create(std::move(blankAddress)));

    auto blankNotes = validInput();
    blankNotes.campuses.front().notes = " \t";
    verifyInvalid(CampusDirectoryProjection::create(std::move(blankNotes)));

    auto oversizedIdentifier = validInput();
    oversizedIdentifier.campuses.front().id = campusId(
        std::string(kCampusDirectoryMaxIdentifierLength + 1, 'i')
        );
    verifyInvalid(
        CampusDirectoryProjection::create(std::move(oversizedIdentifier))
        );

    auto oversizedKey = validInput();
    oversizedKey.campuses.front().key = std::string(
        kCampusDirectoryMaxKeyLength + 1,
        'k'
        );
    verifyInvalid(CampusDirectoryProjection::create(std::move(oversizedKey)));

    auto oversizedDisplayName = validInput();
    oversizedDisplayName.campuses.front().displayName = std::string(
        kCampusDirectoryMaxDisplayNameLength + 1,
        'd'
        );
    verifyInvalid(
        CampusDirectoryProjection::create(std::move(oversizedDisplayName))
        );

    auto oversizedAddress = validInput();
    oversizedAddress.campuses.front().address = std::string(
        kCampusDirectoryMaxAddressLength + 1,
        'a'
        );
    verifyInvalid(
        CampusDirectoryProjection::create(std::move(oversizedAddress))
        );

    auto oversizedNotes = validInput();
    oversizedNotes.campuses.front().notes = std::string(
        kCampusDirectoryMaxNotesLength + 1,
        'n'
        );
    verifyInvalid(
        CampusDirectoryProjection::create(std::move(oversizedNotes))
        );
}

void NextApplicationCampusDirectoryTests::negativeOrderAndOptionalNotesBoundsAreRejected()
{
    auto negativeOrder = validInput();
    negativeOrder.campuses.front().order = -1;
    verifyInvalid(
        CampusDirectoryProjection::create(std::move(negativeOrder))
        );

    auto noNotes = validInput();
    noNotes.campuses.front().notes.reset();
    QVERIFY(CampusDirectoryProjection::validate(noNotes));

    auto exactNotes = validInput();
    exactNotes.campuses.front().notes = std::string(
        kCampusDirectoryMaxNotesLength,
        'n'
        );
    QVERIFY(CampusDirectoryProjection::validate(exactNotes));

    auto invalidCampus = validInput().campuses.front();
    invalidCampus.notes = std::string(" ");
    verifyInvalidValidation(CampusDirectoryProjection::validate(invalidCampus));
}

void NextApplicationCampusDirectoryTests::entryCapRejectsOverflowAndAcceptsExactCap()
{
    CampusDirectoryProjectionInput exactInput;
    exactInput.campuses.reserve(kCampusDirectoryMaxEntries);
    for (std::size_t index = 0; index < kCampusDirectoryMaxEntries; ++index)
    {
        exactInput.campuses.push_back(
            campusSummary(
                "campus-" + std::to_string(index),
                "key-" + std::to_string(index),
                static_cast<std::int32_t>(index),
                index % 2 == 0,
                std::nullopt
                )
            );
    }

    QVERIFY(CampusDirectoryProjection::validate(exactInput));
    const auto exactResult = CampusDirectoryProjection::create(
        std::move(exactInput)
        );
    QVERIFY(exactResult);
    QCOMPARE(exactResult.value().campusCount(), kCampusDirectoryMaxEntries);

    auto overflowInput = validInput();
    overflowInput.campuses.reserve(kCampusDirectoryMaxEntries + 1);
    for (std::size_t index = overflowInput.campuses.size();
         index < kCampusDirectoryMaxEntries + 1;
         ++index)
    {
        overflowInput.campuses.push_back(
            campusSummary(
                "overflow-campus-" + std::to_string(index),
                "overflow-key-" + std::to_string(index)
                )
            );
    }
    verifyInvalid(
        CampusDirectoryProjection::create(std::move(overflowInput))
        );
}

void NextApplicationCampusDirectoryTests::recordsAndProjectionAreCopyableEqualAndReleasable()
{
    static_assert(std::is_copy_constructible_v<CampusSummary>);
    static_assert(std::is_copy_assignable_v<CampusSummary>);
    static_assert(
        std::is_copy_constructible_v<CampusDirectoryProjectionInput>
        );
    static_assert(std::is_copy_assignable_v<CampusDirectoryProjectionInput>);
    static_assert(std::is_copy_constructible_v<CampusDirectoryProjection>);
    static_assert(std::is_copy_assignable_v<CampusDirectoryProjection>);

    const auto input = validInput();
    const auto inputCopy = input;
    QVERIFY(inputCopy == input);

    const auto result = CampusDirectoryProjection::create(input);
    QVERIFY(result);
    const CampusDirectoryProjection original = result.value();
    const CampusDirectoryProjection copy = original;
    QVERIFY(copy == original);

    CampusDirectoryProjection assigned;
    assigned = original;
    QVERIFY(assigned == original);

    const auto moved = std::move(assigned);
    QVERIFY(moved == original);
    QVERIFY(original == copy);
}

void NextApplicationCampusDirectoryTests::contractHasNoMutablePointerOrRichRecordSurface()
{
    static_assert(!HasRawSourceAccessor<CampusSummary>);
    static_assert(!HasRawSourceAccessor<CampusDirectoryProjectionInput>);
    static_assert(!HasRawSourceAccessor<CampusDirectoryProjection>);
    static_assert(
        std::is_same_v<
            decltype(std::declval<const CampusDirectoryProjection>().findCampus(
                std::declval<const CampusId&>()
                )),
            std::optional<CampusSummary>
            >
        );
    static_assert(
        std::is_same_v<
            decltype(std::declval<const CampusDirectoryProjection>().findByKey(
                std::string_view{}
                )),
            std::optional<CampusSummary>
            >
        );
    static_assert(
        std::is_same_v<
            decltype(std::declval<const CampusDirectoryProjection>().campuses()),
            const std::vector<CampusSummary>&
            >
        );
    static_assert(
        !std::is_pointer_v<
            decltype(std::declval<const CampusDirectoryProjection>().findByKey(
                std::string_view{}
                ))
            >
        );

    QVERIFY(true);
}

QTEST_APPLESS_MAIN(NextApplicationCampusDirectoryTests)

#include "next_application_campus_directory_tests.moc"
