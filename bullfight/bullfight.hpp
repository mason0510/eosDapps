#include <eosiolib/eosio.hpp>
#include <eosiolib/asset.hpp>
#include <eosiolib/time.hpp>
#include <eosiolib/singleton.hpp>
#include <eosiolib/transaction.h>
#include <eosiolib/transaction.hpp>

#include "../common/constants.hpp"
#include "../common/contracts.hpp"
#include "../common/round_based_contract.hpp"
#define card_t uint8_t

namespace godapp {
    using namespace eosio;
    using namespace std;

    CONTRACT bullfight: public contract {
    public:
        DEFINE_GLOBAL_TABLE
        DEFINE_GAMES_TABLE(
            std::vector<card_t> banker_cards;
            std::vector<card_t> player1_cards;
            std::vector<card_t> player2_cards;
            std::vector<card_t> player3_cards;
            std::vector<card_t> player4_cards;
        )
        DEFINE_BETS_TABLE
        DEFINE_RESULTS_TABLE
        DEFINE_HISTORY_TABLE
        DEFINE_BET_AMOUNT_TABLE
        DEFINE_RANDOM_KEY_TABLE

        ACTION receipt(uint64_t game_id, capi_checksum256 seed, string banker_cards, string player1_cards,
            int64_t player1_rate, string player2_cards, int64_t player2_rate, string player3_cards,
            int64_t player3_rate, string player4_cards, int64_t player4_rate);
        DECLARE_STANDARD_ACTIONS(bullfight)
    };

    EOSIO_ABI_EX(bullfight, STANDARD_ACTIONS(receipt))
}


