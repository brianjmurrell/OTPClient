cmake_minimum_required(VERSION 3.5)
project(OTPClient VERSION "2.99.1" LANGUAGES "C")

configure_file("src/common/version.h.in" "version.h")
include_directories(${PROJECT_BINARY_DIR})

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

option(BUILD_GUI "Build the GUI" ON)
option(BUILD_CLI "Build the CLI" ON)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_FLAGS "-Wall -Wextra -O2 -Wformat=2 -Wmissing-format-attribute -fstack-protector-strong -Wundef -Wmissing-format-attribute")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fdiagnostics-color=always -Wstrict-prototypes -Wunreachable-code")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wchar-subscripts -Wwrite-strings -Wpointer-arith -Wbad-function-cast -Wcast-align")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Werror=format-security -Werror=implicit-function-declaration -Wno-sign-compare")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=2")
if(CMAKE_COMPILER_IS_GNUCC)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pie -fPIE")
endif()
if(USE_FLATPAK_APP_FOLDER)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DUSE_FLATPAK_APP_FOLDER")
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--no-add-needed -Wl,--as-needed -Wl,--no-undefined")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-z,relro,-z,now")
    set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} -Wl,--no-add-needed -Wl,--as-needed")
    set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} -Wl,-z,relro,-z,now")
endif()

find_package(PkgConfig REQUIRED)
find_package(Gcrypt 1.8.0 REQUIRED)
pkg_check_modules(COTP REQUIRED cotp>=1.2.1)
pkg_check_modules(PNG REQUIRED libpng>=1.2.0)
pkg_check_modules(LIBZIP REQUIRED libzip>=1.5.0)
pkg_check_modules(JANSSON REQUIRED jansson>=2.10.0)
pkg_check_modules(ZBAR REQUIRED zbar>=0.20)
pkg_check_modules(GTK4 REQUIRED gtk4>=4.0)
pkg_check_modules(GLIB2 REQUIRED glib-2.0>=2.58.0)
pkg_check_modules(GIO REQUIRED gio-2.0>=2.58.0)

set(GUI_HEADER_FILES
        src/get-builder.h
        src/app.h src/db.h src/gquarks.h src/get-file-size.h)

set(GUI_SOURCE_FILES
        src/main.c
        src/app.c
        src/get-builder.c src/data-structs.h src/db.c src/gquarks.c src/get-file-size.c)

#set(CLI_HEADER_FILES)

#set(CLI_SOURCE_FILES)

include_directories(${GTK4_INCLUDE_DIRS}
        ${GCRYPT_INCLUDE_DIRS}
        ${COTP_INCLUDE_DIRS}
        ${LIBZIP_INCLUDE_DIRS}
        ${PNG_INCLUDE_DIRS}
        ${JANSSON_INCLUDE_DIRS}
        ${ZBAR_INCLUDE_DIRS})

add_executable(${PROJECT_NAME} ${GUI_SOURCE_FILES} ${GUI_HEADER_FILES})
target_link_libraries(${PROJECT_NAME}
        ${GTK4_LIBRARIES}
        ${GCRYPT_LIBRARIES}
        ${COTP_LIBRARIES}
        ${LIBZIP_LIBRARIES}
        ${PNG_LIBRARIES}
        ${JANSSON_LIBRARIES}
        ${ZBAR_LIBRARIES})

set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME "otpclient")
install(TARGETS ${PROJECT_NAME} DESTINATION bin)

install(FILES data/com.github.paolostivanin.OTPClient.desktop DESTINATION share/applications)
install(FILES data/com.github.paolostivanin.OTPClient.appdata.xml DESTINATION share/metainfo)

install(FILES src/ui/*.ui DESTINATION share/otpclient)

install(FILES man/otpclient.1.gz DESTINATION share/man/man1)
install(FILES man/otpclient-cli.1.gz DESTINATION share/man/man1)

install(FILES data/icons/com.github.paolostivanin.OTPClient.svg DESTINATION share/icons/hicolor/scalable/apps)
install(FILES data/icons/com.github.paolostivanin.OTPClient-symbolic.svg DESTINATION share/icons/hicolor/scalable/apps)
