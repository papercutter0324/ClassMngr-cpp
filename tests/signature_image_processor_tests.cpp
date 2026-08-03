#include "features/my_info/data/signature_image_processor.h"

#include <QBuffer>
#include <QColor>
#include <QImage>
#include <QtTest>

class SignatureImageProcessorTests : public QObject
{
    Q_OBJECT

private slots:
    void whiteAndNearWhitePixelsBecomeProgressivelyTransparent();
    void antialiasedColorIsRecoveredWithoutAWhiteHalo();
    void existingTransparencyIsPreserved();
    void preparedImageIsAnAlphaPng();
    void encodedExistingSignatureIsNormalized();
};

void SignatureImageProcessorTests::
    whiteAndNearWhitePixelsBecomeProgressivelyTransparent()
{
    QImage source(4, 1, QImage::Format_ARGB32);
    source.setPixelColor(0, 0, QColor(255, 255, 255));
    source.setPixelColor(1, 0, QColor(250, 250, 250));
    source.setPixelColor(2, 0, QColor(192, 192, 192));
    source.setPixelColor(3, 0, QColor(0, 0, 0));

    const QImage result =
        SignatureImage::removeWhiteBackground(source);

    QCOMPARE(result.pixelColor(0, 0).alpha(), 0);
    QCOMPARE(result.pixelColor(1, 0), QColor(0, 0, 0, 5));
    QCOMPARE(result.pixelColor(2, 0), QColor(0, 0, 0, 63));
    QCOMPARE(result.pixelColor(3, 0), QColor(0, 0, 0, 255));
}

void SignatureImageProcessorTests::
    antialiasedColorIsRecoveredWithoutAWhiteHalo()
{
    QImage source(1, 1, QImage::Format_ARGB32);
    source.setPixelColor(0, 0, QColor(128, 192, 255));

    const QColor result =
        SignatureImage::removeWhiteBackground(source)
            .pixelColor(0, 0);

    QCOMPARE(result, QColor(0, 129, 255, 127));
}

void SignatureImageProcessorTests::
    existingTransparencyIsPreserved()
{
    QImage source(2, 1, QImage::Format_ARGB32);
    source.setPixelColor(0, 0, QColor(0, 0, 0, 128));
    source.setPixelColor(1, 0, QColor(128, 128, 128, 128));

    const QImage result =
        SignatureImage::removeWhiteBackground(source);

    QCOMPARE(result.pixelColor(0, 0), QColor(0, 0, 0, 128));
    QCOMPARE(result.pixelColor(1, 0), QColor(0, 0, 0, 64));
}

void SignatureImageProcessorTests::preparedImageIsAnAlphaPng()
{
    QImage source(2, 1, QImage::Format_RGB32);
    source.setPixelColor(0, 0, Qt::white);
    source.setPixelColor(1, 0, Qt::black);

    const QByteArray encoded =
        SignatureImage::prepareForEmbedding(source);
    QVERIFY(!encoded.isEmpty());

    const QImage decoded =
        QImage::fromData(encoded, "PNG");
    QVERIFY(!decoded.isNull());
    QVERIFY(decoded.hasAlphaChannel());
    QCOMPARE(decoded.pixelColor(0, 0).alpha(), 0);
    QCOMPARE(decoded.pixelColor(1, 0).alpha(), 255);
}

void SignatureImageProcessorTests::
    encodedExistingSignatureIsNormalized()
{
    QImage source(2, 1, QImage::Format_RGB32);
    source.setPixelColor(0, 0, Qt::white);
    source.setPixelColor(1, 0, QColor(128, 128, 128));

    QByteArray opaquePng;
    QBuffer buffer(&opaquePng);
    QVERIFY(buffer.open(QIODevice::WriteOnly));
    QVERIFY(source.save(&buffer, "PNG"));

    const QByteArray normalized =
        SignatureImage::prepareForEmbedding(opaquePng);
    const QImage decoded =
        QImage::fromData(normalized, "PNG");

    QVERIFY(!decoded.isNull());
    QCOMPARE(decoded.pixelColor(0, 0).alpha(), 0);
    QCOMPARE(decoded.pixelColor(1, 0), QColor(0, 0, 0, 127));
}

QTEST_APPLESS_MAIN(SignatureImageProcessorTests)

#include "signature_image_processor_tests.moc"
