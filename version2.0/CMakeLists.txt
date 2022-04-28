cmake_minimum_required(VERSION 3.22.2)
project(src)

set(CMAKE_VERBOSE_MAKEFILE ON)
set(CMAEK_CXX_FLAGS "$ENV{CXXFLAGS} -rdynamic -O0 -ggdb -std=c++14 -Wall -Wno-deprecated -Werror -Wno-unused-function ")

include_directories(.)
include_directories(/usr/local/include)
link_directories(/usr/local/lib)

find_library(PTHREAD pthread)

set(LIB_SRC
    src/wrap.cpp
    src/ThreadPool.cpp
    src/epoll.cpp
    src/http_conn.cpp

   )
add_library(src SHARED ${LIB_SRC})

set(LIBS
    src
    pthread
    )

# add_executable(server src/mult_pro_server.cpp)
# add_dependencies(server src)
# target_link_libraries(server ${LIBS})


# add_executable(client src/client.cpp)
# add_dependencies(client src)
# target_link_libraries(client ${LIBS})

# add_executable(test_epoll_thp test/test_epoll_thp.cpp)
# add_dependencies(test_epoll_thp src)
# target_link_libraries(test_epoll_thp ${LIBS})

# add_executable(test_thread_pool test/test_thread_pool.cpp)
# add_dependencies(test_thread_pool src)
# target_link_libraries(test_thread_pool ${LIBS})

add_executable(main test/main.cpp)
add_dependencies(main src)
target_link_libraries(main ${LIBS})

SET(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)
SET(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/lib)

