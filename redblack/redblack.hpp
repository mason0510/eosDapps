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
        DEFINE_GAMES_TABLE(vector<card_t>, red_cards, black_cards)
        DEFINE_BETS_TABLE
        DEFINE_RESULTS_TABLE

        ACTION receipt(uint64_t game_id, string red_cards, string blue_cards, string result, bool lucky_strike);
        DEFINE_STANDARD_ACTIONS(redblack)
    };

    EOSIO_ABI_EX(redblack, STANDARD_ACTIONS(receipt))
}
