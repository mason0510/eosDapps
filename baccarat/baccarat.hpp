#include <eosiolib/eosio.hpp>
#include <eosiolib/asset.hpp>
#include <eosiolib/time.hpp>
#include <eosiolib/singleton.hpp>
#include <eosiolib/transaction.h>
#include <eosiolib/transaction.hpp>

#define card_t uint16_t

#include "../common/constants.hpp"
#include "../common/contracts.hpp"
#include "../common/random.hpp"

namespace godapp {
    using namespace eosio;
    using namespace std;

    CONTRACT baccarat: public contract {
        public:

        DEFINE_GLOBAL_TABLE

        TABLE game {
            uint64_t id;
            uint64_t end_time;

            vector<card_t> player_cards;
            vector<card_t> banker_cards;

            symbol symbol;
            uint8_t status;

            uint64_t primary_key()const { return symbol.raw(); }
            uint64_t byid()const {return id;}
        };
        typedef multi_index<name("games"), game,
                indexed_by< name("byid"), const_mem_fun<game, uint64_t, &game::byid> >
        > games_table;
        games_table _games;

        TABLE bet {
            uint64_t id;
            uint64_t game_id;
            name player;
            name referer;
            asset bet;
            uint8_t bet_type;

            uint64_t primary_key()const { return id; }
            uint64_t bygameid()const {return game_id;}
        };
        typedef multi_index<name("bets"), bet,
            indexed_by< name("bygameid"), const_mem_fun<bet, uint64_t, &bet::bygameid> >
        > bet_table;
        bet_table _bets;

        baccarat(name receiver, name code, datastream<const char*> ds);

        ACTION init();
        ACTION setglobal(uint64_t key, uint64_t value);
        ACTION play(name player, asset bet, string memo);
        ACTION reveal(uint64_t game_id);
        ACTION hardclose(uint64_t game_id);

    private:
        void bet(name player, name referer, uint64_t game_id, uint8_t bet_type, asset amount);
        void init_game(symbol sym);
    };

    EOSIO_ABI_EX(baccarat, (init)(reveal)(play)(setglobal)(hardclose))
}


