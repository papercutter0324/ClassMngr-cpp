#include "features/my_info/data/typed_signature_renderer.h"

#include <QColor>
#include <QImage>
#include <QtTest>

class TypedSignatureRendererTests : public QObject
{
    Q_OBJECT

private slots:
    void rendersTypedSignatureAsTransparentPng();
    void acceptsEachSupportedTypeface();
    void replacesInvalidStoredTypefaceWithDefault();
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

QTEST_MAIN(TypedSignatureRendererTests)

#include "typed_signature_renderer_tests.moc"
