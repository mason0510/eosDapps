#include <eosiolib/eosio.hpp>
#include <eosiolib/asset.hpp>
#include <eosiolib/time.hpp>
#include <eosiolib/singleton.hpp>
#include <eosiolib/transaction.h>
#include <eosiolib/transaction.hpp>

#define card_t uint8_t

#include "../common/constants.hpp"
#include "../common/contracts.hpp"
#include "../common/random.hpp"
#include "../common/round_based_contract.hpp"

namespace godapp {
    using namespace eosio;
    using namespace std;

    CONTRACT redblack: public contract {
    public:
        DEFINE_GLOBAL_TABLE
        DEFINE_GAMES_TABLE(vector<card_t> red_cards; vector<card_t> black_cards;)
        DEFINE_BETS_TABLE
        DEFINE_RESULTS_TABLE
        DEFINE_HISTORY_TABLE
        DEFINE_BET_AMOUNT_TABLE
        DEFINE_RANDOM_KEY_TABLE

        ACTION receipt(uint64_t game_id, capi_checksum256 seed, string red_cards, string blue_cards, string result,
            bool lucky_strike);
        DECLARE_STANDARD_ACTIONS(redblack)
    };

    EOSIO_ABI_EX(redblack, STANDARD_ACTIONS(receipt))
}
