cmake_minimum_required(VERSION 3.21.1)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# ---------- 平台基线 ----------
# Qt 6.8 官方支持 macOS 12+；这个变量必须在 project() 之前设置才会生效
if (APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "12.0" CACHE STRING "Qt 6.8 minimum macOS target")
endif ()

# ---------- 版本号处理 ----------
# APP_VERSION 可能是 "1.0.0-rc.3" 这种带后缀的形式
# project() 只接受纯数字，所以拆成两部分：
#   PROJECT_VERSION  →  只给 project() 用，比如 "1.0.0"
#   APP_VERSION     →  完整字符串，给 AppInfo 用，比如 "1.0.0-rc.3"

set(APP_VERSION "2.0.0" CACHE STRING "Full project version (may include suffix)")

# 拆出数字部分（遇到非数字字符就切掉）
string(REGEX MATCH "^[0-9]+(\\.[0-9]+)*" APP_NUMERIC_VERSION "${APP_VERSION}")

# 兜底：万一切出来是空的
if (NOT APP_NUMERIC_VERSION)
    set(APP_NUMERIC_VERSION "0.0.0")
endif ()

project(CampusNetLogin VERSION ${APP_NUMERIC_VERSION} LANGUAGES CXX)

# 后面 configure_file 时把完整版本也传进去（AppInfo.h.in 里用 @APP_VERSION@）

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# ---------- 查找 Qt 模块 ----------
# 6.8 为最低支持版本（与 CI 的 QT_VERSION 对齐），本地更新版本（如 6.11）仍可用
find_package(Qt6 6.8 REQUIRED COMPONENTS
        Core
        Gui
        Widgets
        Network
        Svg
)

qt_standard_project_setup()

# ---------- 平台相关源码 ----------
# 只有开机自启/标题栏着色/托盘图标这几处需要按平台分文件，
# 其余代码全部走 Qt，不写 #ifdef。
if (WIN32)
    set(PLATFORM_SOURCES src/platform/Platform_win.cpp)
elseif (APPLE)
    set(PLATFORM_SOURCES src/platform/Platform_mac.cpp)
else ()
    set(PLATFORM_SOURCES src/platform/Platform_linux.cpp)
endif ()

# ---------- 可执行目标 ----------
qt_add_executable(CampusNetLogin
        src/main.cpp
        src/net/SrunCrypto.cpp
        src/net/SrunClient.cpp
        src/config/Config.cpp
        src/ui/MainWindow.cpp
        src/ui/TrayController.cpp
        resources/resources.qrc
        src/platform/Platform.cpp
        src/platform/AutoStart.cpp
        ${PLATFORM_SOURCES}
        src/ui/ThemeUtils.h
        src/ui/MainWindowUi.cpp
        src/ui/MainWindowLog.cpp
        src/ui/MainWindowConfig.cpp
        src/ui/MainWindowAuth.cpp
        src/ui/MainWindowInfo.cpp
        src/net/SrunHttpUtils.cpp
        src/ui/MainWindowVersion.cpp
        src/util/SingleInstance.cpp
        src/ui/MainWindowTheme.cpp
)

set_target_properties(CampusNetLogin PROPERTIES
        WIN32_EXECUTABLE $<NOT:$<CONFIG:Debug>>
)

# ---------- macOS 应用包 ----------
# macOS 签名身份：默认 "-" = ad-hoc 签名（无需任何证书，本机运行必需）。
# 拿到 Developer ID 后改成 "Developer ID Application: 名字 (TEAMID)"，
# 脚本会自动加上硬化运行时与时间戳，为公证做准备。
set(MACOS_SIGN_IDENTITY "-" CACHE STRING
        "macOS codesign identity: '-' = ad-hoc, or 'Developer ID Application: ...'")

if (APPLE)
    set_target_properties(CampusNetLogin PROPERTIES
            MACOSX_BUNDLE TRUE
            MACOSX_BUNDLE_INFO_PLIST "${CMAKE_BINARY_DIR}/generated/Info.plist"
            MACOSX_BUNDLE_BUNDLE_NAME "CampusNetLogin"
            MACOSX_BUNDLE_GUI_IDENTIFIER "com.hz6826.CampusNetLogin"
            MACOSX_BUNDLE_ICON_FILE "CampusNetLogin.icns"
            MACOSX_BUNDLE_BUNDLE_VERSION "${APP_NUMERIC_VERSION}"
            MACOSX_BUNDLE_SHORT_VERSION_STRING "${APP_VERSION}"
    )

    # Info.plist 模板里的 @APP_VERSION@ 等变量在配置期展开
    configure_file(
            "${CMAKE_SOURCE_DIR}/packaging/macos/Info.plist.in"
            "${CMAKE_BINARY_DIR}/generated/Info.plist"
            @ONLY
    )

    # MACOSX_BUNDLE_ICON_FILE 只是文件名，真正的 .icns 要作为 bundle 资源加进 target
    set_source_files_properties("packaging/macos/CampusNetLogin.icns"
            PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
    target_sources(CampusNetLogin PRIVATE packaging/macos/CampusNetLogin.icns)
endif ()

# ---------- 生成 AppInfo.h（含 Git 信息） ----------
set(GIT_VERSION_FILE  ${CMAKE_BINARY_DIR}/generated/git_version.cmake)
set(APPINFO_GENERATED ${CMAKE_BINARY_DIR}/generated/AppInfo.h)

# 每次都跑一次 git 检查，但只有内容变化时才会写文件
add_custom_target(git_version ALL
        COMMAND ${CMAKE_COMMAND}
        -D SRC=${CMAKE_SOURCE_DIR}
        -D OUT=${GIT_VERSION_FILE}
        -P ${CMAKE_SOURCE_DIR}/cmake/GetGitVersion.cmake
        BYPRODUCTS ${GIT_VERSION_FILE}
        COMMENT "Checking git version"
        VERBATIM
)

# AppInfo.h 依赖 git_version.cmake（文件依赖，不是 target 依赖）
# → git_version.cmake 没变时，AppInfo.h 也不会重新生成
add_custom_command(
        OUTPUT ${APPINFO_GENERATED}
        COMMAND ${CMAKE_COMMAND}
        -D SRC=${CMAKE_SOURCE_DIR}
        -D OUT=${APPINFO_GENERATED}
        -D VERSION=${APP_VERSION}
        -D NAME=${PROJECT_NAME}
        -D GIT_VERSION_FILE=${GIT_VERSION_FILE}
        -D BUILD_TYPE=${CMAKE_BUILD_TYPE}
        -P ${CMAKE_SOURCE_DIR}/cmake/GenerateAppInfo.cmake
        DEPENDS
        ${CMAKE_SOURCE_DIR}/src/AppInfo.h.in
        ${CMAKE_SOURCE_DIR}/cmake/GenerateAppInfo.cmake
        ${GIT_VERSION_FILE}
        COMMENT "Generating AppInfo.h"
        VERBATIM
)

add_custom_target(appinfo DEPENDS ${APPINFO_GENERATED})
add_dependencies(CampusNetLogin appinfo)

if (WIN32)
    # ---------- 生成 Windows 版本资源（VERSIONINFO） ----------
    # FILEVERSION/PRODUCTVERSION 必须是 4 段逗号分隔数字，
    # 所以把 APP_NUMERIC_VERSION（如 "1.0.0"）补齐/截断成 4 段。
    file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/generated")

    string(REPLACE "." ";" _ver_parts "${APP_NUMERIC_VERSION}")
    list(APPEND _ver_parts "0" "0" "0" "0")
    list(GET _ver_parts 0 _ver_major)
    list(GET _ver_parts 1 _ver_minor)
    list(GET _ver_parts 2 _ver_patch)
    list(GET _ver_parts 3 _ver_tweak)
    set(APP_RC_FILEVERSION
            "${_ver_major},${_ver_minor},${_ver_patch},${_ver_tweak}")

    # 配置期生成：版本号变化时 configure_file 会更新，windres 随之重编
    set(VERSION_RC_FILE "${CMAKE_BINARY_DIR}/generated/version.rc")
    configure_file(
            "${CMAKE_SOURCE_DIR}/cmake/Version.rc.in"
            "${VERSION_RC_FILE}"
            @ONLY
    )
    set_source_files_properties("${VERSION_RC_FILE}" PROPERTIES GENERATED TRUE)

    target_sources(CampusNetLogin PRIVATE
            resources/windows/app.rc
            "${VERSION_RC_FILE}"
    )
endif()

# 让源文件可以用 "net/SrunClient.h" 这种相对路径 include
target_include_directories(CampusNetLogin PRIVATE
        src
        ${CMAKE_BINARY_DIR}/generated
)

target_link_libraries(CampusNetLogin PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        Qt6::Network
        Qt6::Svg
        # 自定义标题栏颜色（DWM）
        $<$<PLATFORM_ID:Windows>:dwmapi>
)

# ---------- Windows 开发时拷贝 Qt 运行时 DLL ----------
if (WIN32)
    find_program(WINDEPLOYQT_EXE windeployqt
            HINTS
            "$ENV{QT_ROOT_DIR}/bin"       # install-qt-action
            "${QT_ROOT_DIR}/bin"          # 备用
            "${QT_INSTALL_PATH}/bin"      # 本地你原来用的
            # 不写 PATHS 时默认搜 PATH，CI 里能直接命中
    )

    if (WINDEPLOYQT_EXE)
        add_custom_command(TARGET CampusNetLogin POST_BUILD
                COMMAND "${WINDEPLOYQT_EXE}"
                --no-translations
                --no-system-d3d-compiler
                --no-opengl-sw
                --compiler-runtime
                "$<TARGET_FILE:CampusNetLogin>"
                COMMENT "Running windeployqt to copy Qt runtime")
    else ()
        message(WARNING "windeployqt not found, runtime DLLs may not be copied")
    endif ()
endif ()

# ---------- 安装规则 ----------
# macOS/Linux 的打包工具（macdeployqt / linuxdeploy / AppImage）都要求先有 install()；
# Windows 的发布链路仍走 dist + Inno Setup，不受影响。
include(GNUInstallDirs)

install(TARGETS CampusNetLogin
        BUNDLE DESTINATION .
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if (APPLE)
    # 主题是外置文件，DMG 里没有安装器 → 放进 .app 资源目录，首次启动时播种到用户目录
    install(DIRECTORY themes/ DESTINATION CampusNetLogin.app/Contents/Resources/themes)
    # LGPLv3 合规：Qt 许可证与 relink 说明随包发布
    install(DIRECTORY packaging/LICENSES/ DESTINATION CampusNetLogin.app/Contents/Resources/LICENSES)
endif ()

# 部署 Qt 运行时（macOS: macdeployqt；Linux: 交由 linuxdeploy/AppImage 处理）
if (NOT WIN32)
    qt_generate_deploy_app_script(
            TARGET CampusNetLogin
            OUTPUT_SCRIPT qt_deploy_script
            NO_UNSUPPORTED_PLATFORM_ERROR)
    install(SCRIPT ${qt_deploy_script})
endif ()

if (APPLE)
    # macdeployqt 移动/改写 framework 后原签名失效，Apple Silicon 上会直接拒跑
    # （双击提示「已损坏」）。这里由内到外重新签名；默认 ad-hoc。
    set(_codesign_flags --force)
    if (NOT MACOS_SIGN_IDENTITY STREQUAL "-")
        # 硬化运行时 + 时间戳是公证的前置条件，ad-hoc 签名拿不到时间戳
        set(_codesign_flags ${_codesign_flags} --options runtime --timestamp)
    endif ()

    install(CODE "
        set(_app \"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/CampusNetLogin.app\")
        if (NOT EXISTS \"\${_app}\")
            message(FATAL_ERROR \"找不到应用包：\${_app}\")
        endif ()

        # 安装过程可能带上扩展属性，先清掉再签名
        execute_process(COMMAND xattr -cr \"\${_app}\")

        file(GLOB _app_parts
                \"\${_app}/Contents/Frameworks/*.framework\"
                \"\${_app}/Contents/Frameworks/*.dylib\"
                \"\${_app}/Contents/PlugIns/*.dylib\")
        foreach (_part IN LISTS _app_parts)
            execute_process(
                    COMMAND codesign ${_codesign_flags} --sign \"${MACOS_SIGN_IDENTITY}\" \"\${_part}\"
                    RESULT_VARIABLE _rc)
            if (NOT _rc EQUAL 0)
                message(FATAL_ERROR \"codesign 失败：\${_part}\")
            endif ()
        endforeach ()

        message(STATUS \"签名 \${_app}（identity: ${MACOS_SIGN_IDENTITY}）\")
        execute_process(
                COMMAND codesign ${_codesign_flags} --sign \"${MACOS_SIGN_IDENTITY}\" \"\${_app}\"
                RESULT_VARIABLE _rc)
        if (NOT _rc EQUAL 0)
            message(FATAL_ERROR \"codesign 失败：\${_app}\")
        endif ()
    ")
endif ()
