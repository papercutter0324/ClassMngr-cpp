#include "features/my_info/data/typed_signature_renderer.h"

#include <QColor>
#include <QImage>
#include <QtTest>

#include <array>
#include <memory>
#include <utility>
#include <vector>

namespace
{
struct FontRegistrationState final
{
    int releasedCount = 0;
    std::vector<ClassMngr::Core::FontResource> requests;
    std::vector<std::weak_ptr<const ClassMngr::Core::FontRegistrationLease>> leases;
};

class TrackedFontRegistrationLease final
    : public ClassMngr::Core::FontRegistrationLease
{
public:
    TrackedFontRegistrationLease(
        std::shared_ptr<FontRegistrationState> state,
        QString familyName)
        : m_state(std::move(state))
        , m_familyName(std::move(familyName))
    {
    }

    ~TrackedFontRegistrationLease() override
    {
        ++m_state->releasedCount;
    }

    [[nodiscard]] const QString& familyName() const noexcept override
    {
        return m_familyName;
    }

private:
    std::shared_ptr<FontRegistrationState> m_state;
    QString m_familyName;
};

class TrackedFontRegistrationPort final
    : public ClassMngr::Core::FontRegistrationPort
{
public:
    TrackedFontRegistrationPort(
        std::shared_ptr<FontRegistrationState> state,
        ClassMngr::Core::FontResource unavailableResource)
        : m_state(std::move(state))
        , m_unavailableResource(unavailableResource)
    {
    }

    [[nodiscard]] std::expected<
        std::shared_ptr<const ClassMngr::Core::FontRegistrationLease>, QString>
    loadFont(ClassMngr::Core::FontResource resource) override
    {
        m_state->requests.push_back(resource);
        if (resource == m_unavailableResource)
        {
            return std::unexpected(QStringLiteral("test font is unavailable"));
        }

        QString familyName;
        switch (resource)
        {
        case ClassMngr::Core::FontResource::Inter:
            familyName = QStringLiteral("Inter Injected");
            break;
        case ClassMngr::Core::FontResource::Pretendard:
            familyName = QStringLiteral("Pretendard Injected");
            break;
        case ClassMngr::Core::FontResource::JustAnotherHand:
            familyName = QStringLiteral("Just Another Hand Injected");
            break;
        case ClassMngr::Core::FontResource::DancingScript:
            familyName = QStringLiteral("Dancing Script Injected");
            break;
        case ClassMngr::Core::FontResource::GreatVibes:
            familyName = QStringLiteral("Great Vibes Injected");
            break;
        case ClassMngr::Core::FontResource::Caveat:
            familyName = QStringLiteral("Caveat Injected");
            break;
        }

        auto lease = std::make_shared<TrackedFontRegistrationLease>(
            m_state,
            familyName);
        m_state->leases.push_back(lease);
        return std::shared_ptr<const ClassMngr::Core::FontRegistrationLease>(
            std::move(lease));
    }

private:
    std::shared_ptr<FontRegistrationState> m_state;
    ClassMngr::Core::FontResource m_unavailableResource;
};
}

class TypedSignatureRendererTests : public QObject
{
    Q_OBJECT

private slots:
    void rendersTypedSignatureAsTransparentPng();
    void acceptsEachSupportedTypeface();
    void replacesInvalidStoredTypefaceWithDefault();
    void injectedPortOwnsSignatureFontLeasesAndUsesFallbackOnFailure();
};

void TypedSignatureRendererTests::rendersTypedSignatureAsTransparentPng()
{
    const QByteArray signature =
        TypedSignature::renderForEmbedding(
            QStringLiteral("Alex Morgan"),
            TypedSignatureFont::DancingScript
            );
    QVERIFY(!signature.isEmpty());

    QImage image;
    QVERIFY(image.loadFromData(signature, "PNG"));
    QVERIFY(image.hasAlphaChannel());

    bool containsInk = false;
    bool containsTransparentBackground = false;

    for (int y = 0; y < image.height(); ++y)
    {
        for (int x = 0; x < image.width(); ++x)
        {
            const QColor pixel = image.pixelColor(x, y);
            containsInk = containsInk || pixel.alpha() > 0;
            containsTransparentBackground =
                containsTransparentBackground || pixel.alpha() == 0;
        }
    }

    QVERIFY(containsInk);
    QVERIFY(containsTransparentBackground);
}

void TypedSignatureRendererTests::acceptsEachSupportedTypeface()
{
    const QList<TypedSignatureFont> fonts = {
        TypedSignatureFont::JustAnotherHand,
        TypedSignatureFont::DancingScript,
        TypedSignatureFont::GreatVibes,
        TypedSignatureFont::Caveat
    };

    for (const TypedSignatureFont font : fonts)
    {
        QVERIFY(!TypedSignature::displayName(font).isEmpty());
        QVERIFY(
            !TypedSignature::renderForEmbedding(
                QStringLiteral("Alex Morgan"),
                font
                ).isEmpty()
            );
    }
}

void TypedSignatureRendererTests::replacesInvalidStoredTypefaceWithDefault()
{
    QCOMPARE(
        TypedSignature::fontFromStoredValue(-1),
        TypedSignatureFont::JustAnotherHand
        );
    QCOMPARE(
        TypedSignature::fontFromStoredValue(99),
        TypedSignatureFont::JustAnotherHand
        );
}

void TypedSignatureRendererTests::
injectedPortOwnsSignatureFontLeasesAndUsesFallbackOnFailure()
{
    auto state = std::make_shared<FontRegistrationState>();
    {
        TrackedFontRegistrationPort port(
            state,
            ClassMngr::Core::FontResource::DancingScript);
        const TypedSignatureFontSet fonts = TypedSignatureFontSet::load(port);

        const std::array expectedResources{
            ClassMngr::Core::FontResource::JustAnotherHand,
            ClassMngr::Core::FontResource::DancingScript,
            ClassMngr::Core::FontResource::GreatVibes,
            ClassMngr::Core::FontResource::Caveat
        };
        QCOMPARE(state->requests.size(), expectedResources.size());
        for (std::size_t index = 0; index < expectedResources.size(); ++index)
        {
            QCOMPARE(state->requests.at(index), expectedResources.at(index));
        }

        QCOMPARE(
            fonts.familyFor(TypedSignatureFont::JustAnotherHand),
            QStringLiteral("Just Another Hand Injected"));
        QCOMPARE(
            fonts.familyFor(TypedSignatureFont::DancingScript),
            QStringLiteral("cursive"));
        QCOMPARE(
            fonts.familyFor(TypedSignatureFont::GreatVibes),
            QStringLiteral("Great Vibes Injected"));
        QCOMPARE(
            fonts.familyFor(TypedSignatureFont::Caveat),
            QStringLiteral("Caveat Injected"));
        QCOMPARE(
            TypedSignature::fontFor(
                TypedSignatureFont::GreatVibes,
                24,
                &fonts).family(),
            QStringLiteral("Great Vibes Injected"));

        QCOMPARE(state->releasedCount, 0);
        QCOMPARE(state->leases.size(), std::size_t{3});
        for (const auto& lease : state->leases)
        {
            QVERIFY(!lease.expired());
        }
    }

    QCOMPARE(state->releasedCount, 3);
    for (const auto& lease : state->leases)
    {
        QVERIFY(lease.expired());
    }
}

QTEST_MAIN(TypedSignatureRendererTests)

#include "typed_signature_renderer_tests.moc"
