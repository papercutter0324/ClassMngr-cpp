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

}  // namespace ClassMngr::Next::Platform
