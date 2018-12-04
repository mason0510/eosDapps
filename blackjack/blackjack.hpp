#include <eosiolib/eosio.hpp>
#include <eosiolib/asset.hpp>
#include <eosiolib/time.hpp>
#include <eosiolib/singleton.hpp>
#include <eosiolib/transaction.h>
#include <eosiolib/transaction.hpp>

#include "../common/constants.hpp"
#include "../common/contracts.hpp"
#include "../common/random.hpp"

namespace godapp {
    using namespace eosio;
    using namespace std;

    CONTRACT blackjack: public contract {
        public:

        DEFINE_GLOBAL_TABLE

        TABLE history_item {
            uint64_t id;
            uint64_t close_time;

            vector<uint8_t> player_cards;
            vector<uint8_t> banker_cards;

            name player;
            bool insured;
            uint8_t result;

            asset bet;
            asset payout;

            uint64_t primary_key()const { return id; }
        };
        typedef multi_index<name("results"), history_item> history_table;
        history_table _results;

        TABLE game_item {
            uint64_t id;
            uint64_t start_time;
            uint64_t close_time;

            vector<uint8_t> player_cards;
            vector<uint8_t> banker_cards;

            name player;
            name referer;

            asset bet;
            bool insured;

            uint8_t status;
            uint8_t result;

            uint64_t primary_key()const { return player.value; }
            uint64_t byid()const {return id;}
            uint64_t bystatus()const {return status;}
            uint64_t bytime()const {return start_time;}
        };
        typedef multi_index<name("games"), game_item,
            indexed_by< name("byid"), const_mem_fun<game_item, uint64_t, &game_item::byid> >,
            indexed_by< name("bystatus"), const_mem_fun<game_item, uint64_t, &game_item::bystatus> >,
            indexed_by< name("bytime"), const_mem_fun<game_item, uint64_t, &game_item::bytime> >
        > games_table;
        games_table _games;

        blackjack(name receiver, name code, datastream<const char*> ds);

        ACTION init();
        ACTION play(name player, asset bet, string memo);
        ACTION setglobal(uint64_t key, uint64_t value);
        ACTION deal(uint64_t game_id, uint8_t action);
        ACTION playeraction(name player, uint8_t action);
        ACTION hardclose(uint64_t game_id, string reason);
        ACTION cleargames(uint32_t num);
        ACTION receipt(game_item gm, string banker_cards, string player_cards, string memo);
        ACTION close(uint64_t game_id);

    private:
        uint8_t random_card(random& random_gen, const game_item& gm);
        asset asset_from_vec(const vector<asset>& vec, symbol sym) const;
        void new_game(name player, asset& bet, name referer);
    };

    EOSIO_ABI_EX(blackjack, (init)(deal)(playeraction)(close)(hardclose)(cleargames)(receipt)(play)(setglobal))
}


