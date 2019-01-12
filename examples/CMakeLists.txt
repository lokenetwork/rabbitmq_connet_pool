if (NOT BUILD_SHARED_LIBS)
  add_definitions(-DAMQP_STATIC)
endif()

include_directories(${LIBRABBITMQ_INCLUDE_DIRS})

if (WIN32)
    set(PLATFORM_DIR win32)
else (WIN32)
    set(PLATFORM_DIR unix)
endif (WIN32)

set(COMMON_SRCS
    utils.h
    utils.c
    ${PLATFORM_DIR}/platform_utils.c
    )


add_executable(amqp_con_pool amqp_connect_pool.c socket.c list.c confread.c readn.c daemonize.c single.c lockfile.c ${COMMON_SRCS})
target_link_libraries(amqp_con_pool ${RMQ_LIBRARY_TARGET} m)


add_executable(amqp_consumer amqp_consumer.c confread.c readn.c daemonize.c single.c lockfile.c ${COMMON_SRCS})
target_link_libraries(amqp_consumer ${RMQ_LIBRARY_TARGET} curl)

configure_file(config.ini ${CMAKE_BINARY_DIR}/config.ini)