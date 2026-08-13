#pragma once

#include "sidebar_controller.h"

#include "core/application_services.h"
#include "core/settingsmanager.h"
#include "core/utils/sidebar_node_naming.h"
#include "data/data_service.h"
#include "domain/models/class_info.h"
#include "domain/models/classroom.h"
#include "domain/models/teacher.h"
#include "features/classes/config/class_info_config.h"
#include "features/classes/ui/classes_page.h"
#include "features/teacher/ui/teacher_info_page.h"
#include "ui/shared/actions/action_registry.h"
#include "ui/shared/pages/pagemanager.h"
#include "ui/shared/widgets/sidebar/sidebar.h"

#include <QList>
#include <QPair>
#include <QString>

class ApplicationServices;
class DataService;
class TeacherService;
class QWidget;

namespace SidebarControllerPrivate
{

struct SidebarClassNode
{
    int classId = -1;
    ClassInfo classInfo;
    QString displayName;
    QString teacherKr;
};

DataService* openDataService(ApplicationServices* services);
TeacherService* openTeacherService(ApplicationServices* services);
bool sidebarClassNodeLessThan(
    const SidebarClassNode& left,
    const SidebarClassNode& right
    );
QList<Teacher> sortedTeachers(QList<Teacher> teachers);
int chooseRecord(
    QWidget* parent,
    const QString& title,
    const QString& prompt,
    const QList<QPair<QString, int>>& records
    );

} // namespace SidebarControllerPrivate

