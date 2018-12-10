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
#include "../common/round_based_contract.hpp"

namespace godapp {
    using namespace eosio;
    using namespace std;

    CONTRACT baccarat: public contract {
        public:
        DEFINE_GLOBAL_TABLE
        DEFINE_GAMES_TABLE(vector<card_t>, player_cards, banker_cards)
        DEFINE_BETS_TABLE
        DEFINE_RESULTS_TABLE

        DEFINE_STANDARD_ACTIONS(baccarat)
    };

    EOSIO_ABI_EX(baccarat, STANDARD_ACTIONS)
}


