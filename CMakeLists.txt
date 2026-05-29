cmake_minimum_required(VERSION 3.19)
project(ClassMngr LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 6.11 REQUIRED COMPONENTS
    Core
    Gui
    Sql
    Widgets
    Network
    LinguistTools
)

qt_standard_project_setup()

qt_add_executable(ClassMngr
    WIN32 MACOSX_BUNDLE

    src/main.cpp

    src/app/mainwindow.cpp
    src/app/mainwindow.h
    src/app/mainwindow.ui

    src/core/settingsmanager.h
    src/core/settingsmanager.cpp

    src/core/appsettings.h

    src/core/resourcepaths.h
    src/core/resourcepaths.cpp

    src/core/fontmanager.h
    src/core/fontmanager.cpp

    src/pages/basepage.h

    src/pages/pagemanager.h
    src/pages/pagemanager.cpp

    src/pages/campus/campus_dashboard_page.h
    src/pages/campus/campus_dashboard_page.cpp

    src/pages/teacher/teacher_info_page.h
    src/pages/teacher/teacher_info_page.cpp

    src/pages/class/class_info_page.h
    src/pages/class/class_info_page.cpp


    src/pages/schedule/schedule_page.h
    src/pages/schedule/schedule_page.cpp

    src/pages/roster/roster_page.h
    src/pages/roster/roster_page.cpp

    src/pages/speakingeval/speaking_eval_page.h
    src/pages/speakingeval/speaking_eval_page.cpp

    src/services/dataservice.h
    src/services/dataservice.cpp

    src/utils/platform.cpp
    src/utils/platform.h

    src/utils/colorutils.cpp
    src/utils/colorutils.h

    src/widgets/splash/splashscreen.cpp
    src/widgets/splash/splashscreen.h

    src/widgets/sidebar/sidebar.h
    src/widgets/sidebar/sidebar.cpp

    src/widgets/sidebar/sidebar_definitions.h
    src/widgets/sidebar/sidebar_definitions.cpp

    src/widgets/sidebar/sidebar_types.h

    resources/resources.qrc
)

target_include_directories(ClassMngr
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
)

qt_add_translations(
    TARGETS ClassMngr
    TS_FILES ClassMngr_en_US.ts
)

target_link_libraries(ClassMngr
    PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Sql
        Qt6::Widgets
)

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