cmake_minimum_required(VERSION 3.21)

project(ClassMngr
    VERSION 0.1.0
    LANGUAGES CXX
)

find_package(Qt6 6.11.1 REQUIRED COMPONENTS
    Core
    Gui
    Sql
    Widgets
    Network
    LinguistTools
)

qt_standard_project_setup()

qt_add_executable(ClassMngr
    WIN32
    MACOSX_BUNDLE

    src/main.cpp

    src/app/mainwindow.cpp
    src/app/mainwindow.h
    src/app/mainwindow.ui
    src/app/menu_builder.cpp
    src/app/menu_builder.h

    src/config/class_info_config.cpp
    src/config/class_info_config.h

    src/controllers/class_controller.cpp
    src/controllers/class_controller.h
    src/controllers/edit_controller.cpp
    src/controllers/edit_controller.h
    src/controllers/file_controller.cpp
    src/controllers/file_controller.h
    src/controllers/navigation_controller.cpp
    src/controllers/navigation_controller.h

    src/core/appsettings.h
    src/core/application_services.cpp
    src/core/application_services.h
    src/core/fontmanager.cpp
    src/core/fontmanager.h
    src/core/resources_paths.h
    src/core/settingsmanager.cpp
    src/core/settingsmanager.h

    src/core/enums/schedule_type.h

    src/models/campus.h
    src/models/class_conflict.h
    src/models/class_info.h
    src/models/classroom.cpp
    src/models/classroom.h
    src/models/intensive_slot_state.h
    src/models/roster.cpp
    src/models/roster.h
    src/models/speaking_evaluation.h
    src/models/teacher.h
    src/models/teacher_model.h
    src/models/teacher_model.cpp

    src/services/dataservice.cpp
    src/services/dataservice.h
    src/services/theme_service.cpp
    src/services/theme_service.h

    src/ui/actions/action_registry.cpp
    src/ui/actions/action_registry.h

    src/ui/components/bottom_bar_builder.cpp
    src/ui/components/bottom_bar_builder.h

    src/ui/constants/gui_constants.h
    src/ui/constants/options.h

    src/ui/menus/menu_utils.h

    src/ui/pages/basepage.cpp
    src/ui/pages/basepage.h
    src/ui/pages/campus/campus_dashboard_page.cpp
    src/ui/pages/campus/campus_dashboard_page.h
    src/ui/pages/class/class_info_page.cpp
    src/ui/pages/class/class_info_page.h
    src/ui/pages/class/class_notes_page.cpp
    src/ui/pages/class/class_notes_page.h
    src/ui/pages/pagemanager.cpp
    src/ui/pages/pagemanager.h
    src/ui/pages/roster/roster_column_layout_controller.cpp
    src/ui/pages/roster/roster_column_layout_controller.h
    src/ui/pages/roster/roster_constants.h
    src/ui/pages/roster/roster_header_view.cpp
    src/ui/pages/roster/roster_header_view.h
    src/ui/pages/roster/roster_item_delegate.cpp
    src/ui/pages/roster/roster_item_delegate.h
    src/ui/pages/roster/roster_model.cpp
    src/ui/pages/roster/roster_model.h
    src/ui/pages/roster/roster_page.cpp
    src/ui/pages/roster/roster_page.h
    src/ui/pages/roster/roster_table_view.cpp
    src/ui/pages/roster/roster_table_view.h
    src/ui/pages/schedule/schedule_builder.cpp
    src/ui/pages/schedule/schedule_builder.h
    src/ui/pages/schedule/schedule_editor_dialog.cpp
    src/ui/pages/schedule/schedule_editor_dialog.h
    src/ui/pages/schedule/schedule_page.cpp
    src/ui/pages/schedule/schedule_page.h
    src/ui/pages/speakingeval/speaking_eval_delegate.cpp
    src/ui/pages/speakingeval/speaking_eval_delegate.h
    src/ui/pages/speakingeval/speaking_eval_model.cpp
    src/ui/pages/speakingeval/speaking_eval_model.h
    src/ui/pages/speakingeval/speaking_eval_page.cpp
    src/ui/pages/speakingeval/speaking_eval_page.h
    src/ui/pages/speakingeval/speaking_eval_table_view.cpp
    src/ui/pages/speakingeval/speaking_eval_table_view.h
    src/ui/pages/teacher/teacher_info_page.cpp
    src/ui/pages/teacher/teacher_info_page.h

    src/ui/state/option_state.h
    src/ui/state/option_state_keys.h

    src/ui/utils/widget_sizing.h

    src/ui/widgets/clickable_color_preview.cpp
    src/ui/widgets/clickable_color_preview.h
    src/ui/widgets/no_wheel_combobox.cpp
    src/ui/widgets/no_wheel_combobox.h

    src/ui/widgets/sectioncards/class_info_section_card.cpp
    src/ui/widgets/sectioncards/class_info_section_card.h
    src/ui/widgets/sectioncards/class_time_row.cpp
    src/ui/widgets/sectioncards/class_time_row.h
    src/ui/widgets/sectioncards/teacher_section_card.cpp
    src/ui/widgets/sectioncards/teacher_section_card.h

    src/ui/widgets/sections/class_details_section.cpp
    src/ui/widgets/sections/class_details_section.h
    src/ui/widgets/sections/class_schedule_section.cpp
    src/ui/widgets/sections/class_schedule_section.h
    src/ui/widgets/sections/teacher_info_section.cpp
    src/ui/widgets/sections/teacher_info_section.h

    src/ui/widgets/sidebar/sidebar.cpp
    src/ui/widgets/sidebar/sidebar.h
    src/ui/widgets/sidebar/sidebar_definitions.cpp
    src/ui/widgets/sidebar/sidebar_definitions.h
    src/ui/widgets/sidebar/sidebar_types.h

    src/ui/widgets/splash/splashscreen.cpp
    src/ui/widgets/splash/splashscreen.h

    src/utils/colorutils.cpp
    src/utils/colorutils.h
    src/utils/platform.cpp
    src/utils/platform.h
    src/utils/sidebar_node_naming.cpp
    src/utils/sidebar_node_naming.h
    src/controllers/sidebar_controller.h
    src/controllers/sidebar_controller.cpp

)

qt_add_resources(ClassMngr app_resources
    PREFIX "/"
    BASE "${PROJECT_SOURCE_DIR}/resources"
    FILES
        resources/assets/fonts/Inter.ttc
        resources/assets/fonts/PretendardVariable.ttf
        resources/assets/icons/app_icon.ico
        resources/assets/icons/icon_256x256.png
        resources/assets/splash/splash.png
)

qt_add_translations(
    TARGETS ClassMngr
    TS_FILES ClassMngr_en_US.ts
)

target_compile_features(ClassMngr
    PRIVATE
        cxx_std_23
)

set_target_properties(ClassMngr PROPERTIES
    CXX_EXTENSIONS OFF
)

target_compile_options(ClassMngr
    PRIVATE
        $<$<CXX_COMPILER_ID:MSVC>:/FS>
)

target_include_directories(ClassMngr
    PRIVATE
        ${PROJECT_SOURCE_DIR}/src
)

target_compile_definitions(ClassMngr
    PRIVATE
        CLASSMNGR_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
)

target_link_libraries(ClassMngr
    PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Network
        Qt6::Sql
        Qt6::Widgets
)

if(WIN32)
    add_custom_command(TARGET ClassMngr POST_BUILD
        COMMAND Qt6::windeployqt
            --dir "$<TARGET_FILE_DIR:ClassMngr>"
            "$<TARGET_FILE:ClassMngr>"
        COMMENT "Deploying Qt runtime dependencies"
    )
endif()

install(TARGETS ClassMngr
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

qt_generate_deploy_app_script(
    TARGET ClassMngr
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})
