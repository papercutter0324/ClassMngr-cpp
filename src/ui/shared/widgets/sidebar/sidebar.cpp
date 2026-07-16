#include "sidebar_p.h"

Sidebar::Sidebar(QWidget *parent)
    : QWidget(parent)
{
    qRegisterMetaType<NavigationData>("NavigationData");

    setupUi();

    setupSignals();

    buildTree();
}



// =========================================================
// Overflow Display
// =========================================================

void Sidebar::setOverflowTooltipsEnabled(
    bool enabled
    )
{
    if (m_overflowTooltipsEnabled == enabled)
    {
        return;
    }

    m_overflowTooltipsEnabled = enabled;

    updateOverflowTooltips();
}

void Sidebar::setOverflowMarqueeEnabled(
    bool enabled
    )
{
    if (m_overflowMarqueeEnabled == enabled)
    {
        return;
    }

    m_overflowMarqueeEnabled = enabled;

    if (m_marqueeDelegate)
    {
        m_marqueeDelegate->setMarqueeEnabled(
            enabled
            );
    }
}



// =========================================================
// Resize
// =========================================================

void Sidebar::resizeEvent(
    QResizeEvent* event
    )
{
    QWidget::resizeEvent(event);

    updateOverflowTooltips();

    if (m_marqueeDelegate)
    {
        m_marqueeDelegate->resetMarquee();
    }
}



// =========================================================
// Setup UI
// =========================================================

void Sidebar::setupUi()
{
    auto *layout =
        new QVBoxLayout(this);

    m_tree =
        new QTreeWidget(this);
    m_tree->setStyle(
        new SidebarTreeStyle(m_tree)
        );

    m_tree->setObjectName(
        QStringLiteral("sidebarTree")
        );
    m_tree->setStyleSheet(
        QStringLiteral(
            "QTreeWidget { border-width: %1px; border-style: solid; "
            "border-radius: %2px; }"
            ).arg(
                UiConstants::MainWindow::SidebarFrameWidth
                ).arg(
                UiConstants::MainWindow::SidebarFrameRadius
                )
        );

    m_tree->setFont(
        FontManager::getUiFont(
            FontManager::stdEnglishFont
            )
        );

    m_tree->setHeaderHidden(true);

    m_tree->setUniformRowHeights(true);

    m_tree->setIndentation(12);

    m_tree->setHorizontalScrollBarPolicy(
        Qt::ScrollBarAsNeeded
        );

    m_tree->setTextElideMode(
        Qt::ElideNone
        );

    m_tree->setWordWrap(false);

    m_tree->header()->setSectionResizeMode(
        0,
        QHeaderView::ResizeToContents
        );

    m_tree->header()->setStretchLastSection(false);

    m_marqueeDelegate =
        new SidebarMarqueeDelegate(
            m_tree,
            m_tree
            );

    m_tree->setItemDelegate(
        m_marqueeDelegate
        );

    layout->addWidget(m_tree);

    layout->setContentsMargins(
        5,
        8,
        5,
        18
        );
}



// =========================================================
// Setup Signals
// =========================================================

void Sidebar::setupSignals()
{
    connect(
        m_tree,
        &QTreeWidget::itemClicked,
        this,
        &Sidebar::onItemClicked
        );

    connect(
        m_tree,
        &QTreeWidget::currentItemChanged,
        this,
        [this](
            QTreeWidgetItem*,
            QTreeWidgetItem* previous
            )
        {
            m_previousCurrentItem =
                previous;
        }
        );

    m_tree->setContextMenuPolicy(
        Qt::CustomContextMenu
        );

    connect(
        m_tree,
        &QWidget::customContextMenuRequested,
        this,
        &Sidebar::showContextMenu
        );

    connect(
        m_tree,
        &QTreeWidget::itemExpanded,
        this,
        [this](QTreeWidgetItem*)
        {
            updateTreeColumnWidth();

            if (m_marqueeDelegate)
            {
                m_marqueeDelegate->resetMarquee();
            }
        }
        );

    connect(
        m_tree,
        &QTreeWidget::itemCollapsed,
        this,
        [this](QTreeWidgetItem*)
        {
            updateTreeColumnWidth();

            if (m_marqueeDelegate)
            {
                m_marqueeDelegate->resetMarquee();
            }
        }
        );
}



// =========================================================
// Build Tree
// =========================================================
