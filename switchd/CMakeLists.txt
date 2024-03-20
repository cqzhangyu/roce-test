project(switchd)

set(CMAKE_CXX_COMPILER "g++")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unused-but-set-variable")
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED True)

set(TOFINO_INSTALL $ENV{SDE_INSTALL})

set(TOFINO_LIBS
    # bf_switch
    bfsys
    bfutils
    thrift
    # tofinopdfixed_thrift
    clish
    grpc
    avago
    driver
    piall
    piprotogrpc
    piprotobuf
    pifeproto
    pltfm_mgr
    pltfm_mgr_thrift
    pthread
    )

include_directories(
    ${TOFINO_INSTALL}/include
    ../common
    src)

link_directories(${TOFINO_INSTALL}/lib)

aux_source_directory(../common COMMON_LIST)
add_executable(shuffle_master shuffle_master.cpp ${COMMON_LIST})

set(SwitchdTargets
    shuffle_master)

foreach(_target ${SwitchdTargets})
    target_link_libraries(${_target} PUBLIC
        ${TOFINO_LIBS})
endforeach()
