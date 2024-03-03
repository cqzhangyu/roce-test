aux_source_directory(. RDMA_SRC)

add_library(rdma_lib INTERFACE ${RDMA_SRC})

target_link_libraries(rdma_lib 
                    INTERFACE ibverbs 
                    INTERFACE rdmacm
                    INTERFACE pthread)
