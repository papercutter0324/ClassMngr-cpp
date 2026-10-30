#include "core/fontmanager.h"
#include <QGuiApplication>
#include <QFont>
#include <QFontInfo>
#include <QPdfWriter>
#include <QPainter>
#include <QRawFont>
#include <QTextStream>

static QFont makeFont(float weightAxis, bool bold)
{
    QFont font = FontManager::getKoreanFont(-1);
    const auto tag = QFont::Tag::fromString(QStringLiteral("wght"));
    if (!tag)
        qFatal("Qt rejected the wght axis tag");
    font.setStyleName(bold ? QString() : QStringLiteral("Regular"));
    font.setWeight(bold ? QFont::Bold : QFont::Normal);
    font.setVariableAxis(*tag, weightAxis);
    return font;
}

static void draw(const QString& output, bool boldFirst)
{
    QPdfWriter writer(output);
    writer.setResolution(300);
    QPainter painter(&writer);
    const QFont regular = makeFont(400.0F, false);
    const QFont bold = makeFont(700.0F, true);
    QTextStream(stdout) << output << " regular=" << regular.family() << '/'
        << QFontInfo(regular).styleName() << '/' << QFontInfo(regular).weight()
        << " wght=" << regular.variableAxisValue(*QFont::Tag::fromString(QStringLiteral("wght")))
        << " raw=" << QRawFont::fromFont(regular).styleName()
        << " bold=" << bold.family() << '/' << QFontInfo(bold).styleName() << '/'
        << QFontInfo(bold).weight() << " wght="
        << bold.variableAxisValue(*QFont::Tag::fromString(QStringLiteral("wght")))
        << " raw=" << QRawFont::fromFont(bold).styleName() << '\n';
    const auto drawFont = [&painter](const QFont& font, int y, const QString& label)
    {
        painter.setFont(font);
        painter.drawText(72, y, label);
    };
    if (boldFirst)
    {
        drawFont(bold, 100, QStringLiteral("Bold heading"));
        drawFont(regular, 200, QStringLiteral("Regular Student 1-1"));
    }
    else
    {
        drawFont(regular, 100, QStringLiteral("Regular Student 1-1"));
        drawFont(bold, 200, QStringLiteral("Bold heading"));
    }
    painter.end();
}

int main(int argc, char** argv)
{
    QGuiApplication app(argc, argv);
    FontManager::loadFonts();
    draw(QString::fromLocal8Bit(argv[1]), true);
    draw(QString::fromLocal8Bit(argv[2]), false);
}
