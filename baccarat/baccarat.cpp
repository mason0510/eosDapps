#include "../baccarat/baccarat.hpp"
#include "../common/cards.hpp"
#include "../common/tables.hpp"
#include "../common/param_reader.hpp"
#include "../common/eosio.token.hpp"
#include "../house/house.hpp"

#define G_ID_START                  101

#define G_ID_GAME_ID                102
#define G_ID_BET_ID                 103
#define G_ID_END                    103

#define GAME_LENGTH                 30
#define GAME_RESOLVE_TIME           5

#define NUM_CARDS                   52 * 8

#define RATE_PLAYER_WIN             2
#define RATE_BANKER_WIN             1.95
#define RATE_PAIR                   12
#define RATE_TIE                    9

#define BET_BANKER_WIN              1
#define BET_PLAYER_WIN              2
#define BET_TIE                     4
#define BET_BANKER_PAIR             8
#define BET_PLAYER_PAIR             16

namespace godapp {
    baccarat::baccarat(name receiver, name code, datastream<const char*> ds):
            contract(receiver, code, ds),
            _globals(_self, _self.value),
            _games(_self, _self.value),
            _bets(_self, _self.value){
    }

    DEFINE_STANDARD_FUNCTIONS(baccarat)

	double payout_rate(uint8_t bet_type) {
	    switch (bet_type) {
            case BET_BANKER_WIN:
                return RATE_BANKER_WIN;
	        case BET_PLAYER_WIN:
	            return RATE_PLAYER_WIN;
	        case BET_TIE:
	            return RATE_TIE;
	        case BET_BANKER_PAIR:
	        case BET_PLAYER_PAIR:
	            return RATE_PAIR;
            default:
                return 0;
	    }
	}

	uint8_t card_point(card_t card) {
	    uint8_t value = card_value(card);
	    return value > 9 ? 0 : value;
	}

	uint8_t cards_point(const vector<card_t>& cards) {
	    uint8_t result = 0;
	    for (card_t card: cards) {
	        result += card_point(card);
	    }
	    return result % 10;
	}

	bool banker_draw_third_card(uint8_t banker_point, uint8_t player_third_card) {
        switch (banker_point) {
            case 0:
            case 1:
            case 2:
                return true;
            case 3:
                return player_third_card != 8;
            case 4:
                return player_third_card >= 2 && player_third_card <= 7;
            case 5:
                return player_third_card >= 4 && player_third_card <= 7;
            case 6:
                return player_third_card == 6 || player_third_card == 7;
            default:
                return false;
        }
	}

    void baccarat::reveal(uint64_t game_id) {
        require_auth(_self);

        auto idx = _games.get_index<name("byid")>();
        auto gm_pos = idx.find(game_id);
        uint32_t timestamp = now();

        eosio_assert(gm_pos != idx.end() && gm_pos->id == game_id, "reveal: game id does't exist!");
        eosio_assert(gm_pos->status == GAME_STATUS_ACTIVE && timestamp >= gm_pos->end_time, "Can not reveal yet");

        vector<card_t> cards, banker_cards, player_cards;
        random random_gen;
        add_card(random_gen, banker_cards, cards, NUM_CARDS);
        add_card(random_gen, banker_cards, cards, NUM_CARDS);

        add_card(random_gen, player_cards, cards, NUM_CARDS);
        add_card(random_gen, player_cards, cards, NUM_CARDS);

        uint8_t banker_point = cards_point(banker_cards);
        uint8_t player_point = cards_point(player_cards);

        if (banker_point < 8 && player_point < 6) {
            uint8_t player_third_card = card_point(add_card(random_gen, player_cards, cards, NUM_CARDS));
            player_point = cards_point(player_cards);

            if (banker_draw_third_card(banker_point, player_third_card)) {
                add_card(random_gen, banker_cards, cards, NUM_CARDS);
                banker_point = cards_point(banker_cards);
            }
        }

        uint8_t result = 0;
        if (player_point > banker_point) {
            result = BET_PLAYER_WIN;
        } else if (banker_point > player_point) {
            result = BET_BANKER_WIN;
        } else {
            result = BET_TIE;
        }

        if (card_point(player_cards[0]) == card_point(player_cards[1])) {
            result |= BET_PLAYER_PAIR;
        }
        if (card_point(banker_cards[0]) == card_point(banker_cards[1])) {
            result |= BET_BANKER_PAIR;
        }

        uint64_t next_game_id = increment_global(_globals, G_ID_GAME_ID);
        idx.modify(gm_pos, _self, [&](auto &a) {
            a.id = next_game_id;
            a.status = GAME_STATUS_STANDBY;
            a.end_time = timestamp + GAME_RESOLVE_TIME;

            a.player_cards = player_cards;
            a.banker_cards = banker_cards;
        });

        auto bet_index = _bets.get_index<name("bygameid")>();
        char buff[128];
        sprintf(buff, "[GoDapp] Baccarat game win!");
        string msg(buff);

        for (auto itr = bet_index.begin(); itr != bet_index.end();) {
            uint8_t bet_type = itr->bet_type;
            if (bet_type & result) {
                asset payout(itr->bet.amount * payout_rate(bet_type), itr->bet.symbol);
                if (payout.amount > 0) {
                    transaction trx;
                    trx.actions.emplace_back(permission_level{ _self, name("active") }, HOUSE_ACCOUNT, name("pay"),
                                             make_tuple(_self, itr->player, itr->bet, payout, msg, itr->referer));
                    trx.delay_sec = 1;
                    trx.send(_self.value, _self);
                }
            }
            itr = bet_index.erase(itr);
        }
    }
};