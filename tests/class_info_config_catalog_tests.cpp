#include "domain/config/class_details_catalog.h"
#include "features/classes/config/class_info_config.h"

#include <QtTest/QtTest>

#include <array>
#include <string_view>
#include <vector>

namespace
{

QStringList toQStringList(const std::vector<std::string_view>& values)
{
    QStringList result;
    for (const std::string_view value : values)
    {
        result.push_back(QString::fromUtf8(
            value.data(), static_cast<qsizetype>(value.size())
            ));
    }
    return result;
}

} // namespace

class ClassInfoConfigCatalogTests final : public QObject
{
    Q_OBJECT

private slots:
    void knownGradeWithUnknownLevelKeepsLegacyBookFallbacks();
};

void ClassInfoConfigCatalogTests::
    knownGradeWithUnknownLevelKeepsLegacyBookFallbacks()
{
    using ClassMngr::Domain::ClassDetailsCatalog;

    const std::vector<std::string_view> expectedReadingBooks{
        "",
        "Reading Smart Step 1 Book 1",
        "Reading Smart Step 1 Book 2",
        "Reading Smart Step 1 Book 3",
        "Reading Smart Step 1 Book 4",
        "Reading Smart Step 1 Book 5",
        "Reading Smart Step 1 Book 6",
        "Reading Smart Step 1 Book 7",
        "Reading Smart Step 2 Book 1",
        "Reading Smart Step 2 Book 2",
        "Reading Smart Step 2 Book 3",
        "Reading Smart Step 2 Book 4",
        "Reading Smart Step 2 Book 5",
        "Reading Smart Step 2 Book 6",
        "Reading Smart Step 2 Book 7"
    };
    const std::vector<std::string_view> expectedEssayBooks{"N/A"};
    const std::array<std::string_view, 6> grades{
        "E4", "E5", "E6", "M1", "M2", "M3"
    };

    for (const std::string_view grade : grades)
    {
        const auto sharedReadingBooks =
            ClassDetailsCatalog::readingBooks(grade, "not-a-level");
        const auto sharedEssayBooks =
            ClassDetailsCatalog::essayBooks(grade, "not-a-level");
        QVERIFY(sharedReadingBooks == expectedReadingBooks);
        QVERIFY(sharedEssayBooks == expectedEssayBooks);

        const QString qGrade = QString::fromUtf8(
            grade.data(), static_cast<qsizetype>(grade.size())
            );
        QCOMPARE(
            ClassInfoConfig::readingBooks(qGrade, QStringLiteral("not-a-level")),
            toQStringList(expectedReadingBooks)
            );
        QCOMPARE(
            ClassInfoConfig::essayBooks(qGrade, QStringLiteral("not-a-level")),
            toQStringList(expectedEssayBooks)
            );
    }
}

QTEST_APPLESS_MAIN(ClassInfoConfigCatalogTests)

#include "class_info_config_catalog_tests.moc"
