#include "next/platform/qt_font_family_availability_adapter.h"

#include <QFontDatabase>
#include <QStringList>

namespace ClassMngr::Next::Platform
{

std::vector<std::u16string>
QtFontFamilyAvailabilityAdapter::availableFontFamilies() const
{
    const QStringList families = QFontDatabase::families();
    std::vector<std::u16string> result;
    result.reserve(static_cast<std::size_t>(families.size()));
    for (const QString& family : families)
    {
        result.push_back(family.toStdU16String());
    }
    return result;
}

std::u16string
QtFontFamilyAvailabilityAdapter::outputFallbackFontFamily() const
{
#ifdef Q_OS_MACOS
    return u"Helvetica Neue";
#elif defined(Q_OS_WIN)
    return u"Segoe UI";
#else
    return QFontDatabase::systemFont(QFontDatabase::GeneralFont)
        .family().toStdU16String();
#endif
}

int QtFontFamilyAvailabilityAdapter::outputFallbackEnglishPointSize() const
{
#ifdef Q_OS_MACOS
    return 15;
#else
    return 14;
#endif
}

int QtFontFamilyAvailabilityAdapter::applicationBaseFontPointSize() const
{
#ifdef Q_OS_MACOS
    return 15;
#else
    return 14;
#endif
}

int QtFontFamilyAvailabilityAdapter::teacherProfileKoreanFontPointSize() const
{
#ifdef Q_OS_MACOS
    return 16;
#else
    return 15;
#endif
}

}  // namespace ClassMngr::Next::Platform
