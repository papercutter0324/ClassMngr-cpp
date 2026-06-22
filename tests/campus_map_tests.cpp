#include "features/campus/data/campus_json_repository.h"
#include "features/campus/ui/campus_map_preview.h"
#include "ui/shared/widgets/text_fit_push_button.h"

#include <QFile>
#include <QFrame>
#include <QHBoxLayout>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QPushButton>
#include <QTemporaryDir>
#include <QtTest>

class CampusMapTests : public QObject
{
    Q_OBJECT

private slots:
    void mapConfigurationRoundTrips();
    void legacyAddressNoteMigratesToSharedField();
    void legacyImageMainRemainsSupported();
    void explicitEmptyMapDoesNotRestoreLegacyImage();
    void mapUrlsRequireAbsoluteHttps();
    void bundledCampusImagesLoad();
    void gallerySwitchesAtBreakpoint();
    void mapControlsStayCenteredOverFirstImage();
    void wideMapControlsAreNotCompressed();
    void galleryHandlesMissingImagesAndCampusChanges();
};

void CampusMapTests::mapConfigurationRoundTrips()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    CampusJsonRepository repository(
        directory.path()
        );

    CampusInfo campus;
    campus.id = QStringLiteral("test-campus");
    campus.campusName = QStringLiteral("Test Campus");
    campus.mapImagePaths =
        {
            QStringLiteral(":/assets/campuses/test/map.png"),
            QStringLiteral(":/assets/campuses/test/building.png")
        };
    campus.naverMapUrl =
        QStringLiteral("https://map.naver.com/example");
    campus.kakaoMapUrl =
        QStringLiteral("https://map.kakao.com/example");
    campus.directionsNote =
        QStringLiteral("Use the side entrance.");

    QJsonObject housingLinks;
    housingLinks.insert(
        QStringLiteral("naver"),
        QStringLiteral("https://map.naver.com/housing")
        );
    housingLinks.insert(
        QStringLiteral("kakao"),
        QStringLiteral("https://map.kakao.com/housing")
        );

    QJsonObject housingMap;
    housingMap.insert(
        QStringLiteral("images"),
        QJsonArray{
            QStringLiteral(":/assets/campuses/test/housing.png")
            }
        );
    housingMap.insert(
        QStringLiteral("links"),
        housingLinks
        );

    QJsonObject housing;
    housing.insert(
        QStringLiteral("name"),
        QStringLiteral("Test Housing")
        );
    housing.insert(
        QStringLiteral("map"),
        housingMap
        );
    housing.insert(
        QStringLiteral("addr_note"),
        QStringLiteral("Ask security for the key.")
        );
    campus.housingLocations.append(housing);

    const Status saved =
        repository.saveCampus(campus);
    QVERIFY(saved.has_value());

    const std::optional<CampusInfo> loaded =
        repository.loadCampus(campus.id);
    QVERIFY(loaded.has_value());
    QCOMPARE(
        loaded->mapImagePaths,
        campus.mapImagePaths
        );
    QCOMPARE(loaded->naverMapUrl, campus.naverMapUrl);
    QCOMPARE(loaded->kakaoMapUrl, campus.kakaoMapUrl);
    QCOMPARE(
        loaded->housingLocations,
        campus.housingLocations
        );
    QCOMPARE(
        loaded->directionsNote,
        campus.directionsNote
        );

    QFile file(
        repository.filePathForCampusId(campus.id)
        );
    QVERIFY(file.open(QIODevice::ReadOnly));

    const QJsonObject root =
        QJsonDocument::fromJson(
            file.readAll()
            ).object();

    const QJsonObject savedDirections =
        root.value(QStringLiteral("directions")).toObject();

    QCOMPARE(
        savedDirections
            .value(QStringLiteral("addr_note"))
            .toString(),
        campus.directionsNote
        );
    QVERIFY(
        !savedDirections
            .value(QStringLiteral("en"))
            .toObject()
            .contains(QStringLiteral("addr_note"))
        );

    QCOMPARE(
        root.value(QStringLiteral("image_main")).toString(),
        campus.mapImagePaths.constFirst()
        );
    QCOMPARE(
        root
            .value(QStringLiteral("map"))
            .toObject()
            .value(QStringLiteral("images"))
            .toArray()
            .size(),
        2
        );
}

void CampusMapTests::legacyAddressNoteMigratesToSharedField()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    CampusJsonRepository repository(
        directory.path()
        );

    QJsonObject englishAddress;
    englishAddress.insert(
        QStringLiteral("addr_note"),
        QStringLiteral("Legacy shared note")
        );

    QJsonObject directions;
    directions.insert(
        QStringLiteral("en"),
        englishAddress
        );
    directions.insert(
        QStringLiteral("kr"),
        QJsonObject()
        );

    QJsonObject root;
    root.insert(
        QStringLiteral("id"),
        QStringLiteral("legacy-note")
        );
    root.insert(
        QStringLiteral("campus_name"),
        QStringLiteral("Legacy Note")
        );
    root.insert(
        QStringLiteral("directions"),
        directions
        );

    QFile file(
        repository.filePathForCampusId(
            QStringLiteral("legacy-note")
            )
        );
    QVERIFY(file.open(QIODevice::WriteOnly));

    const QByteArray legacyJson =
        QJsonDocument(root).toJson();

    QCOMPARE(
        file.write(legacyJson),
        static_cast<qint64>(legacyJson.size())
        );
    file.close();

    const std::optional<CampusInfo> loaded =
        repository.loadCampus(
            QStringLiteral("legacy-note")
            );

    QVERIFY(loaded.has_value());
    QCOMPARE(
        loaded->directionsNote,
        QStringLiteral("Legacy shared note")
        );

    const Status saved =
        repository.saveCampus(*loaded);
    QVERIFY(saved.has_value());

    QVERIFY(file.open(QIODevice::ReadOnly));
    const QJsonObject savedRoot =
        QJsonDocument::fromJson(file.readAll()).object();
    const QJsonObject savedDirections =
        savedRoot
            .value(QStringLiteral("directions"))
            .toObject();

    QCOMPARE(
        savedDirections
            .value(QStringLiteral("addr_note"))
            .toString(),
        QStringLiteral("Legacy shared note")
        );
    QVERIFY(
        !savedDirections
            .value(QStringLiteral("en"))
            .toObject()
            .contains(QStringLiteral("addr_note"))
        );
}

void CampusMapTests::legacyImageMainRemainsSupported()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    CampusJsonRepository repository(
        directory.path()
        );

    QFile file(
        repository.filePathForCampusId(
            QStringLiteral("legacy")
            )
        );
    QVERIFY(file.open(QIODevice::WriteOnly));

    QJsonObject root;
    root.insert(QStringLiteral("id"), QStringLiteral("legacy"));
    root.insert(
        QStringLiteral("campus_name"),
        QStringLiteral("Legacy")
        );
    root.insert(
        QStringLiteral("image_main"),
        QStringLiteral("/legacy/map.png")
        );

    file.write(
        QJsonDocument(root).toJson()
        );
    file.close();

    const std::optional<CampusInfo> loaded =
        repository.loadCampus(
            QStringLiteral("legacy")
            );
    QVERIFY(loaded.has_value());
    QCOMPARE(
        loaded->mapImagePaths,
        QStringList{QStringLiteral("/legacy/map.png")}
        );
}

void CampusMapTests::explicitEmptyMapDoesNotRestoreLegacyImage()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    CampusJsonRepository repository(
        directory.path()
        );

    QFile file(
        repository.filePathForCampusId(
            QStringLiteral("empty-map")
            )
        );
    QVERIFY(file.open(QIODevice::WriteOnly));

    QJsonObject map;
    map.insert(QStringLiteral("images"), QJsonArray());
    map.insert(QStringLiteral("links"), QJsonObject());

    QJsonObject root;
    root.insert(QStringLiteral("id"), QStringLiteral("empty-map"));
    root.insert(
        QStringLiteral("campus_name"),
        QStringLiteral("Empty Map")
        );
    root.insert(
        QStringLiteral("image_main"),
        QStringLiteral("/legacy/map.png")
        );
    root.insert(QStringLiteral("map"), map);

    file.write(
        QJsonDocument(root).toJson()
        );
    file.close();

    const std::optional<CampusInfo> loaded =
        repository.loadCampus(
            QStringLiteral("empty-map")
            );
    QVERIFY(loaded.has_value());
    QVERIFY(loaded->mapImagePaths.isEmpty());
}

void CampusMapTests::mapUrlsRequireAbsoluteHttps()
{
    QVERIFY(CampusMapPreview::isValidMapUrl(
        QStringLiteral("https://map.naver.com/example")
        ));
    QVERIFY(CampusMapPreview::isValidMapUrl(
        QStringLiteral("  https://map.kakao.com/example  ")
        ));
    QVERIFY(!CampusMapPreview::isValidMapUrl(
        QStringLiteral("http://map.naver.com/example")
        ));
    QVERIFY(!CampusMapPreview::isValidMapUrl(
        QStringLiteral("map.naver.com/example")
        ));
    QVERIFY(!CampusMapPreview::isValidMapUrl(
        QStringLiteral("not a URL")
        ));
}

void CampusMapTests::bundledCampusImagesLoad()
{
    CampusMapPreview preview;
    preview.setImagePaths(
        {
            QStringLiteral(
                ":/assets/campuses/bundang/bundang_map.png"
                ),
            QStringLiteral(
                ":/assets/campuses/bundang/bundang_building.png"
                )
        }
        );

    QCOMPARE(preview.displayedImageCount(), 2);

    preview.show();
    preview.resize(
        CampusMapPreview::HorizontalBreakpoint,
        1000
        );
    QCoreApplication::processEvents();

    const QList<QLabel*> imageLabels =
        preview.findChildren<QLabel*>(
            QStringLiteral("campusMapImage")
            );
    QCOMPARE(imageLabels.size(), 2);
    QCOMPARE(
        imageLabels.at(0)->pixmap().height(),
        imageLabels.at(1)->pixmap().height()
        );

    preview.resize(
        CampusMapPreview::HorizontalBreakpoint - 1,
        1000
        );
    QCoreApplication::processEvents();

    QCOMPARE(
        imageLabels.at(0)->pixmap().size(),
        QSize(759, 710)
        );
    QCOMPARE(
        imageLabels.at(1)->pixmap().size(),
        QSize(759, 532)
        );
    QVERIFY(imageLabels.at(1)->geometry().bottom()
        < preview.height());
}

void CampusMapTests::gallerySwitchesAtBreakpoint()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const QString wideImagePath =
        directory.filePath(QStringLiteral("wide.png"));
    const QString tallImagePath =
        directory.filePath(QStringLiteral("tall.png"));

    QVERIFY(QImage(400, 200, QImage::Format_ARGB32)
        .save(wideImagePath));
    QVERIFY(QImage(200, 400, QImage::Format_ARGB32)
        .save(tallImagePath));

    CampusMapPreview preview;
    preview.setImagePaths(
        {
            wideImagePath,
            tallImagePath
        }
        );
    preview.show();

    preview.resize(
        CampusMapPreview::HorizontalBreakpoint - 1,
        1000
        );
    QCoreApplication::processEvents();

    QVERIFY(!preview.isHorizontal());
    QCOMPARE(preview.displayedImageCount(), 2);

    QList<QLabel*> imageLabels =
        preview.findChildren<QLabel*>(
            QStringLiteral("campusMapImage")
            );
    QCOMPARE(imageLabels.size(), 2);
    QCOMPARE(imageLabels.at(0)->pixmap().size(), QSize(759, 380));
    QCOMPARE(imageLabels.at(1)->pixmap().size(), QSize(759, 1518));
    QCOMPARE(
        preview.height(),
        preview.heightForWidth(
            CampusMapPreview::HorizontalBreakpoint - 1
            )
        );
    QVERIFY(imageLabels.at(0)->geometry().bottom()
        < preview.height());
    QVERIFY(imageLabels.at(1)->geometry().bottom()
        < preview.height());

    preview.resize(
        CampusMapPreview::HorizontalBreakpoint,
        1000
        );
    QCoreApplication::processEvents();

    QVERIFY(preview.isHorizontal());

    imageLabels =
        preview.findChildren<QLabel*>(
            QStringLiteral("campusMapImage")
            );
    QCOMPARE(imageLabels.size(), 2);
    QVERIFY(
        imageLabels.at(0)->width()
        > imageLabels.at(1)->width()
        );
    QCOMPARE(
        imageLabels.at(0)->pixmap().height(),
        imageLabels.at(1)->pixmap().height()
        );
    QCOMPARE(imageLabels.at(0)->pixmap().size(), QSize(580, 290));
    QCOMPARE(imageLabels.at(1)->pixmap().size(), QSize(145, 290));
    QCOMPARE(
        preview.height(),
        preview.heightForWidth(
            CampusMapPreview::HorizontalBreakpoint
            )
        );

    const auto* divider =
        preview.findChild<QFrame*>(
            QStringLiteral("campusMapDivider")
            );
    QVERIFY(divider);
    QVERIFY(divider->isVisible());
    QCOMPARE(divider->frameShape(), QFrame::VLine);
    QVERIFY(divider->geometry().left()
        > imageLabels.at(0)->geometry().right());
    QVERIFY(divider->geometry().right()
        < imageLabels.at(1)->geometry().left());

    preview.resize(2000, 1000);
    QCoreApplication::processEvents();
    QCOMPARE(
        imageLabels.at(0)->pixmap().height(),
        CampusMapPreview::MaximumImageHeight
        );
    QCOMPARE(
        imageLabels.at(1)->pixmap().height(),
        CampusMapPreview::MaximumImageHeight
        );
    QCOMPARE(imageLabels.at(0)->pixmap().size(), QSize(720, 360));
    QCOMPARE(imageLabels.at(1)->pixmap().size(), QSize(180, 360));
}

void CampusMapTests::galleryHandlesMissingImagesAndCampusChanges()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const QString imagePath =
        directory.filePath(QStringLiteral("map.png"));
    QVERIFY(QImage(320, 180, QImage::Format_ARGB32)
        .save(imagePath));

    CampusMapPreview preview;

    preview.setImagePaths(
        {
            QStringLiteral("/missing/map.png")
        }
        );
    QVERIFY(!preview.hasImages());
    QCOMPARE(preview.displayedImageCount(), 0);

    preview.setImagePaths({imagePath});
    QVERIFY(preview.hasImages());
    QCOMPARE(preview.displayedImageCount(), 1);

    preview.setImagePaths({});
    QVERIFY(!preview.hasImages());
    QCOMPARE(preview.displayedImageCount(), 0);
}

void CampusMapTests::mapControlsStayCenteredOverFirstImage()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const QString mapImagePath =
        directory.filePath(QStringLiteral("map.png"));
    const QString buildingImagePath =
        directory.filePath(QStringLiteral("building.png"));

    QVERIFY(QImage(400, 300, QImage::Format_ARGB32)
        .save(mapImagePath));
    QVERIFY(QImage(300, 400, QImage::Format_ARGB32)
        .save(buildingImagePath));

    CampusMapPreview preview;
    auto* controls = new QPushButton(QStringLiteral("Open map"));
    controls->setObjectName(QStringLiteral("mapControls"));
    preview.setMapControls(controls);
    preview.setImagePaths({mapImagePath, buildingImagePath});
    preview.show();

    const auto verifyCentered = [&preview, controls]()
    {
        const QList<QLabel*> imageLabels =
            preview.findChildren<QLabel*>(
                QStringLiteral("campusMapImage")
                );
        QCOMPARE(imageLabels.size(), 2);

        const int controlsCenter =
            controls->geometry().center().x();
        const int mapCenter =
            imageLabels.constFirst()->geometry().center().x();

        QVERIFY(qAbs(controlsCenter - mapCenter) <= 1);

        const auto* mapTitle =
            preview.findChild<QLabel*>(
                QStringLiteral("mapViewTitle")
                );
        const auto* buildingTitle =
            preview.findChild<QLabel*>(
                QStringLiteral("buildingViewTitle")
                );
        QVERIFY(mapTitle);
        QVERIFY(buildingTitle);
        QCOMPARE(mapTitle->text(), QStringLiteral("Map View"));
        QCOMPARE(
            buildingTitle->text(),
            QStringLiteral("Building View")
            );
        QVERIFY(mapTitle->geometry().bottom()
            < imageLabels.constFirst()->geometry().top());
        QVERIFY(buildingTitle->geometry().bottom()
            < imageLabels.at(1)->geometry().top());
        QVERIFY(controls->geometry().top()
            > imageLabels.constFirst()->geometry().bottom());

        const auto* divider =
            preview.findChild<QFrame*>(
                QStringLiteral("campusMapDivider")
                );
        QVERIFY(divider);
        QVERIFY(divider->isVisible());

        if (preview.isHorizontal())
        {
            QCOMPARE(divider->frameShape(), QFrame::VLine);
        }
        else
        {
            QCOMPARE(divider->frameShape(), QFrame::HLine);
            QVERIFY(divider->geometry().top()
                > controls->geometry().bottom());
            QVERIFY(divider->geometry().bottom()
                < buildingTitle->geometry().top());
        }
    };

    preview.resize(
        CampusMapPreview::HorizontalBreakpoint,
        1000
        );
    QCoreApplication::processEvents();
    verifyCentered();

    preview.resize(
        CampusMapPreview::HorizontalBreakpoint - 1,
        1000
        );
    QCoreApplication::processEvents();
    verifyCentered();
}

void CampusMapTests::wideMapControlsAreNotCompressed()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const QString mapImagePath =
        directory.filePath(QStringLiteral("map.png"));
    const QString buildingImagePath =
        directory.filePath(QStringLiteral("building.png"));

    QVERIFY(QImage(400, 300, QImage::Format_ARGB32)
        .save(mapImagePath));
    QVERIFY(QImage(300, 400, QImage::Format_ARGB32)
        .save(buildingImagePath));

    CampusMapPreview preview;
    auto* controls = new QWidget;
    auto* layout = new QHBoxLayout(controls);

    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);
    layout->addStretch();

    auto* naverButton =
        new TextFitPushButton(
            QStringLiteral("Naver Maps"),
            controls
            );
    auto* kakaoButton =
        new TextFitPushButton(
            QStringLiteral("Kakao Maps"),
            controls
            );

    QFont extraLargeFont = naverButton->font();
    extraLargeFont.setPointSize(
        extraLargeFont.pointSize() + 4
        );
    naverButton->setFont(extraLargeFont);
    kakaoButton->setFont(extraLargeFont);

    layout->addWidget(naverButton);
    layout->addWidget(kakaoButton);
    layout->addStretch();

    preview.setMapControls(controls);
    preview.setImagePaths({mapImagePath, buildingImagePath});
    preview.resize(
        CampusMapPreview::HorizontalBreakpoint,
        1000
        );
    preview.show();
    QCoreApplication::processEvents();

    QVERIFY(naverButton->width() >= naverButton->sizeHint().width());
    QVERIFY(kakaoButton->width() >= kakaoButton->sizeHint().width());
}

QTEST_MAIN(CampusMapTests)

#include "campus_map_tests.moc"
