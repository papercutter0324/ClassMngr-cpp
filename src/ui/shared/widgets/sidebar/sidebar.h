#ifndef SIDEBAR_H
#define SIDEBAR_H

#include "sidebar_types.h"

#include <QHash>
#include <QList>
#include <QTreeWidgetItem>
#include <QWidget>

class QTreeWidget;
class QResizeEvent;
class DocumentCatalog;
class SidebarMarqueeDelegate;
struct TreeNodeSpec;



// =========================================================
// Sidebar
// =========================================================

class Sidebar : public QWidget
{
    Q_OBJECT

public:

    explicit Sidebar(
        QWidget *parent = nullptr
        );


    // =====================================================
    // Teachers
    // =====================================================

    void addTeacherNode(
        const QString &displayName,
        int teacherId,
        bool myCoTeacher
        );

    void clearTeachers();

    void selectTeacher(
        int teacherId
        );

    int getSelectedTeacherId() const;



    // =====================================================
    // Database-Backed Sections
    // =====================================================

    void setDatabaseSectionsVisible(
        bool visible
        );

    void rebuildTree();

    void setDocumentCatalog(
        const DocumentCatalog* catalog,
        const QString& localeName
        );

    QStringList expandedRootKeys() const;

    void restoreExpandedRootKeys(
        const QStringList& keys
        );

    QStringList selectedKeys() const;

    void selectByKeys(
        const QStringList& keys,
        int teacherId = -1
        );



    // =====================================================
    // Sub Prep
    // =====================================================

    void selectMyInfoSection(
        const QString& sectionName
        );

    void selectCampusSection(
        const QString& sectionName
        );



    // =====================================================
    // Overflow Display
    // =====================================================

    void setOverflowTooltipsEnabled(
        bool enabled
        );

    void setOverflowMarqueeEnabled(
        bool enabled
        );



signals:

    void itemSelected(
        const NavigationData &data
        );

    void addClassRequested();

    void addTeacherRequested();

    void deleteTeacherRequested();



protected:

    void resizeEvent(
        QResizeEvent* event
        ) override;



private slots:

    void onItemClicked(
        QTreeWidgetItem *item,
        int column
        );

    void showContextMenu(
        const QPoint &position
        );



private:

    // =====================================================
    // Setup
    // =====================================================

    void setupUi();

    void setupSignals();

    void buildTree();



    // =====================================================
    // Helpers
    // =====================================================

    QTreeWidgetItem* createItem(
        const QString &label,
        NodeType type,
        bool selectable = true,
        const QString& key = QString()
        );

    QTreeWidgetItem* createItem(
        const TreeNodeSpec& spec
        );

    QStringList getItemPath(
        QTreeWidgetItem *item
        ) const;

    QStringList getItemKeys(
        QTreeWidgetItem* item
        ) const;

    QTreeWidgetItem* childWithText(
        QTreeWidgetItem* item,
        const QString& text
        ) const;

    QTreeWidgetItem* childWithKey(
        QTreeWidgetItem* item,
        const QString& key
        ) const;

    void updateTreeColumnWidth();

    void updateOverflowTooltips();

    void updateItemOverflowTooltips(
        QTreeWidgetItem* item
        );

    bool isItemTextOverflowing(
        QTreeWidgetItem* item
        ) const;

    int itemDepth(
        QTreeWidgetItem* item
        ) const;



    // =====================================================
    // Widgets
    // =====================================================

    QTreeWidget *m_tree = nullptr;

    const DocumentCatalog* m_documentCatalog = nullptr;
    QString m_documentLocaleName;

    SidebarMarqueeDelegate* m_marqueeDelegate = nullptr;

    bool m_overflowTooltipsEnabled = true;

    bool m_overflowMarqueeEnabled = false;

    bool m_databaseSectionsVisible = true;

    QTreeWidgetItem* m_previousCurrentItem = nullptr;



    // =====================================================
    // Tree References
    // =====================================================

    QHash<
        QString,
        QTreeWidgetItem*
        > m_nodes;

    QHash<
        int,
        QList<QTreeWidgetItem*>
        > m_teacherItems;
};



#endif // SIDEBAR_H
