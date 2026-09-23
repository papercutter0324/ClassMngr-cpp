#include "core/application_services.h"
#include "data/data_service.h"
#include "features/my_info/data/signature_image_processor.h"
#include "next/platform/application_services_personal_signature_image_port.h"

#include <QBuffer>
#include <QImage>
#include <QTemporaryDir>
#include <QUuid>
#include <QtTest/QtTest>

#include <cstddef>

using namespace ClassMngr::Next::Platform;

namespace
{

constexpr auto SignatureImageKey = "myInfo/signatureImage";
constexpr auto WrongKey = "myInfo/signature";
constexpr auto UnrelatedKey = "myInfo/signatureMode";

QString databasePath(QTemporaryDir& directory)
{
    return directory.filePath(
        QStringLiteral("personal-signature-image-%1.tps").arg(
            QUuid::createUuid().toString(QUuid::WithoutBraces)
            )
        );
}

bool openDatabase(
    ApplicationServices& services,
    QTemporaryDir& directory
    )
{
    return services.openDatabase(databasePath(directory)).has_value();
}

QByteArray sourcePng()
{
    QImage source(2, 1, QImage::Format_RGB32);
    source.setPixelColor(0, 0, Qt::white);
    source.setPixelColor(1, 0, Qt::black);

    QByteArray encoded;
    QBuffer buffer(&encoded);
    if (!buffer.open(QIODevice::WriteOnly) || !source.save(&buffer, "PNG"))
    {
        return {};
    }

    return encoded;
}

QByteArray preparedBytes(
    const std::string& value
    )
{
    return QByteArray::fromStdString(value);
}

} // namespace

class NextPlatformApplicationServicesPersonalSignatureImagePortTests final
    : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void validBase64ImageIsPreparedOnceForTheExactKey();
    void missingAndUnavailableSettingsReturnEmpty();
    void corruptBase64ReturnsEmpty();
    void preservesUnrelatedSettingsAndDoesNotWrite();

private:
    QTemporaryDir m_directory;
};

void NextPlatformApplicationServicesPersonalSignatureImagePortTests::
initTestCase()
{
    QVERIFY(m_directory.isValid());
}

void NextPlatformApplicationServicesPersonalSignatureImagePortTests::
validBase64ImageIsPreparedOnceForTheExactKey()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    QVERIFY(services.dataService());

    const QByteArray source = sourcePng();
    QVERIFY(!source.isEmpty());
    QVERIFY(
        services.dataService()->saveSetting(
            QString::fromUtf8(SignatureImageKey),
            QString::fromLatin1(source.toBase64())
            )
        );

    ApplicationServicesPersonalSignatureImagePort port(services);
    const QByteArray actual = preparedBytes(port.read());
    const QByteArray expected =
        SignatureImage::prepareForEmbedding(source);

    QVERIFY(!expected.isEmpty());
    QCOMPARE(actual, expected);

    const QImage decoded = QImage::fromData(actual, "PNG");
    QVERIFY(!decoded.isNull());
    QVERIFY(decoded.hasAlphaChannel());
    QCOMPARE(decoded.pixelColor(0, 0).alpha(), 0);
    QCOMPARE(decoded.pixelColor(1, 0).alpha(), 255);

    const auto wrongKey = services.dataService()->loadSetting(
        QString::fromUtf8(WrongKey)
        );
    QVERIFY(wrongKey);
    QVERIFY(!wrongKey->isValid());
}

void NextPlatformApplicationServicesPersonalSignatureImagePortTests::
missingAndUnavailableSettingsReturnEmpty()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));

    ApplicationServicesPersonalSignatureImagePort port(services);
    QVERIFY(port.read().empty());

    const auto missing = services.dataService()->loadSetting(
        QString::fromUtf8(SignatureImageKey)
        );
    QVERIFY(missing);
    QVERIFY(!missing->isValid());

    ApplicationServices unavailableServices;
    ApplicationServicesPersonalSignatureImagePort unavailablePort(
        unavailableServices
        );
    QVERIFY(unavailablePort.read().empty());

    ApplicationServices* nullServices = nullptr;
    ApplicationServicesPersonalSignatureImagePort nullPort(nullServices);
    QVERIFY(nullPort.read().empty());
}

void NextPlatformApplicationServicesPersonalSignatureImagePortTests::
corruptBase64ReturnsEmpty()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    QVERIFY(services.dataService());
    QVERIFY(
        services.dataService()->saveSetting(
            QString::fromUtf8(SignatureImageKey),
            QStringLiteral("%%%not-base64%%")
            )
        );

    ApplicationServicesPersonalSignatureImagePort port(services);
    QVERIFY(port.read().empty());
}

void NextPlatformApplicationServicesPersonalSignatureImagePortTests::
preservesUnrelatedSettingsAndDoesNotWrite()
{
    ApplicationServices services;
    QVERIFY(openDatabase(services, m_directory));
    QVERIFY(services.dataService());
    QVERIFY(
        services.dataService()->saveSetting(
            QString::fromUtf8(UnrelatedKey),
            1
            )
        );

    ApplicationServicesPersonalSignatureImagePort port(services);
    QVERIFY(port.read().empty());

    const auto unrelated = services.dataService()->loadSetting(
        QString::fromUtf8(UnrelatedKey)
        );
    QVERIFY(unrelated);
    QCOMPARE(unrelated->toInt(), 1);

    const auto stillMissing = services.dataService()->loadSetting(
        QString::fromUtf8(SignatureImageKey)
        );
    QVERIFY(stillMissing);
    QVERIFY(!stillMissing->isValid());
}

QTEST_MAIN(NextPlatformApplicationServicesPersonalSignatureImagePortTests)

#include "next_platform_application_services_personal_signature_image_port_tests.moc"
