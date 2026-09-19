include_guard(GLOBAL)

classmngr_add_qt_test(
    NAME NextDomainContract
    SOURCES
        tests/next_domain_contract_tests.cpp
    LIBRARIES
        ClassMngrNext::Domain
        Qt6::Test
)

classmngr_add_qt_test(
    NAME NextApplicationContract
    SOURCES
        tests/next_application_contract_tests.cpp
    LIBRARIES
        ClassMngrNext::Application
        Qt6::Test
)

classmngr_add_qt_test(
    NAME NextApplicationState
    SOURCES
        tests/next_application_state_tests.cpp
    LIBRARIES
        ClassMngrNext::Application
        Qt6::Test
)

classmngr_add_qt_test(
    NAME NextApplicationWorkspaceCoordinator
    SOURCES
        tests/next_application_workspace_coordinator_tests.cpp
    LIBRARIES
        ClassMngrNext::Application
        Qt6::Test
)

classmngr_add_qt_test(
    NAME NextApplicationSelection
    SOURCES
        tests/next_application_selection_tests.cpp
    LIBRARIES
        ClassMngrNext::Application
        Qt6::Test
)

classmngr_add_qt_test(
    NAME NextApplicationImportJob
    SOURCES
        tests/next_application_import_job_tests.cpp
    LIBRARIES
        ClassMngrNext::Application
        Qt6::Test
)

classmngr_add_qt_test(
    NAME NextApplicationImportJobCoordinator
    SOURCES
        tests/next_application_import_job_coordinator_tests.cpp
    LIBRARIES
        ClassMngrNext::Application
        Qt6::Test
)

classmngr_add_qt_test(
    NAME NextApplicationReportJob
    SOURCES
        tests/next_application_report_job_tests.cpp
    LIBRARIES
        ClassMngrNext::Application
        Qt6::Test
)

classmngr_add_qt_test(
    NAME NextApplicationReportJobCoordinator
    SOURCES
        tests/next_application_report_job_coordinator_tests.cpp
    LIBRARIES
        ClassMngrNext::Application
        Qt6::Test
)

classmngr_add_qt_test(
    NAME NextApplicationImportReview
    SOURCES
        tests/next_application_import_review_tests.cpp
    LIBRARIES
        ClassMngrNext::Application
        Qt6::Test
)

classmngr_add_qt_test(
    NAME NextApplicationDocumentCatalog
    SOURCES
        tests/next_application_document_catalog_tests.cpp
    LIBRARIES
        ClassMngrNext::Application
        Qt6::Test
)

classmngr_add_qt_test(
    NAME NextApplicationDocumentCatalogUseCase
    SOURCES
        tests/next_application_document_catalog_use_case_tests.cpp
    LIBRARIES
        ClassMngrNext::Application
        Qt6::Test
)

classmngr_add_qt_test(
    NAME NextApplicationDocumentContent
    SOURCES
        tests/next_application_document_content_tests.cpp
    LIBRARIES
        ClassMngrNext::Application
        Qt6::Test
)
