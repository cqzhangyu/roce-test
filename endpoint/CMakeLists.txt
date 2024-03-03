set(targets one_read_endpoint query_device)

foreach(target ${targets})
    add_executable(${target} ${target}.cpp)
    target_link_libraries(${target} common_lib rdma_lib)
endforeach()
