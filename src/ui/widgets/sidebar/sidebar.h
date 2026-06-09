#ifndef SIDEBAR_H
#define SIDEBAR_H

#include "sidebar_types.h"

#include <QHash>
#include <QTreeWidgetItem>
#include <QWidget>

class QTreeWidget;



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
    // Classes
    // =====================================================

    void addClassNode(
        const QString &displayName,
        int classId
        );

    void clearClasses();

    void selectClass(
        int classId
        );

    int getSelectedClassId() const;



    // =====================================================
    // Teachers
    // =====================================================

    void addTeacherNode(
        const QString &displayName,
        int teacherId
        );

    void clearTeachers();

    void selectTeacher(
        int teacherId
        );

    int getSelectedTeacherId() const;



signals:

    void itemSelected(
        const NavigationData &data
        );

    void addClassRequested();

    void deleteClassRequested();

    void addTeacherRequested();

    void deleteTeacherRequested();



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
        bool selectable = true
        );

    QStringList getItemPath(
        QTreeWidgetItem *item
        ) const;

    bool isClassItem(
        QTreeWidgetItem *item
        ) const;



    // =====================================================
    // Widgets
    // =====================================================

    QTreeWidget *m_tree = nullptr;



    // =====================================================
    // Tree References
    // =====================================================

    QHash<
        QString,
        QTreeWidgetItem*
        > m_nodes;

    QHash<
        int,
        QTreeWidgetItem*
        > m_classItems;

    QHash<
        int,
        QTreeWidgetItem*
        > m_teacherItems;
};



#endif // SIDEBAR_H
