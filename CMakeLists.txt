cmake_minimum_required(VERSION 3.3)
project(pf_diverters)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DPRIVATE -Wno-format-security ")

set(COMMON daemon.c stdpf.c)
set(SOURCE_FILES1 ${COMMON} dnsbl/dnsbl-divert.c)
set(SOURCE_FILES2 ${COMMON} bofh/bofh-divert.c )
set(SOURCE_FILES3 ${COMMON} heartbleed/heartbleed-divert.c)

add_executable(dnsbl-divert ${SOURCE_FILES1})
add_executable(bofh-divert ${SOURCE_FILES2})
add_executable(heartbleed-divert ${SOURCE_FILES3})

include_directories(. ./include)

target_link_libraries(dnsbl-divert resolv)
