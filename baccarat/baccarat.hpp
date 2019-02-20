#include <eosiolib/eosio.hpp>
#include <eosiolib/asset.hpp>
#include <eosiolib/time.hpp>
#include <eosiolib/singleton.hpp>
#include <eosiolib/transaction.h>
#include <eosiolib/transaction.hpp>

#include "../common/constants.hpp"
#include "../common/contracts.hpp"
#include "../common/round_based_contract.hpp"
#include "../baccarat/baccarat_common.hpp"

namespace godapp {
    using namespace eosio;
    using namespace std;

    CONTRACT baccarat: public contract {
        public:
        DEFINE_GLOBAL_TABLE
        DEFINE_GAMES_TABLE(vector<card_t> player_cards; vector<card_t> banker_cards;)
        DEFINE_BETS_TABLE
        DEFINE_RESULTS_TABLE
        DEFINE_HISTORY_TABLE
        DEFINE_BET_AMOUNT_TABLE

        ACTION receipt(uint64_t game_id, string player_cards, uint8_t player_point, string banker_cards, uint8_t banker_point, string result);
        DECLARE_STANDARD_ACTIONS(baccarat)
    };

    EOSIO_ABI_EX(baccarat, STANDARD_ACTIONS(receipt))
}


