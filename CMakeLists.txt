cmake_minimum_required(VERSION 3.0.0)
project(net VERSION 0.1.0)

set(CPM_DOWNLOAD_VERSION 0.36.0)

if (CPM_SOURCE_CACHE)
    set(CPM_DOWNLOAD_LOCATION "${CPM_SOURCE_CACHE}/cpm/CPM_${CPM_DOWNLOAD_VERSION}.cmake")
elseif (DEFINED ENV{CPM_SOURCE_CACHE})
    set(CPM_DOWNLOAD_LOCATION "$ENV{CPM_SOURCE_CACHE}/cpm/CPM_${CPM_DOWNLOAD_VERSION}.cmake")
else ()
    set(CPM_DOWNLOAD_LOCATION "${CMAKE_BINARY_DIR}/cmake/CPM_${CPM_DOWNLOAD_VERSION}.cmake")
endif ()

# Expand relative path. This is important if the provided path contains a tilde (~)
get_filename_component(CPM_DOWNLOAD_LOCATION ${CPM_DOWNLOAD_LOCATION} ABSOLUTE)
if (NOT (EXISTS ${CPM_DOWNLOAD_LOCATION}))
    message(STATUS "Downloading CPM.cmake to ${CPM_DOWNLOAD_LOCATION}")
    file(DOWNLOAD
            https://github.com/cpm-cmake/CPM.cmake/releases/download/v${CPM_DOWNLOAD_VERSION}/CPM.cmake
            ${CPM_DOWNLOAD_LOCATION}
            )
endif ()

include(${CPM_DOWNLOAD_LOCATION})

CPMAddPackage(
        NAME debug_macros
        VERSION 1.05
        GITHUB_REPOSITORY chiro2001/debug-macros)

link_libraries(debug_macros)

#Add extra source files here if any
set(EXTRA_FILE 
    
)

# set(CMAKE_C_FLAGS "-std=c99")

if(WIN32)
    set(PCAP wpcap)
else()
    set(PCAP pcap)
endif()

add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/source-charset:utf-8>")

set (CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fno-omit-frame-pointer -fsanitize=address")
set (CMAKE_LINKER_FLAGS_DEBUG "${CMAKE_LINKER_FLAGS_DEBUG} -fno-omit-frame-pointer -fsanitize=address")

add_compile_options(-Wall -g)
#set(EXECUTABLE_OUTPUT_PATH ${CMAKE_CURRENT_SOURCE_DIR}/test) 
include_directories(./include ./Npcap/Include)
link_directories(./Npcap/Lib ./Npcap/Lib/x64)
aux_source_directory(./src DIR_SRCS)

add_executable(main ${DIR_SRCS})
target_link_libraries(main ${PCAP})

# add_definitions(-DCONFIG_DEBUG=1)

set(TEST_FIX_SOURCE 
    testing/faker/driver.c 
    testing/global.c
    src/net.c
    src/buf.c
    src/map.c
    src/queue.c
    src/utils.c
    testing/faker/tcp.c
)

# aux_source_directory(./testing DIR_TEST)
add_executable(eth_in 
    testing/eth_in.c
    src/ethernet.c
    testing/faker/arp.c
    testing/faker/ip.c
    testing/faker/icmp.c
    testing/faker/udp.c
    ${TEST_FIX_SOURCE}
    ${EXTRA_FILE}
)
target_link_libraries(eth_in ${PCAP})
target_compile_definitions(eth_in PUBLIC TEST)

add_executable(eth_out
    testing/eth_out.c
    src/ethernet.c
    testing/faker/arp.c
    testing/faker/ip.c
    testing/faker/icmp.c
    testing/faker/udp.c
    ${TEST_FIX_SOURCE}
    ${EXTRA_FILE}
)
target_link_libraries(eth_out ${PCAP})
target_compile_definitions(eth_out PUBLIC TEST)

add_executable(arp_test
    testing/arp_test.c
    src/ethernet.c
    src/arp.c
    testing/faker/ip.c
    testing/faker/icmp.c
    testing/faker/udp.c
    ${TEST_FIX_SOURCE}
    ${EXTRA_FILE}
)
target_link_libraries(arp_test ${PCAP})
target_compile_definitions(arp_test PUBLIC TEST)

add_executable(ip_test
    testing/ip_test.c
    src/ethernet.c
    src/arp.c
    src/ip.c
    testing/faker/icmp.c
    testing/faker/udp.c
    ${TEST_FIX_SOURCE}
    ${EXTRA_FILE}
)
target_link_libraries(ip_test ${PCAP})
target_compile_definitions(ip_test PUBLIC TEST)

add_executable(ip_frag_test
    testing/ip_frag_test.c
    testing/faker/arp.c
    src/ethernet.c
    src/ip.c
    testing/faker/icmp.c
    testing/faker/udp.c
    ${TEST_FIX_SOURCE}
    ${EXTRA_FILE}
)
target_link_libraries(ip_frag_test ${PCAP})
target_compile_definitions(ip_frag_test PUBLIC TEST)

add_executable(icmp_test
    testing/icmp_test.c
    src/ethernet.c
    src/arp.c
    src/ip.c
    src/icmp.c
    testing/faker/udp.c
    ${TEST_FIX_SOURCE}
    ${EXTRA_FILE}
)
target_link_libraries(icmp_test ${PCAP})
target_compile_definitions(icmp_test PUBLIC TEST)

add_executable(queue_test
        testing/queue_test.c
        src/udp.c
        src/net.c
        src/ethernet.c
        src/arp.c
        src/ip.c
        src/icmp.c
        ${TEST_FIX_SOURCE}
        ${EXTRA_FILE})
target_link_libraries(queue_test ${PCAP})
target_compile_definitions(queue_test PUBLIC TEST)

enable_testing()

add_test(
    NAME eth_in 
    COMMAND $<TARGET_FILE:eth_in> ${CMAKE_CURRENT_LIST_DIR}/testing/data/eth_in
)

add_test(
    NAME eth_out
    COMMAND $<TARGET_FILE:eth_out> ${CMAKE_CURRENT_LIST_DIR}/testing/data/eth_out
)

add_test(
    NAME arp_test
    COMMAND $<TARGET_FILE:arp_test> ${CMAKE_CURRENT_LIST_DIR}/testing/data/arp_test
)

add_test(
    NAME ip_test
    COMMAND $<TARGET_FILE:ip_test> ${CMAKE_CURRENT_LIST_DIR}/testing/data/ip_test
)

add_test(
    NAME ip_frag_test
    COMMAND $<TARGET_FILE:ip_frag_test> ${CMAKE_CURRENT_LIST_DIR}/testing/data/ip_frag_test
)

add_test(
    NAME icmp_test
    COMMAND $<TARGET_FILE:icmp_test> ${CMAKE_CURRENT_LIST_DIR}/testing/data/icmp_test
)

add_test(NAME queue_test COMMAND $<TARGET_FILE:queue_test>)

if(WIN32)
    add_test(
        NAME main_test
        COMMAND $<TARGET_FILE:main>
    )
else()
    add_test(
        NAME main_test
        COMMAND sudo $<TARGET_FILE:main>
    )
endif()

message("Executable files is in ${EXECUTABLE_OUTPUT_PATH}.")

