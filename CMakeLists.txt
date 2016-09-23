project(pvr.kartina.tv)

cmake_minimum_required(VERSION 2.6)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${PROJECT_SOURCE_DIR})

enable_language(CXX)

find_package(Kodi REQUIRED)
find_package(kodiplatform REQUIRED)
find_package(p8-platform REQUIRED)

include_directories(${kodiplatform_INCLUDE_DIRS}
                    ${platform_INCLUDE_DIRS}
                    ${KODI_INCLUDE_DIR}
                    ${PROJECT_SOURCE_DIR}/src)

set(DEPLIBS ${kodiplatform_LIBRARIES}
            ${p8-platform_LIBRARIES})

set(KARTINATV_SOURCES
	src/client.cpp
	src/kartinatvclient.cpp
	src/kartinatvclient.h
)

if(MSVC)
	list(APPEND DEPLIBS ws2_32)
endif()

find_package(JsonCpp REQUIRED)
list(APPEND DEPLIBS ${JSONCPP_LIBRARIES})
list(APPEND INCLUDES ${JSONCPP_INCLUDE_DIRS})

include_directories(${INCLUDES})
link_directories(${CMAKE_PREFIX_PATH}/lib)

build_addon(pvr.kartina.tv KARTINATV DEPLIBS)

include(CPack)
