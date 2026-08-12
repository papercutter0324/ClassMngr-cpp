#include "navigation_tab_widget.h"

#include "ui/shared/widgets/navigation_pill_button.h"
#include "ui/shared/widgets/navigation_pill_style.h"
#include "ui/shared/widgets/navigation_settings_button.h"

#include <algorithm>

#include <QApplication>
#include <QEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QScrollArea>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QStackedWidget>
#include <QStyle>
#include <QToolButton>
#include <QVBoxLayout>

namespace
{
constexpr int ScrollButtonWidth = 28;
constexpr int TabContentSpacing = NavigationPillStyle::Gap;
constexpr int PageSpacing = 8;
constexpr int InitialStripHeight =
    NavigationPillStyle::ControlHeight
    + NavigationPillStyle::RowBottomSpacing;
}

NavigationTabStrip::NavigationTabStrip(
    NavigationTabKind kind,
    QWidget* parent
    )
    : QWidget(parent),
      m_kind(kind),
      m_alignment(
          kind == NavigationTabKind::Grade
              ? NavigationTabAlignment::Leading
              : NavigationTabAlignment::Center
          )
{
    setProperty("navigationTabKind", kindPropertyValue(kind));
    setFixedHeight(InitialStripHeight);
    setMinimumWidth(0);

    m_rootLayout = new QHBoxLayout(this);
    m_rootLayout->setContentsMargins(0, 0, 0, 0);
    m_rootLayout->setSpacing(0);

    m_leftButton = new QToolButton(this);
    m_leftButton->setObjectName(
        QStringLiteral("NavigationTabScrollLeftButton")
        );
    m_leftButton->setArrowType(Qt::LeftArrow);
    m_leftButton->setFixedSize(
        ScrollButtonWidth,
        NavigationPillStyle::ControlHeight
        );
    m_leftButton->setAutoRepeat(true);
    m_leftButton->setAutoRepeatDelay(350);
    m_leftButton->setAutoRepeatInterval(90);
    m_leftButton->setAccessibleName(tr("Scroll tabs left"));

    m_rightButton = new QToolButton(this);
    m_rightButton->setObjectName(
        QStringLiteral("NavigationTabScrollRightButton")
        );
    m_rightButton->setArrowType(Qt::RightArrow);
    m_rightButton->setFixedSize(
        ScrollButtonWidth,
        NavigationPillStyle::ControlHeight
        );
    m_rightButton->setAutoRepeat(true);
    m_rightButton->setAutoRepeatDelay(350);
    m_rightButton->setAutoRepeatInterval(90);
    m_rightButton->setAccessibleName(tr("Scroll tabs right"));

    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setObjectName(QStringLiteral("navigationTabViewport"));
    m_scrollArea->setFrameShape(QFrame::NoFrame);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scrollArea->setWidgetResizable(false);
    m_scrollArea->setMinimumWidth(0);
    m_scrollArea->setFixedHeight(InitialStripHeight);
    m_scrollArea->setSizePolicy(
        QSizePolicy::Expanding,
        QSizePolicy::Fixed
        );

    m_tabContent = new QWidget;
    m_tabContent->setObjectName(QStringLiteral("navigationTabContent"));
    m_tabContent->setFixedHeight(InitialStripHeight);
    m_tabLayout = new QHBoxLayout(m_tabContent);
    m_tabLayout->setContentsMargins(0, 0, 0, 0);
    m_tabLayout->setSpacing(TabContentSpacing);
    m_scrollArea->setWidget(m_tabContent);

    m_rootLayout->addWidget(m_leftButton, 0, Qt::AlignTop);
    m_rootLayout->addWidget(m_scrollArea, 1, Qt::AlignTop);
    m_rootLayout->addWidget(m_rightButton, 0, Qt::AlignTop);

    m_leftButton->hide();
    m_rightButton->hide();

    m_scrollArea->viewport()->installEventFilter(this);
    m_tabContent->installEventFilter(this);

    connect(
        m_leftButton,
        &QToolButton::clicked,
        this,
        [this]
        {
            scrollByTab(false);
        }
        );
    connect(
        m_rightButton,
        &QToolButton::clicked,
        this,
        [this]
        {
            scrollByTab(true);
        }
        );
    connect(
        m_scrollArea->horizontalScrollBar(),
        &QScrollBar::valueChanged,
        this,
        [this]
        {
            updateScrollButtons();
        }
        );
    connect(
        m_scrollArea->horizontalScrollBar(),
        &QScrollBar::rangeChanged,
        this,
        [this]
        {
            updateScrollButtons();
            ensureCurrentVisible();
        }
        );

    updateLayoutState();
}

int NavigationTabStrip::addTab(const QString& text)
{
    auto* button = new NavigationPillButton(m_tabContent);
    button->setObjectName(QStringLiteral("navigationTabButton"));
    button->setProperty("navigationTab", true);
    button->setText(text);
    button->setCheckable(true);
    button->setFocusPolicy(Qt::StrongFocus);
    button->installEventFilter(this);
    m_tabLayout->addWidget(button, 0, Qt::AlignTop);

    const int index = m_buttons.size();
    m_buttons.append(button);

    connect(
        button,
        &QPushButton::clicked,
        this,
        [this, button]
        {
            if (!m_dragging)
            {
                setCurrentIndex(m_buttons.indexOf(button));
            }
        }
        );

    updateTabSizes();

    if (m_currentIndex < 0)
    {
        setCurrentIndex(0);
    }

    return index;
}

void NavigationTabStrip::removeTab(int index)
{
    if (index < 0 || index >= m_buttons.size())
    {
        return;
    }

    NavigationPillButton* button = m_buttons.takeAt(index);
    m_tabLayout->removeWidget(button);
    button->removeEventFilter(this);
    button->deleteLater();

    const int previousIndex = m_currentIndex;
    if (m_buttons.isEmpty())
    {
        m_currentIndex = -1;
    }
    else if (index < previousIndex)
    {
        m_currentIndex = previousIndex - 1;
    }
    else if (index == previousIndex)
    {
        m_currentIndex = std::min(
            index,
            static_cast<int>(m_buttons.size()) - 1
            );
    }

    updateTabSizes();
    updateButtonStates();

    if (m_currentIndex != previousIndex || index == previousIndex)
    {
        emit currentChanged(m_currentIndex);
    }
}

void NavigationTabStrip::clear()
{
    while (!m_buttons.isEmpty())
    {
        removeTab(m_buttons.size() - 1);
    }
}

int NavigationTabStrip::count() const
{
    return m_buttons.size();
}

int NavigationTabStrip::currentIndex() const
{
    return m_currentIndex;
}

void NavigationTabStrip::setCurrentIndex(int index)
{
    if (index < -1 || index >= m_buttons.size())
    {
        return;
    }

    if (m_currentIndex == index)
    {
        updateButtonStates();
        ensureCurrentVisible();
        return;
    }

    m_currentIndex = index;
    updateButtonStates();
    ensureCurrentVisible();
    emit currentChanged(index);
}

QString NavigationTabStrip::tabText(int index) const
{
    NavigationPillButton* button = tabButton(index);
    return button ? button->text() : QString();
}

void NavigationTabStrip::setTabText(
    int index,
    const QString& text
    )
{
    NavigationPillButton* button = tabButton(index);
    if (!button || button->text() == text)
    {
        return;
    }

    button->setText(text);
    updateTabSizes();
}

NavigationPillButton* NavigationTabStrip::tabButton(int index) const
{
    return index >= 0 && index < m_buttons.size()
        ? m_buttons.at(index)
        : nullptr;
}

NavigationTabKind NavigationTabStrip::tabKind() const
{
    return m_kind;
}

NavigationTabAlignment NavigationTabStrip::tabAlignment() const
{
    return m_alignment;
}

void NavigationTabStrip::setTabAlignment(
    NavigationTabAlignment alignment
    )
{
    if (m_alignment == alignment)
    {
        return;
    }

    m_alignment = alignment;
    updateLayoutState();
}

bool NavigationTabStrip::selectionVisible() const
{
    return m_selectionVisible;
}

void NavigationTabStrip::setSelectionVisible(bool visible)
{
    if (m_selectionVisible == visible)
    {
        return;
    }

    m_selectionVisible = visible;
    updateButtonStates();
}

void NavigationTabStrip::setTrailingWidget(QWidget* widget)
{
    if (m_trailingWidget == widget)
    {
        return;
    }

    if (m_trailingWidget)
    {
        m_rootLayout->removeWidget(m_trailingWidget);
        m_trailingWidget->setParent(nullptr);
    }

    m_trailingWidget = widget;
    if (m_trailingWidget)
    {
        m_trailingWidget->setParent(this);
        m_rootLayout->addWidget(
            m_trailingWidget,
            0,
            Qt::AlignTop
            );
        m_trailingWidget->show();
    }

    updateTabSizes();
}

QWidget* NavigationTabStrip::trailingWidget() const
{
    return m_trailingWidget;
}

int NavigationTabStrip::contentWidth() const
{
    return m_contentWidth;
}

bool NavigationTabStrip::hasOverflow() const
{
    return m_hasOverflow;
}

void NavigationTabStrip::changeEvent(QEvent* event)
{
    QWidget::changeEvent(event);

    if (
        event
        && (
            event->type() == QEvent::FontChange
            || event->type() == QEvent::ApplicationFontChange
            || event->type() == QEvent::StyleChange
            || event->type() == QEvent::PaletteChange
            )
        )
    {
        updateTabSizes();
    }
}

bool NavigationTabStrip::eventFilter(
    QObject* watched,
    QEvent* event
    )
{
    if (!event)
    {
        return QWidget::eventFilter(watched, event);
    }

    const int index = buttonIndex(watched);

    if (
        index >= 0
        && (
            event->type() == QEvent::FontChange
            || event->type() == QEvent::StyleChange
            )
        )
    {
        updateTabSizes();
    }

    if (index >= 0 && event->type() == QEvent::KeyPress)
    {
        auto* keyEvent = static_cast<QKeyEvent*>(event);
        int targetIndex = -1;

        switch (keyEvent->key())
        {
        case Qt::Key_Left:
            targetIndex = std::max(0, index - 1);
            break;

        case Qt::Key_Right:
            targetIndex = std::min(
                static_cast<int>(m_buttons.size()) - 1,
                index + 1
                );
            break;

        case Qt::Key_Home:
            targetIndex = 0;
            break;

        case Qt::Key_End:
            targetIndex = m_buttons.size() - 1;
            break;

        case Qt::Key_Space:
        case Qt::Key_Return:
        case Qt::Key_Enter:
            targetIndex = index;
            break;

        default:
            break;
        }

        if (targetIndex >= 0)
        {
            setCurrentIndex(targetIndex);
            m_buttons.at(targetIndex)->setFocus(Qt::TabFocusReason);
            event->accept();
            return true;
        }
    }

    if (event->type() == QEvent::Wheel)
    {
        event->ignore();
        return true;
    }

    if (event->type() == QEvent::MouseButtonPress)
    {
        auto* mouseEvent = static_cast<QMouseEvent*>(event);
        if (mouseEvent->button() == Qt::LeftButton && m_hasOverflow)
        {
            m_dragCandidate = true;
            m_dragging = false;
            m_dragPressGlobalX =
                qRound(mouseEvent->globalPosition().x());
            m_dragStartScrollValue =
                m_scrollArea->horizontalScrollBar()->value();
            m_dragButton = index >= 0 ? m_buttons.at(index) : nullptr;
        }
    }
    else if (
        event->type() == QEvent::MouseMove
        && m_dragCandidate
        )
    {
        auto* mouseEvent = static_cast<QMouseEvent*>(event);
        const int distance =
            qRound(mouseEvent->globalPosition().x())
            - m_dragPressGlobalX;

        if (
            !m_dragging
            && std::abs(distance) >= QApplication::startDragDistance()
            )
        {
            m_dragging = true;
            setCursor(Qt::ClosedHandCursor);
            if (m_dragButton)
            {
                m_dragButton->setDown(false);
            }
        }

        if (m_dragging)
        {
            m_scrollArea->horizontalScrollBar()->setValue(
                m_dragStartScrollValue - distance
                );
            event->accept();
            return true;
        }
    }
    else if (event->type() == QEvent::MouseButtonRelease)
    {
        auto* mouseEvent = static_cast<QMouseEvent*>(event);
        if (
            mouseEvent->button() == Qt::LeftButton
            && m_dragCandidate
            )
        {
            const bool wasDragging = m_dragging;
            m_dragCandidate = false;
            m_dragging = false;
            m_dragButton = nullptr;
            unsetCursor();

            if (wasDragging)
            {
                event->accept();
                return true;
            }
        }
    }

    return QWidget::eventFilter(watched, event);
}

void NavigationTabStrip::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    updateLayoutState();
}

void NavigationTabStrip::updateTabSizes()
{
    int widestWidth = 0;
    int tallestHeight = NavigationPillStyle::ControlHeight;
    const QList<NavigationPillButton*> trailingButtons =
        m_trailingWidget
            ? m_trailingWidget->findChildren<NavigationPillButton*>()
            : QList<NavigationPillButton*>();
    const QList<NavigationSettingsButton*> settingsButtons =
        m_trailingWidget
            ? m_trailingWidget->findChildren<NavigationSettingsButton*>()
            : QList<NavigationSettingsButton*>();

    for (NavigationPillButton* button : m_buttons)
    {
        button->setMinimumWidth(0);
        button->setMaximumWidth(QWIDGETSIZE_MAX);
        const QSize requiredSize = button->sizeHint();
        widestWidth = std::max(widestWidth, requiredSize.width());
        tallestHeight = std::max(tallestHeight, requiredSize.height());
    }

    for (NavigationPillButton* button : trailingButtons)
    {
        tallestHeight = std::max(
            tallestHeight,
            button->sizeHint().height()
            );
    }

    for (NavigationSettingsButton* button : settingsButtons)
    {
        tallestHeight = std::max(
            tallestHeight,
            button->sizeHint().height()
            );
    }

    for (NavigationPillButton* button : m_buttons)
    {
        button->setFixedSize(widestWidth, tallestHeight);
    }

    for (NavigationPillButton* button : trailingButtons)
    {
        button->setFixedHeight(tallestHeight);
    }

    for (NavigationSettingsButton* button : settingsButtons)
    {
        button->setFixedHeight(tallestHeight);
    }

    const int stripHeight =
        tallestHeight + NavigationPillStyle::RowBottomSpacing;
    setFixedHeight(stripHeight);
    m_leftButton->setFixedSize(ScrollButtonWidth, tallestHeight);
    m_rightButton->setFixedSize(ScrollButtonWidth, tallestHeight);
    m_scrollArea->setFixedHeight(stripHeight);

    m_contentWidth =
        m_buttons.isEmpty()
            ? 0
            : (widestWidth * static_cast<int>(m_buttons.size()))
                + (
                    TabContentSpacing
                    * (static_cast<int>(m_buttons.size()) - 1)
                    );
    m_tabContent->setFixedSize(
        m_contentWidth,
        stripHeight
        );

    updateLayoutState();
}

void NavigationTabStrip::updateLayoutState()
{
    if (m_updatingLayout)
    {
        return;
    }

    m_updatingLayout = true;

    const int trailingWidth =
        m_trailingWidget && m_trailingWidget->isVisible()
            ? std::max(
                m_trailingWidget->width(),
                m_trailingWidget->sizeHint().width()
                )
            : 0;
    const int availableWithoutArrows =
        std::max(0, width() - trailingWidth);
    const bool overflow =
        m_contentWidth > availableWithoutArrows;

    if (m_hasOverflow != overflow)
    {
        m_hasOverflow = overflow;
        m_leftButton->setVisible(overflow);
        m_rightButton->setVisible(overflow);
    }

    m_scrollArea->setAlignment(
        !m_hasOverflow && m_alignment == NavigationTabAlignment::Center
            ? Qt::AlignHCenter | Qt::AlignTop
            : Qt::AlignLeft | Qt::AlignTop
        );

    m_rootLayout->activate();
    m_updatingLayout = false;

    updateScrollButtons();
    ensureCurrentVisible();
}

void NavigationTabStrip::updateScrollButtons()
{
    QScrollBar* scrollBar = m_scrollArea->horizontalScrollBar();
    m_leftButton->setEnabled(
        m_hasOverflow && scrollBar->value() > scrollBar->minimum()
        );
    m_rightButton->setEnabled(
        m_hasOverflow && scrollBar->value() < scrollBar->maximum()
        );
}

void NavigationTabStrip::scrollByTab(bool forward)
{
    if (!m_hasOverflow || m_buttons.isEmpty())
    {
        return;
    }

    QScrollBar* scrollBar = m_scrollArea->horizontalScrollBar();
    const int step = m_buttons.first()->width() + TabContentSpacing;
    const int value = scrollBar->value();
    int target = 0;

    if (forward)
    {
        target = ((value / step) + 1) * step;
    }
    else
    {
        target = value % step == 0
            ? value - step
            : (value / step) * step;
    }

    scrollBar->setValue(
        std::clamp(target, scrollBar->minimum(), scrollBar->maximum())
        );
}

void NavigationTabStrip::ensureCurrentVisible()
{
    if (
        !m_hasOverflow
        || m_currentIndex < 0
        || m_currentIndex >= m_buttons.size()
        )
    {
        return;
    }

    NavigationPillButton* button = m_buttons.at(m_currentIndex);
    QScrollBar* scrollBar = m_scrollArea->horizontalScrollBar();
    const int viewportWidth = m_scrollArea->viewport()->width();
    const int visibleLeft = scrollBar->value();
    const int visibleRight = visibleLeft + viewportWidth - 1;

    if (button->geometry().left() < visibleLeft)
    {
        scrollBar->setValue(button->geometry().left());
    }
    else if (button->geometry().right() > visibleRight)
    {
        scrollBar->setValue(
            button->geometry().right() - viewportWidth + 1
            );
    }
}

void NavigationTabStrip::updateButtonStates()
{
    for (int index = 0; index < m_buttons.size(); ++index)
    {
        NavigationPillButton* button = m_buttons.at(index);
        const QSignalBlocker blocker(button);
        button->setChecked(
            m_selectionVisible && index == m_currentIndex
            );
    }
}

int NavigationTabStrip::buttonIndex(QObject* object) const
{
    return m_buttons.indexOf(
        qobject_cast<NavigationPillButton*>(object)
        );
}

QString NavigationTabStrip::kindPropertyValue(NavigationTabKind kind)
{
    switch (kind)
    {
    case NavigationTabKind::Grade:
        return QStringLiteral("grade");

    case NavigationTabKind::Class:
        return QStringLiteral("class");

    case NavigationTabKind::Section:
        return QStringLiteral("section");
    }

    return QString();
}

NavigationTabWidget::NavigationTabWidget(
    NavigationTabKind kind,
    const QString& tabStripObjectName,
    QWidget* parent
    )
    : QWidget(parent)
{
    setProperty(
        "navigationTabKind",
        kind == NavigationTabKind::Grade
            ? QStringLiteral("grade")
            : kind == NavigationTabKind::Class
                ? QStringLiteral("class")
                : QStringLiteral("section")
        );

    m_layout = new QVBoxLayout(this);
    m_layout->setContentsMargins(0, 0, 0, 0);
    m_layout->setSpacing(PageSpacing);

    m_tabStrip = new NavigationTabStrip(kind, this);
    m_tabStrip->setObjectName(tabStripObjectName);
    m_stack = new QStackedWidget(this);
    m_stack->setObjectName(QStringLiteral("navigationTabStack"));

    m_layout->addWidget(m_tabStrip);
    m_layout->addWidget(m_stack);

    connect(
        m_tabStrip,
        &NavigationTabStrip::currentChanged,
        this,
        [this](int index)
        {
            m_stack->setCurrentIndex(index);
            emit currentChanged(index);
        }
        );
}

int NavigationTabWidget::addTab(
    QWidget* page,
    const QString& text
    )
{
    if (!page)
    {
        return -1;
    }

    const int index = m_stack->addWidget(page);
    m_tabStrip->addTab(text);
    m_stack->setCurrentIndex(m_tabStrip->currentIndex());
    return index;
}

void NavigationTabWidget::removeTab(int index)
{
    QWidget* page = widget(index);
    if (!page)
    {
        return;
    }

    m_stack->removeWidget(page);
    m_tabStrip->removeTab(index);
    m_stack->setCurrentIndex(m_tabStrip->currentIndex());
}

void NavigationTabWidget::clear()
{
    while (count() > 0)
    {
        removeTab(count() - 1);
    }
}

int NavigationTabWidget::count() const
{
    return m_stack->count();
}

QWidget* NavigationTabWidget::widget(int index) const
{
    return m_stack->widget(index);
}

QWidget* NavigationTabWidget::currentWidget() const
{
    return m_stack->currentWidget();
}

int NavigationTabWidget::currentIndex() const
{
    return m_tabStrip->currentIndex();
}

void NavigationTabWidget::setCurrentIndex(int index)
{
    m_tabStrip->setCurrentIndex(index);
    m_stack->setCurrentIndex(m_tabStrip->currentIndex());
}

QString NavigationTabWidget::tabText(int index) const
{
    return m_tabStrip->tabText(index);
}

void NavigationTabWidget::setTabText(
    int index,
    const QString& text
    )
{
    m_tabStrip->setTabText(index, text);
}

NavigationTabKind NavigationTabWidget::tabKind() const
{
    return m_tabStrip->tabKind();
}

NavigationTabStrip* NavigationTabWidget::tabStrip() const
{
    return m_tabStrip;
}

void NavigationTabWidget::setSelectionVisible(bool visible)
{
    m_tabStrip->setSelectionVisible(visible);
}

bool NavigationTabWidget::selectionVisible() const
{
    return m_tabStrip->selectionVisible();
}

void NavigationTabWidget::setTrailingWidget(QWidget* widget)
{
    m_tabStrip->setTrailingWidget(widget);
}

QWidget* NavigationTabWidget::trailingWidget() const
{
    return m_tabStrip->trailingWidget();
}
