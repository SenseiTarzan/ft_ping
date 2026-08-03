cmake_minimum_required(VERSION 3.0)
project(ft_ping C)

set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -g3 -O0")

set(CMAKE_C_STANDARD 99)

add_executable(ft_ping main.c
        src/binary_stream/binary_stream.c
        src/binary_stream/binary_stream.h
        src/icmp/binary_stream_icmp.c
        src/icmp/binary_stream_icmp.h
        src/packet_processors/packet_processors.c
        src/packet_processors/packet_processors.h
        src/icmp/packet_processors_icmp.c
        src/icmp/packet_processors_icmp.h
        src/icmp/packet/echo_request_packet.c
        src/icmp/packet/echo_request_packet.h
        src/icmp/packet/echo_reply_packet.c
        src/icmp/packet/echo_reply_packet.h)

add_executable(test_binary_stream EXCLUDE_FROM_ALL
        tests/test_binary_stream.c
        src/binary_stream/binary_stream.c
        src/binary_stream/binary_stream.h)
target_compile_options(test_binary_stream PRIVATE -Wall -Wextra)

# Variante instrumentee : detecte les debordements, fuites et UB
add_executable(test_binary_stream_san EXCLUDE_FROM_ALL
        tests/test_binary_stream.c
        src/binary_stream/binary_stream.c
        src/binary_stream/binary_stream.h)
target_compile_options(test_binary_stream_san PRIVATE
        -Wall -Wextra -fsanitize=address,undefined -fno-omit-frame-pointer)
target_link_options(test_binary_stream_san PRIVATE -fsanitize=address,undefined)

add_executable(test_packet_processors EXCLUDE_FROM_ALL
        tests/test_packet_processors.c
        src/packet_processors/packet_processors.c
        src/packet_processors/packet_processors.h
        src/binary_stream/binary_stream.c
        src/binary_stream/binary_stream.h)
target_compile_options(test_packet_processors PRIVATE -Wall -Wextra)

add_executable(test_packet_processors_san EXCLUDE_FROM_ALL
        tests/test_packet_processors.c
        src/packet_processors/packet_processors.c
        src/packet_processors/packet_processors.h
        src/binary_stream/binary_stream.c
        src/binary_stream/binary_stream.h)
target_compile_options(test_packet_processors_san PRIVATE
        -Wall -Wextra -fsanitize=address,undefined -fno-omit-frame-pointer)
target_link_options(test_packet_processors_san PRIVATE -fsanitize=address,undefined)

add_custom_target(tests DEPENDS test_binary_stream test_packet_processors)
add_custom_target(tests_san DEPENDS test_binary_stream_san test_packet_processors_san)

# ------------------------------------------------------------------
# Valgrind
# ------------------------------------------------------------------
# Les cibles _san sont compilees avec ASan : valgrind et ASan ne peuvent pas
# instrumenter le meme processus, donc memcheck tourne sur les cibles nues.
find_program(VALGRIND_COMMAND NAMES valgrind)

set(VALGRIND_OPTIONS
        --tool=memcheck
        --leak-check=full
        --show-leak-kinds=all
        --track-origins=yes
        --errors-for-leak-kinds=definite,indirect
        --error-exitcode=42
        --suppressions=${CMAKE_CURRENT_SOURCE_DIR}/valgrind.supp)

# CTest : rend « ctest -T memcheck » utilisable, et alimente le runner CTest
# de CLion. A positionner avant include(CTest).
set(MEMORYCHECK_COMMAND "${VALGRIND_COMMAND}")
set(MEMORYCHECK_COMMAND_OPTIONS "--leak-check=full --show-leak-kinds=all --track-origins=yes --errors-for-leak-kinds=definite,indirect")
set(MEMORYCHECK_SUPPRESSIONS_FILE "${CMAKE_CURRENT_SOURCE_DIR}/valgrind.supp")

include(CTest)
add_test(NAME binary_stream COMMAND test_binary_stream)
add_test(NAME packet_processors COMMAND test_packet_processors)

if (VALGRIND_COMMAND)
    # Une cible par binaire : elles apparaissent telles quelles dans le
    # selecteur de configuration de CLion.
    add_custom_target(valgrind_ft_ping
            COMMAND ${VALGRIND_COMMAND} ${VALGRIND_OPTIONS} $<TARGET_FILE:ft_ping>
            USES_TERMINAL
            COMMENT "memcheck : ft_ping")
    add_dependencies(valgrind_ft_ping ft_ping)

    add_custom_target(valgrind_test_binary_stream
            COMMAND ${VALGRIND_COMMAND} ${VALGRIND_OPTIONS} $<TARGET_FILE:test_binary_stream>
            USES_TERMINAL
            COMMENT "memcheck : test_binary_stream")
    add_dependencies(valgrind_test_binary_stream test_binary_stream)

    add_custom_target(valgrind_test_packet_processors
            COMMAND ${VALGRIND_COMMAND} ${VALGRIND_OPTIONS} $<TARGET_FILE:test_packet_processors>
            USES_TERMINAL
            COMMENT "memcheck : test_packet_processors")
    add_dependencies(valgrind_test_packet_processors test_packet_processors)

    add_custom_target(valgrind)
    add_dependencies(valgrind
            valgrind_test_binary_stream
            valgrind_test_packet_processors
            valgrind_ft_ping)
else ()
    message(WARNING "valgrind introuvable : les cibles valgrind_* ne sont pas generees")
endif ()

# ------------------------------------------------------------------
# Funcheck (https://github.com/froz42/funcheck)
# ------------------------------------------------------------------
find_program(FUNCHECK_COMMAND NAMES funcheck HINTS "$ENV{HOME}/.local/funcheck/host")

set(FUNCHECK_OPTIONS
        --show-current-test
        --track-allocations)

if (FUNCHECK_COMMAND)
    add_custom_target(funcheck_ft_ping
            COMMAND ${FUNCHECK_COMMAND} ${FUNCHECK_OPTIONS} $<TARGET_FILE:ft_ping>
            USES_TERMINAL
            COMMENT "funcheck : ft_ping")
    add_dependencies(funcheck_ft_ping ft_ping)

    add_custom_target(funcheck_test_binary_stream
            COMMAND ${FUNCHECK_COMMAND} ${FUNCHECK_OPTIONS} $<TARGET_FILE:test_binary_stream>
            USES_TERMINAL
            COMMENT "funcheck : test_binary_stream")
    add_dependencies(funcheck_test_binary_stream test_binary_stream)

    add_custom_target(funcheck_test_packet_processors
            COMMAND ${FUNCHECK_COMMAND} ${FUNCHECK_OPTIONS} $<TARGET_FILE:test_packet_processors>
            USES_TERMINAL
            COMMENT "funcheck : test_packet_processors")
    add_dependencies(funcheck_test_packet_processors test_packet_processors)

    add_custom_target(funcheck)
    add_dependencies(funcheck
            funcheck_test_binary_stream
            funcheck_test_packet_processors
            funcheck_ft_ping)
else ()
    message(WARNING "funcheck introuvable : les cibles funcheck_* ne sont pas generees")
endif ()

