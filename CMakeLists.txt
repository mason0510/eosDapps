cmake_minimum_required(VERSION 3.9)
project(contracts)

set(EOSIO_CDT_INSTALL_DIR /usr/local/eosio.cdt)

include_directories(include)
include_directories(${EOSIO_CDT_INSTALL_DIR}/include)
include_directories(${EOSIO_CDT_INSTALL_DIR}/include/libc)
include_directories(${EOSIO_CDT_INSTALL_DIR}/include/libcxx)

set(COMMON_FILES common/random.hpp common/constants.hpp common/eosio.token.hpp common/utils.hpp
        common/tables.hpp common/contracts.hpp common/cards.hpp common/param_reader.hpp)
set(HPP_FILES dice/dice.hpp blackjack/blackjack.hpp baccarat/baccarat.hpp house/house.hpp)
set(CPP_FILES dice/dice.cpp blackjack/blackjack.cpp baccarat/baccarat.cpp house/house.cpp)

add_library(link SHARED
        ${COMMON_FILES}
        ${CPP_FILES}
        ${HPP_FILES})

target_link_libraries(link
        ${EOSIO_CDT_INSTALL_DIR}/lib/libc.bc
        ${EOSIO_CDT_INSTALL_DIR}/lib/libc++.bc
        ${EOSIO_CDT_INSTALL_DIR}/lib/eosiolib.bc)
