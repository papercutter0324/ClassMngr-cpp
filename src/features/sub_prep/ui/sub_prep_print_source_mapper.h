#pragma once

#include "features/sub_prep/ui/sub_prep_class_information_model.h"
#include "next/application/sub_prep_print_source_query.h"

#include <QList>

namespace SubPrepPrintSourceMapper
{
[[nodiscard]] ClassMngr::Next::Domain::Result<
    QList<SubPrepClassInformation::TeacherGroup>
    > toClassInformation(
        const ClassMngr::Next::Application::SubPrepPrintSource& source,
        const ClassMngr::Next::Application::SubPrepPrintSourceRequest& request
        );
}
