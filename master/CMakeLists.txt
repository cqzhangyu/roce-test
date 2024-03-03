set(targets master)

foreach(target ${targets})
    add_executable(${target} ${target}.cpp)
    target_link_libraries(${target} common_lib rdma_lib)
endforeach()
