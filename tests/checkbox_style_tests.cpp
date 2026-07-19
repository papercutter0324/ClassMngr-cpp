#include <QApplication>
#include <QAbstractButton>
#include <QCheckBox>
#include <QColor>
#include <QFile>
#include <QImage>
#include <QListWidget>
#include <QRadioButton>
#include <QStyle>
#include <QStyleOptionButton>
#include <QStyleOptionViewItem>
#include <QTest>

#include <algorithm>

namespace
{
QString loadStylesheet(
    const QString& path
    )
{
    QFile file(path);

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        return {};
    }

    return QString::fromUtf8(file.readAll());
}

struct IndicatorRendering
{
    QSize size;
    int brightestInteriorPixel = 0;
    int darkestInteriorPixel = 0;
    int medianInteriorPixel = 0;
};

IndicatorRendering analyzeIndicator(
    const QImage& rendering,
    const QRect& indicatorRect
    )
{
    const QRect interior =
        indicatorRect.adjusted(3, 3, -3, -3);
    QList<int> lightnessValues;

    for (int y = interior.top(); y <= interior.bottom(); ++y)
    {
        for (int x = interior.left(); x <= interior.right(); ++x)
        {
            lightnessValues.append(
                QColor::fromRgba(rendering.pixel(x, y)).lightness()
                );
        }
    }

    std::ranges::sort(lightnessValues);

    IndicatorRendering result;
    result.size = indicatorRect.size();

    if (!lightnessValues.isEmpty())
    {
        result.darkestInteriorPixel = lightnessValues.constFirst();
        result.brightestInteriorPixel = lightnessValues.constLast();
        result.medianInteriorPixel =
            lightnessValues.at(lightnessValues.size() / 2);
    }

    return result;
}

IndicatorRendering renderCheckedButtonIndicator(
    const QString& controlType,
    bool enabled
    )
{
    QCheckBox checkBox(QStringLiteral("Option"));
    QRadioButton radioButton(QStringLiteral("Option"));

    QAbstractButton* button =
        controlType == QStringLiteral("radio")
            ? static_cast<QAbstractButton*>(&radioButton)
            : static_cast<QAbstractButton*>(&checkBox);
    const QStyle::SubElement indicatorElement =
        controlType == QStringLiteral("radio")
            ? QStyle::SE_RadioButtonIndicator
            : QStyle::SE_CheckBoxIndicator;

    button->setChecked(true);
    button->setEnabled(enabled);
    button->resize(160, 40);
    button->ensurePolished();

    QStyleOptionButton option;
    option.initFrom(button);
    option.rect = button->rect();
    option.state.setFlag(QStyle::State_On, true);
    option.state.setFlag(QStyle::State_Off, false);

    const QRect indicatorRect =
        button->style()->subElementRect(
            indicatorElement,
            &option,
            button
            );

    QImage rendering(
        button->size(),
        QImage::Format_ARGB32_Premultiplied
        );
    rendering.fill(Qt::transparent);
    button->render(&rendering);

    return analyzeIndicator(rendering, indicatorRect);
}

IndicatorRendering renderCheckedItemViewIndicator(
    bool enabled
    )
{
    QListWidget list;
    list.setEnabled(enabled);
    list.resize(180, 48);

    auto* item =
        new QListWidgetItem(QStringLiteral("Class"), &list);
    item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
    item->setCheckState(Qt::Checked);

    list.show();
    QApplication::processEvents();

    QStyleOptionViewItem option;
    option.initFrom(&list);
    option.rect = list.visualItemRect(item);
    option.features.setFlag(
        QStyleOptionViewItem::HasCheckIndicator,
        true
        );
    option.checkState = Qt::Checked;
    option.state.setFlag(QStyle::State_On, true);
    option.state.setFlag(QStyle::State_Off, false);

    const QRect indicatorRect =
        list.style()->subElementRect(
            QStyle::SE_ItemViewItemCheckIndicator,
            &option,
            &list
            );

    QImage rendering(
        list.viewport()->size(),
        QImage::Format_ARGB32_Premultiplied
        );
    rendering.fill(Qt::transparent);
    list.viewport()->render(&rendering);

    return analyzeIndicator(rendering, indicatorRect);
}
}

class CheckboxStyleTests : public QObject
{
    Q_OBJECT

private slots:
    void checkedIndicatorsAreVisible_data();
    void checkedIndicatorsAreVisible();
    void itemViewIndicatorsAreVisible_data();
    void itemViewIndicatorsAreVisible();
};

void CheckboxStyleTests::checkedIndicatorsAreVisible_data()
{
    QTest::addColumn<QString>("stylesheetPath");
    QTest::addColumn<QString>("controlType");
    QTest::addColumn<bool>("enabled");

    const QList<QPair<QString, QString>> themes{
        {
            QStringLiteral("light"),
            QStringLiteral(":/assets/styles/light.qss")
        },
        {
            QStringLiteral("dark"),
            QStringLiteral(":/assets/styles/dark.qss")
        }
    };

    for (const auto& [theme, stylesheetPath] : themes)
    {
        for (const QString& controlType
             : {QStringLiteral("checkbox"), QStringLiteral("radio")})
        {
            QTest::newRow(
                qPrintable(
                    theme
                    + QStringLiteral("-")
                    + controlType
                    + QStringLiteral("-enabled")
                    )
                )
                << stylesheetPath
                << controlType
                << true;

            QTest::newRow(
                qPrintable(
                    theme
                    + QStringLiteral("-")
                    + controlType
                    + QStringLiteral("-disabled")
                    )
                )
                << stylesheetPath
                << controlType
                << false;
        }
    }
}

void CheckboxStyleTests::checkedIndicatorsAreVisible()
{
    QFETCH(QString, stylesheetPath);
    QFETCH(QString, controlType);
    QFETCH(bool, enabled);

    const QString stylesheet =
        loadStylesheet(stylesheetPath);
    QVERIFY2(
        !stylesheet.isEmpty(),
        qPrintable(QStringLiteral("Unable to load %1").arg(stylesheetPath))
        );

    qApp->setStyleSheet(stylesheet);

    const IndicatorRendering rendering =
        renderCheckedButtonIndicator(controlType, enabled);

    QCOMPARE(rendering.size, QSize(22, 22));
    const int contrast =
        std::max(
            rendering.brightestInteriorPixel
                - rendering.medianInteriorPixel,
            rendering.medianInteriorPixel
                - rendering.darkestInteriorPixel
            );
    QVERIFY2(
        contrast >= 60,
        qPrintable(
            QStringLiteral(
                "Checked indicator contrast was only %1 (brightest %2, median %3, darkest %4)"
                )
                .arg(contrast)
                .arg(rendering.brightestInteriorPixel)
                .arg(rendering.medianInteriorPixel)
                .arg(rendering.darkestInteriorPixel)
            )
        );
}

void CheckboxStyleTests::itemViewIndicatorsAreVisible_data()
{
    QTest::addColumn<QString>("stylesheetPath");

    QTest::newRow("light")
        << QStringLiteral(":/assets/styles/light.qss");
    QTest::newRow("dark")
        << QStringLiteral(":/assets/styles/dark.qss");
}

void CheckboxStyleTests::itemViewIndicatorsAreVisible()
{
    QFETCH(QString, stylesheetPath);

    const QString stylesheet =
        loadStylesheet(stylesheetPath);
    QVERIFY2(
        !stylesheet.isEmpty(),
        qPrintable(QStringLiteral("Unable to load %1").arg(stylesheetPath))
        );

    qApp->setStyleSheet(stylesheet);

    const IndicatorRendering rendering =
        renderCheckedItemViewIndicator(true);

    QCOMPARE(rendering.size, QSize(22, 22));
    QVERIFY2(
        rendering.brightestInteriorPixel
            - rendering.medianInteriorPixel >= 60,
        "The checked item-view indicator does not have a visible tick"
        );
}

QTEST_MAIN(CheckboxStyleTests)

#include "checkbox_style_tests.moc"
