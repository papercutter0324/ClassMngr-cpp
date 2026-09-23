#include "sub_prep_document_model.h"

#include "sub_prep_print_service.h"

#include <functional>

namespace SubPrepDocumentModel
{
Document build(
    const SubPrepPrintService::Request& request
    )
{
    return {
        {
            request.campus.officeNumber,
            request.campus.officeWifi,
            request.campus.officeWifiPassword,
            request.campus.photocopierCode
        },
        {
            request.zoom.loginId,
            request.zoom.password
        },
        request.classMaterials,
        request.gradingInstructions,
        request.specialInstructions,
        request.schedule,
        std::cref(request.classInformation),
        request.subNotes
    };
}
}
