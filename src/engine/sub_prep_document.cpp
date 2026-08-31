#include "classmngr/engine/sub_prep_document.h"

namespace classmngr::engine
{

SubPrepDocument SubPrepDocumentService::build(
    const SubPrepDocumentRequest& request
    )
{
    return {
        request.campus,
        request.zoom,
        request.classMaterials,
        request.gradingInstructions,
        request.specialInstructions,
        request.schedule,
        request.classInformation,
        request.subNotes
    };
}

} // namespace classmngr::engine
