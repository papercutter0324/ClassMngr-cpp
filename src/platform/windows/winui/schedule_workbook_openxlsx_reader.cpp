#include "schedule_workbook_openxlsx_reader.h"

#include "classmngr/engine/schedule_workbook_interpreter.h"
#include "classmngr/engine/schedule_workbook_layout.h"

#include "OpenXLSX/headers/XLDocument.hpp"
#include "OpenXLSX/headers/XLSheet.hpp"
#include "OpenXLSX/headers/XLZipArchive.hpp"
#include <pugixml.hpp>

#include <algorithm>
#include <array>
#include <charconv>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace classmngr::winui
{
namespace
{
using classmngr::engine::Error;
using classmngr::engine::ErrorCode;
using classmngr::engine::Result;
using classmngr::engine::ScheduleImportKind;
using classmngr::engine::ScheduleImportWorkbook;
using classmngr::engine::ScheduleWorkbookLayout;
using classmngr::engine::ScheduleWorkbookLayoutCell;
using classmngr::engine::ScheduleWorkbookLayoutRange;
using classmngr::engine::ScheduleWorkbookLayoutSheet;
using classmngr::engine::ScheduleWorkbookLayoutStyle;

class ReaderFormatFailure final {};
class ReaderCancellation final {};
class ReaderLimitFailure final {};

struct RawSheetReference
{
    std::string name;
    std::string archivePath;
    bool visible = true;
};

using RawNotes = std::unordered_map<std::string, std::string>;

constexpr std::array<std::string_view, 12> ThemeNames{
    "lt1", "dk1", "lt2", "dk2", "accent1", "accent2",
    "accent3", "accent4", "accent5", "accent6", "hlink", "folHlink"
};

// These limits are intentionally well above the dimensions of supported
// schedule templates while bounding ZIP/XML expansion and native model size.
constexpr std::uintmax_t MaxWorkbookFileBytes = 64u * 1024u * 1024u;
constexpr std::size_t MaxWorkbookXmlBytes = 1u * 1024u * 1024u;
constexpr std::size_t MaxRelationshipsXmlBytes = 2u * 1024u * 1024u;
constexpr std::size_t MaxStylesXmlBytes = 4u * 1024u * 1024u;
constexpr std::size_t MaxWorksheetXmlBytes = 16u * 1024u * 1024u;
constexpr std::size_t MaxNotesXmlBytes = 4u * 1024u * 1024u;
constexpr std::size_t MaxSheetCount = 32;
constexpr std::uint32_t MaxRowsPerSheet = 10'000;
constexpr std::uint16_t MaxColumnsPerRow = 512;
constexpr std::size_t MaxCellsPerSheet = 100'000;
constexpr std::size_t MaxTotalCells = 250'000;
constexpr std::size_t MaxMergedRangesPerSheet = 4'096;
constexpr std::size_t MaxStyles = 4'096;
constexpr std::size_t MaxNotes = 10'000;
constexpr std::size_t MaxCellTextBytes = 1u * 1024u * 1024u;

void requireCondition(bool condition)
{
    if (!condition)
    {
        throw ReaderFormatFailure{};
    }
}

void requireWithinLimit(std::size_t value, std::size_t limit)
{
    if (value > limit)
    {
        throw ReaderLimitFailure{};
    }
}

std::string readArchiveEntry(
    const OpenXLSX::XLZipArchive& archive,
    const std::string& name,
    std::size_t limit
    )
{
    if (!archive.hasEntry(name))
    {
        return {};
    }
    std::string data = archive.getEntry(name);
    requireWithinLimit(data.size(), limit);
    return data;
}

void checkCancellation(
    const classmngr::engine::ScheduleWorkbookCancellation& isCancelled
    )
{
    if (isCancelled && isCancelled())
    {
        throw ReaderCancellation{};
    }
}

Error makeError(ErrorCode code, const char* message)
{
    return Error{code, message, std::nullopt};
}

std::string pathToUtf8(const std::filesystem::path& path)
{
    const auto value = path.u8string();
    return std::string(
        reinterpret_cast<const char*>(value.data()),
        value.size()
        );
}

std::string trimAsciiWhitespace(std::string_view value)
{
    std::size_t first = 0;
    while (first < value.size()
           && std::isspace(static_cast<unsigned char>(value[first])) != 0)
    {
        ++first;
    }

    std::size_t last = value.size();
    while (last > first
           && std::isspace(static_cast<unsigned char>(value[last - 1])) != 0)
    {
        --last;
    }
    return std::string(value.substr(first, last - first));
}

std::string simplifyAsciiWhitespace(std::string_view value)
{
    std::string result;
    result.reserve(value.size());
    bool pendingSpace = false;
    for (const char character : value)
    {
        if (std::isspace(static_cast<unsigned char>(character)) != 0)
        {
            pendingSpace = !result.empty();
            continue;
        }
        if (pendingSpace)
        {
            result.push_back(' ');
            pendingSpace = false;
        }
        result.push_back(character);
    }
    return result;
}

std::string upperAscii(std::string value)
{
    for (char& character : value)
    {
        if (character >= 'a' && character <= 'z')
        {
            character = static_cast<char>(character - 'a' + 'A');
        }
    }
    return value;
}

std::string lowerAscii(std::string value)
{
    for (char& character : value)
    {
        if (character >= 'A' && character <= 'Z')
        {
            character = static_cast<char>(character - 'A' + 'a');
        }
    }
    return value;
}

bool endsWith(std::string_view value, std::string_view suffix)
{
    return value.size() >= suffix.size()
        && value.compare(value.size() - suffix.size(), suffix.size(), suffix) == 0;
}

std::string localName(const pugi::xml_node& node)
{
    const std::string_view name = node.name();
    const std::size_t separator = name.rfind(':');
    return std::string(
        separator == std::string_view::npos
            ? name
            : name.substr(separator + 1)
        );
}

pugi::xml_node childNamed(
    const pugi::xml_node& parent,
    std::string_view name
    )
{
    for (const pugi::xml_node child : parent.children())
    {
        if (localName(child) == name)
        {
            return child;
        }
    }
    return {};
}

std::string attributeValue(
    const pugi::xml_node& node,
    const char* name
    )
{
    const pugi::xml_attribute attribute = node.attribute(name);
    return attribute.empty() ? std::string{} : std::string(attribute.value());
}

std::optional<int> integerAttribute(
    const pugi::xml_node& node,
    const char* name
    )
{
    const std::string value = attributeValue(node, name);
    if (value.empty())
    {
        return std::nullopt;
    }

    int result = 0;
    const auto parsed = std::from_chars(
        value.data(),
        value.data() + value.size(),
        result
        );
    if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size())
    {
        return std::nullopt;
    }
    return result;
}

std::optional<double> doubleAttribute(
    const pugi::xml_node& node,
    const char* name
    )
{
    const std::string value = attributeValue(node, name);
    if (value.empty())
    {
        return std::nullopt;
    }

    char* end = nullptr;
    const double result = std::strtod(value.c_str(), &end);
    if (end == value.c_str() || *end != '\0' || !std::isfinite(result))
    {
        return std::nullopt;
    }
    return result;
}

std::string normalizedColor(std::string color)
{
    color = upperAscii(trimAsciiWhitespace(color));
    if (!color.empty() && color.front() == '#')
    {
        color.erase(color.begin());
    }
    if (color.size() == 8 && color.starts_with("FF"))
    {
        color.erase(0, 2);
    }
    return color;
}

int hexByte(std::string_view value, bool* ok)
{
    unsigned int result = 0;
    const auto parsed = std::from_chars(
        value.data(),
        value.data() + value.size(),
        result,
        16
        );
    if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size()
        || result > 255)
    {
        if (ok)
        {
            *ok = false;
        }
        return 0;
    }
    if (ok)
    {
        *ok = true;
    }
    return static_cast<int>(result);
}

std::string rgbHex(int red, int green, int blue)
{
    constexpr char digits[] = "0123456789ABCDEF";
    std::string result(6, '0');
    const std::array<int, 3> values{red, green, blue};
    for (std::size_t index = 0; index < values.size(); ++index)
    {
        const int value = std::clamp(values[index], 0, 255);
        result[index * 2] = digits[value >> 4];
        result[index * 2 + 1] = digits[value & 0x0f];
    }
    return result;
}

double hueComponent(double first, double second, double hue)
{
    if (hue < 0.0)
    {
        hue += 1.0;
    }
    if (hue > 1.0)
    {
        hue -= 1.0;
    }
    if (hue < 1.0 / 6.0)
    {
        return first + (second - first) * 6.0 * hue;
    }
    if (hue < 0.5)
    {
        return second;
    }
    if (hue < 2.0 / 3.0)
    {
        return first + (second - first) * (2.0 / 3.0 - hue) * 6.0;
    }
    return first;
}

std::string tintedColor(std::string color, double tint)
{
    color = normalizedColor(std::move(color));
    if (color.size() != 6)
    {
        return color;
    }

    bool redOk = false;
    bool greenOk = false;
    bool blueOk = false;
    const double red = hexByte(color.substr(0, 2), &redOk) / 255.0;
    const double green = hexByte(color.substr(2, 2), &greenOk) / 255.0;
    const double blue = hexByte(color.substr(4, 2), &blueOk) / 255.0;
    if (!redOk || !greenOk || !blueOk)
    {
        return {};
    }

    const double maximum = std::max({red, green, blue});
    const double minimum = std::min({red, green, blue});
    double hue = 0.0;
    double saturation = 0.0;
    double luminance = (maximum + minimum) / 2.0;

    if (maximum != minimum)
    {
        const double difference = maximum - minimum;
        saturation = luminance > 0.5
            ? difference / (2.0 - maximum - minimum)
            : difference / (maximum + minimum);
        if (maximum == red)
        {
            hue = (green - blue) / difference
                + (green < blue ? 6.0 : 0.0);
        }
        else if (maximum == green)
        {
            hue = (blue - red) / difference + 2.0;
        }
        else
        {
            hue = (red - green) / difference + 4.0;
        }
        hue /= 6.0;
    }

    tint = std::clamp(tint, -1.0, 1.0);
    luminance = tint < 0.0
        ? luminance * (1.0 + tint)
        : luminance * (1.0 - tint) + tint;

    double tintedRed = luminance;
    double tintedGreen = luminance;
    double tintedBlue = luminance;
    if (saturation > 0.0)
    {
        const double second = luminance < 0.5
            ? luminance * (1.0 + saturation)
            : luminance + saturation - luminance * saturation;
        const double first = 2.0 * luminance - second;
        tintedRed = hueComponent(first, second, hue + 1.0 / 3.0);
        tintedGreen = hueComponent(first, second, hue);
        tintedBlue = hueComponent(first, second, hue - 1.0 / 3.0);
    }

    return rgbHex(
        static_cast<int>(std::lround(tintedRed * 255.0)),
        static_cast<int>(std::lround(tintedGreen * 255.0)),
        static_cast<int>(std::lround(tintedBlue * 255.0))
        );
}

std::vector<std::string> parseThemeColors(const std::string& xmlData)
{
    std::vector<std::string> colors(ThemeNames.size());
    colors[0] = "FFFFFF";
    colors[1] = "000000";

    if (xmlData.empty())
    {
        return colors;
    }

    pugi::xml_document document;
    requireCondition(document.load_buffer(xmlData.data(), xmlData.size()));
    const pugi::xml_node scheme = childNamed(
        document.document_element(),
        "clrScheme"
        );
    for (const pugi::xml_node colorSlot : scheme.children())
    {
        const std::string slotName = localName(colorSlot);
        const auto slot = std::find(
            ThemeNames.begin(),
            ThemeNames.end(),
            slotName
            );
        if (slot == ThemeNames.end())
        {
            continue;
        }
        const std::size_t index = static_cast<std::size_t>(
            std::distance(ThemeNames.begin(), slot)
            );
        const pugi::xml_node srgb = childNamed(colorSlot, "srgbClr");
        const pugi::xml_node system = childNamed(colorSlot, "sysClr");
        if (!srgb.empty())
        {
            colors[index] = normalizedColor(attributeValue(srgb, "val"));
        }
        else if (!system.empty())
        {
            colors[index] = normalizedColor(attributeValue(system, "lastClr"));
        }
    }
    return colors;
}

std::vector<std::string> parseIndexedColors(const pugi::xml_node& styleSheet)
{
    std::vector<std::string> colors;
    const pugi::xml_node indexedColors = childNamed(styleSheet, "colors");
    const pugi::xml_node indexed = childNamed(indexedColors, "indexedColors");
    for (const pugi::xml_node color : indexed.children())
    {
        if (localName(color) == "rgbColor")
        {
            requireWithinLimit(colors.size() + 1u, MaxStyles);
            colors.push_back(normalizedColor(attributeValue(color, "rgb")));
        }
    }
    return colors;
}

std::string resolvedStyleColor(
    const pugi::xml_node& colorNode,
    const std::vector<std::string>& themeColors,
    const std::vector<std::string>& indexedColors
    )
{
    std::string color = normalizedColor(attributeValue(colorNode, "rgb"));
    const auto theme = integerAttribute(colorNode, "theme");
    if (color.empty() && theme && *theme >= 0
        && static_cast<std::size_t>(*theme) < themeColors.size())
    {
        color = themeColors[static_cast<std::size_t>(*theme)];
    }

    const auto indexed = integerAttribute(colorNode, "indexed");
    if (color.empty() && indexed && *indexed >= 0
        && static_cast<std::size_t>(*indexed) < indexedColors.size())
    {
        color = indexedColors[static_cast<std::size_t>(*indexed)];
    }

    const auto tint = doubleAttribute(colorNode, "tint");
    return tint ? tintedColor(std::move(color), *tint) : normalizedColor(color);
}

std::vector<ScheduleWorkbookLayoutStyle> parseStyles(
    const std::string& xmlData,
    const std::string& themeData
    )
{
    if (xmlData.empty())
    {
        return {ScheduleWorkbookLayoutStyle{}};
    }

    pugi::xml_document document;
    requireCondition(document.load_buffer(xmlData.data(), xmlData.size()));
    const pugi::xml_node styleSheet = document.document_element();
    requireCondition(
        !styleSheet.empty()
        && localName(styleSheet) == "styleSheet"
        );

    const std::vector<std::string> themeColors = parseThemeColors(themeData);
    const std::vector<std::string> indexedColors = parseIndexedColors(styleSheet);

    std::vector<std::string> fillColors;
    std::vector<bool> fillFlags;
    const pugi::xml_node fills = childNamed(styleSheet, "fills");
    for (const pugi::xml_node fill : fills.children())
    {
        if (localName(fill) != "fill")
        {
            continue;
        }
        requireWithinLimit(fillColors.size() + 1u, MaxStyles);
        const pugi::xml_node patternFill = childNamed(fill, "patternFill");
        const std::string pattern = attributeValue(patternFill, "patternType");
        const bool filled = !pattern.empty()
            && pattern != "none"
            && pattern != "gray125";
        const pugi::xml_node foreground = childNamed(patternFill, "fgColor");
        const std::string color = resolvedStyleColor(
            foreground,
            themeColors,
            indexedColors
            );
        fillColors.push_back(color);
        fillFlags.push_back(filled && normalizedColor(color) != "FFFFFF");
    }

    std::vector<std::string> fontColors;
    std::vector<bool> fontBoldFlags;
    const pugi::xml_node fonts = childNamed(styleSheet, "fonts");
    for (const pugi::xml_node font : fonts.children())
    {
        if (localName(font) != "font")
        {
            continue;
        }
        requireWithinLimit(fontColors.size() + 1u, MaxStyles);
        const pugi::xml_node bold = childNamed(font, "b");
        const std::string boldValue = attributeValue(bold, "val");
        const bool isBold = !bold.empty()
            && (boldValue.empty()
                || (boldValue != "0"
                    && lowerAscii(boldValue) != "false"));
        const pugi::xml_node colorNode = childNamed(font, "color");
        fontColors.push_back(resolvedStyleColor(
            colorNode,
            themeColors,
            indexedColors
            ));
        fontBoldFlags.push_back(isBold);
    }

    std::vector<ScheduleWorkbookLayoutStyle> styles;
    const pugi::xml_node cellXfs = childNamed(styleSheet, "cellXfs");
    for (const pugi::xml_node xf : cellXfs.children())
    {
        if (localName(xf) != "xf")
        {
            continue;
        }
        requireWithinLimit(styles.size() + 1u, MaxStyles);
        ScheduleWorkbookLayoutStyle style;
        const auto fill = integerAttribute(xf, "fillId");
        if (fill && *fill >= 0 && static_cast<std::size_t>(*fill) < fillColors.size())
        {
            style.fillColor = fillColors[static_cast<std::size_t>(*fill)];
            style.filled = fillFlags[static_cast<std::size_t>(*fill)];
        }
        const auto font = integerAttribute(xf, "fontId");
        if (font && *font >= 0 && static_cast<std::size_t>(*font) < fontColors.size())
        {
            style.fontColor = fontColors[static_cast<std::size_t>(*font)];
            style.bold = fontBoldFlags[static_cast<std::size_t>(*font)];
        }
        styles.push_back(std::move(style));
    }

    if (styles.empty())
    {
        styles.push_back(ScheduleWorkbookLayoutStyle{});
    }
    return styles;
}

std::string normalizeArchivePath(
    std::string baseDirectory,
    std::string target
    )
{
    target = trimAsciiWhitespace(target);
    if (!target.empty() && target.front() == '/')
    {
        baseDirectory.clear();
        target.erase(target.begin());
    }

    std::string combined = std::move(baseDirectory) + target;
    std::vector<std::string> components;
    std::size_t start = 0;
    while (start <= combined.size())
    {
        const std::size_t separator = combined.find('/', start);
        const std::size_t end = separator == std::string::npos
            ? combined.size()
            : separator;
        const std::string component = combined.substr(start, end - start);
        if (component.empty() || component == ".")
        {
            // Nothing to append.
        }
        else if (component == "..")
        {
            if (!components.empty())
            {
                components.pop_back();
            }
        }
        else
        {
            components.push_back(component);
        }
        if (separator == std::string::npos)
        {
            break;
        }
        start = separator + 1;
    }

    std::string result;
    for (const std::string& component : components)
    {
        if (!result.empty())
        {
            result.push_back('/');
        }
        result += component;
    }
    return result;
}

std::string directoryName(std::string_view archivePath)
{
    const std::size_t separator = archivePath.rfind('/');
    return separator == std::string_view::npos
        ? std::string{}
        : std::string(archivePath.substr(0, separator + 1));
}

std::string fileName(std::string_view archivePath)
{
    const std::size_t separator = archivePath.rfind('/');
    return separator == std::string_view::npos
        ? std::string(archivePath)
        : std::string(archivePath.substr(separator + 1));
}

struct RawRelationship
{
    std::string type;
    std::string target;
};

std::unordered_map<std::string, RawRelationship> parseRelationships(
    const std::string& xmlData
    )
{
    pugi::xml_document document;
    requireCondition(document.load_buffer(xmlData.data(), xmlData.size()));
    std::unordered_map<std::string, RawRelationship> result;
    for (const pugi::xml_node relationship : document.document_element().children())
    {
        if (localName(relationship) != "Relationship")
        {
            continue;
        }
        const std::string id = attributeValue(relationship, "Id");
        if (!id.empty())
        {
            result.insert_or_assign(
                id,
                RawRelationship{
                    attributeValue(relationship, "Type"),
                    attributeValue(relationship, "Target")
                }
                );
        }
    }
    return result;
}

std::vector<RawSheetReference> parseSheetReferences(
    const std::string& workbookData,
    const std::string& workbookRelationshipsData,
    const OpenXLSX::XLZipArchive& archive
    )
{
    pugi::xml_document workbookDocument;
    requireCondition(workbookDocument.load_buffer(
        workbookData.data(),
        workbookData.size()
        ));
    const auto relationships = parseRelationships(workbookRelationshipsData);
    const pugi::xml_node sheets = childNamed(
        workbookDocument.document_element(),
        "sheets"
        );

    std::vector<RawSheetReference> result;
    for (const pugi::xml_node sheet : sheets.children())
    {
        if (localName(sheet) != "sheet")
        {
            continue;
        }
        const std::string relationshipId = attributeValue(sheet, "r:id");
        const auto relationship = relationships.find(relationshipId);
        if (relationship == relationships.end())
        {
            // Chartsheets and other non-worksheet parts are not candidates.
            continue;
        }
        if (!endsWith(relationship->second.type, "/worksheet"))
        {
            continue;
        }

        requireWithinLimit(result.size() + 1u, MaxSheetCount);

        const std::string path = normalizeArchivePath(
            "xl/",
            relationship->second.target
            );
        requireCondition(!path.empty() && archive.hasEntry(path));
        const std::string state = lowerAscii(attributeValue(sheet, "state"));
        result.push_back({
            attributeValue(sheet, "name"),
            path,
            state != "hidden" && state != "veryhidden"
        });
    }
    return result;
}

void appendXmlText(const pugi::xml_node& node, std::string* result)
{
    for (const pugi::xml_node child : node.children())
    {
        if (child.type() == pugi::node_pcdata
            || child.type() == pugi::node_cdata)
        {
            result->append(child.value());
        }
        else
        {
            appendXmlText(child, result);
        }
    }
}

void appendNotesFromDocument(
    const std::string& xmlData,
    RawNotes* notes
    )
{
    pugi::xml_document document;
    requireCondition(document.load_buffer(xmlData.data(), xmlData.size()));
    std::vector<pugi::xml_node> pendingNodes;
    pendingNodes.push_back(document.document_element());
    while (!pendingNodes.empty())
    {
        const pugi::xml_node node = pendingNodes.back();
        pendingNodes.pop_back();
        const std::string name = localName(node);
        if (name == "comment" || name == "threadedComment")
        {
            const std::string reference = upperAscii(
                trimAsciiWhitespace(attributeValue(node, "ref"))
                );
            if (!reference.empty())
            {
                std::string text;
                appendXmlText(node, &text);
                text = simplifyAsciiWhitespace(text);
                if (!text.empty())
                {
                    requireWithinLimit(notes->size() + 1u, MaxNotes);
                    notes->insert_or_assign(reference, std::move(text));
                }
            }
        }
        for (const pugi::xml_node child : node.children())
        {
            pendingNodes.push_back(child);
        }
    }
}

RawNotes parseSheetNotes(
    const OpenXLSX::XLZipArchive& archive,
    std::string_view worksheetPath
    )
{
    RawNotes result;
    const std::string relationshipsPath = directoryName(worksheetPath)
        + "_rels/" + fileName(worksheetPath) + ".rels";
    if (!archive.hasEntry(relationshipsPath))
    {
        return result;
    }

    const auto relationships = parseRelationships(readArchiveEntry(
        archive,
        relationshipsPath,
        MaxRelationshipsXmlBytes
        ));
    for (const auto& [unusedId, relationship] : relationships)
    {
        (void)unusedId;
        if (!endsWith(relationship.type, "/comments")
            && !endsWith(relationship.type, "/threadedComment"))
        {
            continue;
        }
        const std::string notePath = normalizeArchivePath(
            directoryName(worksheetPath),
            relationship.target
        );
        if (archive.hasEntry(notePath))
        {
            appendNotesFromDocument(
                readArchiveEntry(archive, notePath, MaxNotesXmlBytes),
                &result
                );
        }
    }
    return result;
}

void appendMergedRanges(
    OpenXLSX::XLWorksheet& worksheet,
    ScheduleWorkbookLayoutSheet* result
    )
{
    auto& merges = worksheet.merges();
    requireWithinLimit(
        static_cast<std::size_t>(merges.count()),
        MaxMergedRangesPerSheet
        );
    for (OpenXLSX::XLMergeIndex index = 0; index < merges.count(); ++index)
    {
        const OpenXLSX::XLCellRange range = merges.mergeAsRange(index);
        const OpenXLSX::XLCellReference first = range.topLeft();
        const OpenXLSX::XLCellReference last = range.bottomRight();
        requireCondition(
            first.row() > 0
            && first.column() > 0
            && last.row() >= first.row()
            && last.column() >= first.column()
            );
        result->mergedRanges.push_back({
            static_cast<int>(first.row()),
            static_cast<int>(first.column()),
            static_cast<int>(last.row()),
            static_cast<int>(last.column())
        });
    }
}

void appendWorksheetCells(
    OpenXLSX::XLWorksheet& worksheet,
    const RawNotes& notes,
    const classmngr::engine::ScheduleWorkbookCancellation& isCancelled,
    std::size_t* totalCellCount,
    ScheduleWorkbookLayoutSheet* result
    )
{
    auto rows = worksheet.rows();
    for (auto rowIterator = rows.begin(); rowIterator != rows.end(); ++rowIterator)
    {
        checkCancellation(isCancelled);
        requireCondition(rowIterator.rowNumber() > 0);
        requireWithinLimit(rowIterator.rowNumber(), MaxRowsPerSheet);
        if (!rowIterator.rowExists())
        {
            continue;
        }

        auto& row = *rowIterator;
        const uint16_t cellCount = row.cellCount();
        requireWithinLimit(cellCount, MaxColumnsPerRow);
        for (uint16_t column = 1; column <= cellCount; ++column)
        {
            checkCancellation(isCancelled);
            OpenXLSX::XLCell cell = row.findCell(column);
            if (!cell)
            {
                continue;
            }

            const OpenXLSX::XLCellReference reference = cell.cellReference();
            requireCondition(
                reference.row() <= static_cast<uint32_t>(std::numeric_limits<int>::max())
                && reference.column() <= static_cast<uint16_t>(std::numeric_limits<int>::max())
                );
            requireWithinLimit(result->cells.size() + 1u, MaxCellsPerSheet);
            requireWithinLimit(*totalCellCount + 1u, MaxTotalCells);
            const std::string address = upperAscii(reference.address());
            const std::size_t style = cell.cellFormat();
            requireCondition(style <= static_cast<std::size_t>(
                std::numeric_limits<int>::max()
                ));

            ScheduleWorkbookLayoutCell mapped;
            mapped.row = static_cast<int>(reference.row());
            mapped.column = static_cast<int>(reference.column());
            mapped.style = static_cast<int>(style);
            mapped.value = cell.getString();
            requireWithinLimit(mapped.value.size(), MaxCellTextBytes);
            const auto note = notes.find(address);
            if (note != notes.end())
            {
                mapped.note = note->second;
                requireWithinLimit(mapped.note.size(), MaxCellTextBytes);
            }
            result->cells.push_back(std::move(mapped));
            ++*totalCellCount;
        }
    }
}

ScheduleWorkbookLayout mapWorkbook(
    OpenXLSX::XLDocument& document,
    const OpenXLSX::XLZipArchive& archive,
    const std::vector<RawSheetReference>& sheetReferences,
    const classmngr::engine::ScheduleWorkbookCancellation& isCancelled
    )
{
    ScheduleWorkbookLayout result;
    const std::string stylesData = readArchiveEntry(
        archive,
        "xl/styles.xml",
        MaxStylesXmlBytes
        );
    const std::string themeData = readArchiveEntry(
        archive,
        "xl/theme/theme1.xml",
        MaxStylesXmlBytes
        );
    result.styles = parseStyles(stylesData, themeData);
    result.sheets.reserve(sheetReferences.size());

    auto workbook = document.workbook();
    std::size_t totalCellCount = 0;
    for (const RawSheetReference& reference : sheetReferences)
    {
        checkCancellation(isCancelled);
        const std::string worksheetData = readArchiveEntry(
            archive,
            reference.archivePath,
            MaxWorksheetXmlBytes
            );
        requireCondition(!worksheetData.empty());
        ScheduleWorkbookLayoutSheet sheet;
        sheet.name = reference.name;
        sheet.visible = reference.visible;
        const RawNotes notes = parseSheetNotes(archive, reference.archivePath);
        auto worksheet = workbook.worksheet(reference.name);
        appendWorksheetCells(
            worksheet,
            notes,
            isCancelled,
            &totalCellCount,
            &sheet
            );
        appendMergedRanges(worksheet, &sheet);
        result.sheets.push_back(std::move(sheet));
    }
    return result;
}

Result<ScheduleWorkbookLayout> readLayout(
    const std::filesystem::path& file,
    const classmngr::engine::ScheduleWorkbookCancellation& isCancelled
    )
{
    checkCancellation(isCancelled);
    const std::string utf8Path = pathToUtf8(file);

    OpenXLSX::XLZipArchive archive;
    archive.open(utf8Path);
    requireCondition(archive.isOpen());
    const std::string workbookData = readArchiveEntry(
        archive,
        "xl/workbook.xml",
        MaxWorkbookXmlBytes
        );
    const std::string workbookRelationshipsData = readArchiveEntry(
        archive,
        "xl/_rels/workbook.xml.rels",
        MaxRelationshipsXmlBytes
        );
    requireCondition(
        !workbookData.empty()
        && !workbookRelationshipsData.empty()
        );

    const std::vector<RawSheetReference> sheetReferences = parseSheetReferences(
        workbookData,
        workbookRelationshipsData,
        archive
        );

    checkCancellation(isCancelled);
    OpenXLSX::XLDocument document(utf8Path);
    requireCondition(document.isOpen());
    return mapWorkbook(document, archive, sheetReferences, isCancelled);
}

} // namespace

Result<ScheduleImportWorkbook> ScheduleWorkbookOpenXLSXReader::read(
    const std::filesystem::path& file,
    ScheduleImportKind kind,
    const classmngr::engine::ScheduleWorkbookCancellation& isCancelled
    ) const
{
    if (file.empty())
    {
        return std::unexpected(makeError(
            ErrorCode::InvalidArgument,
            "Choose an XLSX schedule file."
            ));
    }

    const std::string extension = lowerAscii(pathToUtf8(file.extension()));
    if (extension != ".xlsx")
    {
        return std::unexpected(makeError(
            ErrorCode::Unsupported,
            "The selected file is not an XLSX workbook."
            ));
    }

    std::error_code filesystemError;
    if (!std::filesystem::is_regular_file(file, filesystemError))
    {
        return std::unexpected(makeError(
            filesystemError
                ? ErrorCode::Io
                : ErrorCode::NotFound,
            filesystemError
                ? "The selected schedule file could not be accessed."
                : "The selected schedule file was not found."
            ));
    }

    filesystemError.clear();
    const std::uintmax_t fileSize = std::filesystem::file_size(
        file,
        filesystemError
        );
    if (filesystemError)
    {
        return std::unexpected(makeError(
            ErrorCode::Io,
            "The selected schedule file could not be accessed."
            ));
    }
    if (fileSize > MaxWorkbookFileBytes)
    {
        return std::unexpected(makeError(
            ErrorCode::InvalidFormat,
            "The selected workbook exceeds the supported import size limit."
            ));
    }

    try
    {
        checkCancellation(isCancelled);
        const Result<ScheduleWorkbookLayout> layout = readLayout(file, isCancelled);
        if (!layout)
        {
            return std::unexpected(layout.error());
        }
        checkCancellation(isCancelled);
        return classmngr::engine::ScheduleWorkbookInterpreter::interpret(
            *layout,
            kind,
            isCancelled
            );
    }
    catch (const ReaderCancellation&)
    {
        return std::unexpected(makeError(
            ErrorCode::Cancelled,
            "The schedule import was cancelled."
            ));
    }
    catch (const ReaderLimitFailure&)
    {
        return std::unexpected(makeError(
            ErrorCode::InvalidFormat,
            "The workbook exceeds the supported schedule import limits."
            ));
    }
    catch (const ReaderFormatFailure&)
    {
        return std::unexpected(makeError(
            ErrorCode::InvalidFormat,
            "The selected workbook could not be read."
            ));
    }
    catch (const std::filesystem::filesystem_error&)
    {
        return std::unexpected(makeError(
            ErrorCode::Io,
            "The selected schedule file could not be accessed."
            ));
    }
    catch (const std::exception&)
    {
        return std::unexpected(makeError(
            ErrorCode::InvalidFormat,
            "The selected workbook could not be read."
            ));
    }
    catch (...)
    {
        return std::unexpected(makeError(
            ErrorCode::Internal,
            "The schedule workbook reader failed."
            ));
    }
}

std::unique_ptr<classmngr::engine::ScheduleWorkbookReader>
makeScheduleWorkbookReader()
{
    return std::make_unique<ScheduleWorkbookOpenXLSXReader>();
}

} // namespace classmngr::winui
