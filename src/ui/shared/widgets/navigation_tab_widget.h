#pragma once

#include <QList>
#include <QPoint>
#include <QWidget>

class QEvent;
class QHBoxLayout;
class QResizeEvent;
class QScrollArea;
class QSpacerItem;
class QStackedWidget;
class QToolButton;
class QVBoxLayout;
class NavigationPillButton;

enum class NavigationTabKind
{
    Grade,
    Class,
    Section
};

enum class NavigationTabAlignment
{
    Leading,
    Center
};

class NavigationTabStrip : public QWidget
{
    Q_OBJECT

public:
    explicit NavigationTabStrip(
        NavigationTabKind kind,
        QWidget* parent = nullptr
        );

    int addTab(const QString& text);
    void removeTab(int index);
    void clear();

    int count() const;
    int currentIndex() const;
    void setCurrentIndex(int index);

    QString tabText(int index) const;
    void setTabText(int index, const QString& text);

    NavigationPillButton* tabButton(int index) const;

    NavigationTabKind tabKind() const;
    NavigationTabAlignment tabAlignment() const;
    void setTabAlignment(NavigationTabAlignment alignment);

    bool selectionVisible() const;
    void setSelectionVisible(bool visible);

    void setTrailingWidget(QWidget* widget);
    QWidget* trailingWidget() const;

    void setTrailingMinimumGap(int gap);

    int contentWidth() const;
    int allTabsContentWidth() const;
    bool hasOverflow() const;

    // Limits the number of adjacent tabs shown at once.  A count greater than
    // or equal to the total tab count shows every tab.
    void setVisibleTabCount(int count);
    int visibleTabCount() const;
    int tabGroupWidth(int visibleTabCount) const;

    void setSingleTabNavigation(bool enabled);
    bool singleTabNavigation() const;
    void refreshLayout();

signals:
    void currentChanged(int index);

protected:
    void changeEvent(QEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    void updateTabSizes();
    void updateLayoutState();
    void updateScrollButtons();
    void scrollByTab(bool forward);
    void ensureCurrentVisible();
    void updateButtonStates();
    void updateVisibleButtons();
    void updateVisibleTabWindow();
    int effectiveVisibleTabCount() const;
    bool usesVisibleTabWindow() const;
    int buttonIndex(QObject* object) const;
    static QString kindPropertyValue(NavigationTabKind kind);

    NavigationTabKind m_kind;
    NavigationTabAlignment m_alignment;
    QHBoxLayout* m_rootLayout = nullptr;
    QToolButton* m_leftButton = nullptr;
    QToolButton* m_rightButton = nullptr;
    QScrollArea* m_scrollArea = nullptr;
    QWidget* m_tabContent = nullptr;
    QHBoxLayout* m_tabLayout = nullptr;
    QWidget* m_trailingWidget = nullptr;
    QSpacerItem* m_trailingSpacer = nullptr;
    int m_trailingMinimumGap = 0;
    QList<NavigationPillButton*> m_buttons;
    int m_currentIndex = -1;
    int m_contentWidth = 0;
    int m_allTabsContentWidth = 0;
    int m_tabWidth = 0;
    int m_requestedVisibleTabCount = 0;
    int m_firstVisibleTabIndex = 0;
    bool m_selectionVisible = true;
    bool m_hasOverflow = false;
    bool m_updatingLayout = false;
    bool m_dragCandidate = false;
    bool m_dragging = false;
    int m_dragPressGlobalX = 0;
    int m_dragStartScrollValue = 0;
    NavigationPillButton* m_dragButton = nullptr;
};

class NavigationTabWidget : public QWidget
{
    Q_OBJECT

public:
    explicit NavigationTabWidget(
        NavigationTabKind kind,
        const QString& tabStripObjectName,
        QWidget* parent = nullptr
        );

    int addTab(QWidget* page, const QString& text);
    void removeTab(int index);
    void clear();

    int count() const;
    QWidget* widget(int index) const;
    QWidget* currentWidget() const;

    int currentIndex() const;
    void setCurrentIndex(int index);

    QString tabText(int index) const;
    void setTabText(int index, const QString& text);

    NavigationTabKind tabKind() const;
    NavigationTabStrip* tabStrip() const;

    void setSelectionVisible(bool visible);
    bool selectionVisible() const;

    void setTrailingWidget(QWidget* widget);
    QWidget* trailingWidget() const;

    // Controls the space between the tab strip and its current page.
    void setPageSpacing(int spacing);

signals:
    void currentChanged(int index);

private:
    QVBoxLayout* m_layout = nullptr;
    NavigationTabStrip* m_tabStrip = nullptr;
    QStackedWidget* m_stack = nullptr;
};
