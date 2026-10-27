#include "features/teacher/ui/teacher_info_page.h"
#include "next/application/preferences_model.h"
#include "next/application/teacher_profile_edit_use_case.h"
#include "next/domain/teacher_profile.h"
#include "next/resources/resource_catalog.h"
#include "next/resources/resource_loader.h"
#include "next/ui/page_descriptor.h"
#include "next/ui/page_host.h"
#include "next/ui/teacher_profile_page.h"

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFontDatabase>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTranslator>

#include <memory>
#include <optional>
#include <utility>

using namespace ClassMngr;

namespace
{

constexpr QSize Viewport{1024, 768};

class EmptyPreferencesReadPort final
    : public Next::Application::PreferencesStateReadPort
{
public:
    [[nodiscard]] Next::Application::PreferencesStateReadResult read() const override
    {
        return Next::Application::PreferencesStateReadResult::success({});
    }
};

class EmptyPreferencesPersistPort final
    : public Next::Application::PreferencesSnapshotPersistPort
{
public:
    [[nodiscard]] Next::Application::PreferencesPersistResult persist(
        const Next::Application::UserPreferencesSnapshot&)
        const override
    {
        return Next::Application::PreferencesPersistResult::success();
    }
};

[[nodiscard]] QString fileText(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        return {};
    }
    return QString::fromUtf8(file.readAll());
}

[[nodiscard]] Next::Domain::TeacherProfileFields profileFields()
{
    return {
        .teacherKr = u"김선생",
        .teacherEn = u"Dana Kim",
        .preferredRomanization = u"Dana Kim",
        .preferredName = u"Dana Kim",
        .roomNumber = u"204",
        .birthday = u"03-14",
        .phoneNumber = u"010-1234-5678",
        .wifiName = u"Teachers",
        .wifiPassword = u"classroom-wifi",
        .internetType = u"WiFi",
        .zoomId = u"dana.kim",
        .zoomPassword = u"zoom-secret",
        .projectionType = u"HDMI",
        .notes = u"Available after lunch. Bring the HDMI adapter."
    };
}

[[nodiscard]] Teacher legacyTeacher(
    const Next::Domain::TeacherProfileFields& fields
    )
{
    return {
        .teacherKr = QString::fromStdU16String(fields.teacherKr),
        .teacherEn = QString::fromStdU16String(fields.teacherEn),
        .preferredRomanization = QString::fromStdU16String(
            fields.preferredRomanization),
        .preferredName = QString::fromStdU16String(fields.preferredName),
        .roomNumber = QString::fromStdU16String(fields.roomNumber),
        .birthday = QString::fromStdU16String(fields.birthday),
        .phoneNumber = QString::fromStdU16String(fields.phoneNumber),
        .wifiName = QString::fromStdU16String(fields.wifiName),
        .wifiPassword = QString::fromStdU16String(fields.wifiPassword),
        .internetType = QString::fromStdU16String(fields.internetType),
        .zoomId = QString::fromStdU16String(fields.zoomId),
        .zoomPassword = QString::fromStdU16String(fields.zoomPassword),
        .projectionType = QString::fromStdU16String(fields.projectionType),
        .notes = QString::fromStdU16String(fields.notes)
    };
}

[[nodiscard]] QJsonObject profileJson()
{
    return {
        {QStringLiteral("teacherKr"), QStringLiteral("김선생")},
        {QStringLiteral("teacherEn"), QStringLiteral("Dana Kim")},
        {QStringLiteral("preferredRomanization"), QStringLiteral("Dana Kim")},
        {QStringLiteral("preferredName"), QStringLiteral("Dana Kim")},
        {QStringLiteral("roomNumber"), QStringLiteral("204")},
        {QStringLiteral("birthday"), QStringLiteral("03-14")},
        {QStringLiteral("phoneNumber"), QStringLiteral("010-1234-5678")},
        {QStringLiteral("wifiName"), QStringLiteral("Teachers")},
        {QStringLiteral("wifiPassword"), QStringLiteral("classroom-wifi")},
        {QStringLiteral("internetType"), QStringLiteral("WiFi")},
        {QStringLiteral("zoomId"), QStringLiteral("dana.kim")},
        {QStringLiteral("zoomPassword"), QStringLiteral("zoom-secret")},
        {QStringLiteral("projectionType"), QStringLiteral("HDMI")},
        {QStringLiteral("notes"), QStringLiteral(
             "Available after lunch. Bring the HDMI adapter.")}
    };
}

[[nodiscard]] bool captureLegacy(
    const Next::Domain::TeacherProfileFields& fields,
    const QString& path
    )
{
    TeacherInfoPage page(nullptr);
    page.loadTeacher(legacyTeacher(fields), qint64{42});
    page.resize(Viewport);
    page.show();
    QApplication::processEvents();
    if (QWidget* const focused = QApplication::focusWidget())
    {
        focused->clearFocus();
    }
    QApplication::processEvents();
    const QImage image = page.grab().toImage();
    return image.size() == Viewport && image.save(path, "PNG");
}

[[nodiscard]] bool captureNext(
    Next::Resources::ResourceLoader& resourceLoader,
    Next::Application::PreferencesModel& preferences,
    const Next::Domain::TeacherProfileFields& fields,
    const QString& path
    )
{
    Next::Ui::PageHost host(resourceLoader, preferences);
    const Next::Ui::PageId pageId = *Next::Ui::PageId::fromString(
        QStringLiteral("teacher_profile")
        );
    Next::Ui::PageDescriptor descriptor{
        .id = pageId,
        .label = QStringLiteral("Teacher Profile"),
        .factory = [](const Next::Ui::PageContext& context)
            -> Next::Ui::PageOperationResult<std::unique_ptr<Next::Ui::Page>> {
            Next::Ui::TeacherProfilePageActions actions;
            actions.validate = [](const Next::Domain::TeacherProfile& profile) {
                return Next::Application::TeacherProfileValidationResult{
                    .normalizedFields = profile.fields
                };
            };
            return std::unique_ptr<Next::Ui::Page>(
                std::make_unique<Next::Ui::TeacherProfilePage>(
                    context, std::move(actions))
                );
        }
    };
    if (!host.registerPage(std::move(descriptor))
        || !host.activatePage(pageId.value()))
    {
        return false;
    }
    auto* const page = dynamic_cast<Next::Ui::TeacherProfilePage*>(
        host.pageInstance(pageId.value())
        );
    const auto teacherId = Next::Domain::TeacherId::fromString("42");
    if (!page || !teacherId
        || !page->displayProfile({.teacherId = *teacherId, .fields = fields}))
    {
        return false;
    }

    host.resize(Viewport);
    host.show();
    QApplication::processEvents();
    if (QWidget* const focused = QApplication::focusWidget())
    {
        focused->clearFocus();
    }
    QApplication::processEvents();
    const QImage image = page->grab().toImage();
    return image.size() == Viewport && image.save(path, "PNG");
}

} // namespace

int main(int argc, char* argv[])
{
    QApplication application(argc, argv);
    QApplication::setStyle(QStringLiteral("Fusion"));
    if (argc != 2)
    {
        qCritical("Usage: ClassMngrTeacherProfileVisualCapture OUTPUT_DIR");
        return 2;
    }

    const QString outputDirectory = QString::fromLocal8Bit(argv[1]);
    if (!QDir().mkpath(outputDirectory))
    {
        qCritical("Could not create capture output directory");
        return 2;
    }

    const QString resourceRoot = QString::fromUtf8(
        CLASSMNGR_NEXT_TEST_RESOURCE_ROOT);
    const auto catalog = Next::Resources::ResourceCatalog::load(
        QString::fromUtf8(CLASSMNGR_NEXT_TEST_RESOURCE_CATALOG), resourceRoot);
    if (!catalog)
    {
        qCritical().noquote() << "Could not load Next resource catalog:"
                              << catalog.error().message;
        return 1;
    }
    Next::Resources::ResourceLoader resourceLoader(std::move(*catalog));
    EmptyPreferencesReadPort preferencesReadPort;
    EmptyPreferencesPersistPort preferencesPersistPort;
    Next::Application::PreferencesModel preferences(
        preferencesReadPort, preferencesPersistPort);

    for (const QString& font : {
             QStringLiteral("Inter.ttc"),
             QStringLiteral("PretendardVariable.ttf")
         })
    {
        const QString fontPath = QDir(resourceRoot).filePath(
            QStringLiteral("assets/fonts/") + font);
        if (QFile::exists(fontPath))
        {
            static_cast<void>(QFontDatabase::addApplicationFont(fontPath));
        }
    }

    const Next::Domain::TeacherProfileFields fields = profileFields();
    const QStringList themes{
        QStringLiteral("light"), QStringLiteral("dark")
    };
    const QStringList locales{
        QStringLiteral("en_US"), QStringLiteral("ko_KR")
    };
    QJsonArray captures;

    for (int index = 0; index < 2; ++index)
    {
        const QString theme = themes.at(index);
        const QString locale = locales.at(index);
        const QString stylePath = QDir(resourceRoot).filePath(
            QStringLiteral("assets/styles/%1.qss").arg(theme));
        const QString stylesheet = fileText(stylePath);
        if (stylesheet.isEmpty())
        {
            qCritical().noquote() << "Could not load theme:" << stylePath;
            return 1;
        }
        application.setStyleSheet(stylesheet);

        std::unique_ptr<QTranslator> translator;
        if (locale == QStringLiteral("ko_KR"))
        {
            translator = std::make_unique<QTranslator>();
            const QString translationDirectory = QDir(
                QFileInfo(QString::fromUtf8(
                    CLASSMNGR_NEXT_TEST_RESOURCE_CATALOG)).absolutePath()
                ).filePath(QStringLiteral("assets/translations"));
            const QString translationPath = QDir(translationDirectory).filePath(
                QStringLiteral("ClassMngr_ko_KR.qm"));
            if (!translator->load(translationPath)
                || !application.installTranslator(translator.get()))
            {
                qCritical().noquote() << "Could not load Korean translation:"
                                      << translationPath;
                return 1;
            }
        }

        const QString pair = locale + QLatin1Char('-') + theme;
        for (const QString& page : {
                 QStringLiteral("legacy"), QStringLiteral("next")
             })
        {
            const QString path = QDir(outputDirectory).filePath(
                QStringLiteral("%1_%2.png").arg(page, pair));
            const bool saved = page == QStringLiteral("legacy")
                ? captureLegacy(fields, path)
                : captureNext(resourceLoader, preferences, fields, path);
            if (!saved)
            {
                qCritical().noquote() << "Could not capture page:" << path;
                return 1;
            }
            captures.append(QJsonObject{
                {QStringLiteral("page"), page},
                {QStringLiteral("locale"), locale},
                {QStringLiteral("theme"), theme},
                {QStringLiteral("file"), QFileInfo(path).fileName()},
                {QStringLiteral("width"), Viewport.width()},
                {QStringLiteral("height"), Viewport.height()}
            });
        }
        if (translator)
        {
            application.removeTranslator(translator.get());
        }
    }

    const QJsonObject manifest{
        {QStringLiteral("purpose"), QStringLiteral(
             "Matched legacy and Next Teacher Profile page captures")},
        {QStringLiteral("viewport"), QJsonObject{
             {QStringLiteral("width"), Viewport.width()},
             {QStringLiteral("height"), Viewport.height()}}},
        {QStringLiteral("qtPlatform"), QStringLiteral("offscreen")},
        {QStringLiteral("style"), QStringLiteral("Fusion")},
        {QStringLiteral("profile"), profileJson()},
        {QStringLiteral("captures"), captures}
    };
    QFile manifestFile(QDir(outputDirectory).filePath(
        QStringLiteral("manifest.json")));
    if (!manifestFile.open(QIODevice::WriteOnly | QIODevice::Truncate)
        || manifestFile.write(QJsonDocument(manifest).toJson(
               QJsonDocument::Indented)) < 0)
    {
        qCritical("Could not write capture manifest");
        return 1;
    }
    qInfo().noquote() << "Captured matched profile pages in" << outputDirectory;
    return 0;
}
