cmake_minimum_required(VERSION 3.9)
project(contracts)

set(EOSIO_CDT_INSTALL_DIR /usr/local/eosio.cdt)

include_directories(include)
include_directories(${EOSIO_CDT_INSTALL_DIR}/include)
include_directories(${EOSIO_CDT_INSTALL_DIR}/include/libc)
include_directories(${EOSIO_CDT_INSTALL_DIR}/include/libcxx)

set(COMMON_FILES common/random.hpp common/constants.hpp common/eosio.token.hpp common/utils.hpp
        common/tables.hpp common/contracts.hpp common/cards.hpp common/param_reader.hpp common/round_based_contract.hpp
        common/game_contracts.hpp baccarat/baccarat_common.hpp baccarat/baccarat_common.hpp common/payment_map.hpp)
set(HPP_FILES dice/dice.hpp blackjack/blackjack.hpp baccarat/baccarat.hpp redblack/redblack.hpp house/house.hpp roulette/roulette.hpp
        cbaccarat/cbaccarat.hpp scratch/scratch.hpp slots/slots.hpp bullfight/bullfight.hpp quick3/quick3.hpp event/event.hpp
        centergame/centergame.hpp)
set(CPP_FILES dice/dice.cpp blackjack/blackjack.cpp baccarat/baccarat.cpp redblack/redblack.cpp house/house.cpp roulette/roulette.cpp
        cbaccarat/cbaccarat.cpp scratch/scratch.cpp slots/slots.cpp bullfight/bullfight.cpp quick3/quick3.cpp event/event.cpp
        centergame/centergame.cpp)

add_library(link SHARED
        ${COMMON_FILES}
        ${CPP_FILES}
        ${HPP_FILES})

target_link_libraries(link
        ${EOSIO_CDT_INSTALL_DIR}/lib/libc.bc
        ${EOSIO_CDT_INSTALL_DIR}/lib/libc++.bc
        ${EOSIO_CDT_INSTALL_DIR}/lib/eosiolib.bc)
