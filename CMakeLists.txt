cmake_minimum_required(VERSION 3.4.1)

set(distribution_DIR ${CMAKE_SOURCE_DIR}/../../../../distribution)

set(include_DIR ${CMAKE_SOURCE_DIR}/../../../../gen-libs/src/main/cpp/live)

add_library(lib_liveMedia STATIC IMPORTED)
set_target_properties(lib_liveMedia PROPERTIES IMPORTED_LOCATION
    ${distribution_DIR}/liveMedia/lib/${ANDROID_ABI}/libliveMedia.a)

add_library(lib_groupsock STATIC IMPORTED)
set_target_properties(lib_groupsock PROPERTIES IMPORTED_LOCATION
    ${distribution_DIR}/groupsock/lib/${ANDROID_ABI}/libgroupsock.a)

add_library(lib_BasicUsageEnvironment STATIC IMPORTED)
set_target_properties(lib_BasicUsageEnvironment PROPERTIES IMPORTED_LOCATION
    ${distribution_DIR}/BasicUsageEnvironment/lib/${ANDROID_ABI}/libBasicUsageEnvironment.a)

add_library(lib_UsageEnvironment STATIC IMPORTED)
set_target_properties(lib_UsageEnvironment PROPERTIES IMPORTED_LOCATION
    ${distribution_DIR}/UsageEnvironment/lib/${ANDROID_ABI}/libUsageEnvironment.a)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=gnu++11")

add_library(live555-libs SHARED
            live555-jni.cpp
            NextProxyServerMediaSession.cpp
            QosMeasurementRecord.cpp
            NextProxyMonitor.cpp)

target_include_directories(live555-libs PRIVATE
                           ${CMAKE_SOURCE_DIR}/include
                           ${include_DIR}/liveMedia/include
                           ${include_DIR}/groupsock/include
                           ${include_DIR}/BasicUsageEnvironment/include
                           ${include_DIR}/UsageEnvironment/include)

target_link_libraries(live555-libs
                      lib_liveMedia
                      lib_groupsock
                      lib_BasicUsageEnvironment
                      lib_UsageEnvironment
                      log)