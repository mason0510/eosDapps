#include "../baccarat/baccarat.hpp"
#include "../common/cards.hpp"
#include "../common/tables.hpp"
#include "../common/param_reader.hpp"
#include "../common/eosio.token.hpp"
#include "../house/house.hpp"

#define G_ID_START                  101
#define G_ID_RESULT_ID              101
#define G_ID_GAME_ID                102
#define G_ID_BET_ID                 103
#define G_ID_HISTORY_ID             104
#define G_ID_END                    104

#define GAME_LENGTH                 45
#define GAME_RESOLVE_TIME           20

#define NUM_CARDS                   52 * 8
#define RESULT_SIZE                 50
#define HISTORY_SIZE                40

#define BET_BANKER_WIN              1
#define BET_PLAYER_WIN              2
#define BET_TIE                     3
#define BET_DRAGON                  4
#define BET_PANDA                   5

namespace godapp {
    uint8_t PAYOUT_MATRIX[5][5] = {
            {2,     0,      0,      0,      0}, // BANKER
            {0,     2,      0,      0,      0}, // PLAYER
            {1,     1,      9,      0,      0}, // TIE
            {1,     0,      0,      41,     0}, // DRAGON
            {0,     0,      0,      0,      26} // PANDA
    };
    //      BANKER, PLAYER, TIE,    DRAGON, PANDA
    baccarat::baccarat(name receiver, name code, datastream<const char*> ds):
            contract(receiver, code, ds),
            _globals(_self, _self.value),
            _games(_self, _self.value),
            _bets(_self, _self.value),
            _results(_self, _self.value){
    }

    DEFINE_STANDARD_FUNCTIONS(baccarat)

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

	string result_to_string(uint8_t result) {
        switch (result) {
            case BET_BANKER_WIN:
                return "Banker Wins";
            case BET_PLAYER_WIN:
                return "Player Wins";
            case BET_TIE:
                return "Tie";
            case BET_DRAGON:
                return "Dragon 7";
            case BET_PANDA:
                return "Panda 8";
            default:
                return "";
        }
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

        if (banker_point < 8 && player_point < 8) {
            if (player_point < 6) {
                uint8_t player_third_card = card_point(add_card(random_gen, player_cards, cards, NUM_CARDS));
                player_point = cards_point(player_cards);

                if (banker_draw_third_card(banker_point, player_third_card)) {
                    add_card(random_gen, banker_cards, cards, NUM_CARDS);
                    banker_point = cards_point(banker_cards);
                }
            } else if (banker_point < 6) {
                add_card(random_gen, banker_cards, cards, NUM_CARDS);
                banker_point = cards_point(banker_cards);
            }
        }

        uint8_t result = 0;
        if (player_point > banker_point) {
            if (player_point == 8 && player_cards.size() == 3) {
                result = BET_PANDA;
            } else {
                result = BET_PLAYER_WIN;
            }
        } else if (banker_point > player_point) {
            if (banker_point == 7 && banker_cards.size() == 3) {
                result = BET_DRAGON;
            } else {
                result = BET_BANKER_WIN;
            }
        } else {
            result = BET_TIE;
        }

        uint8_t* payout_array = PAYOUT_MATRIX[result - 1];

        history_table history(_self, _self.value);
        uint64_t history_id = get_global(_globals, G_ID_HISTORY_ID);

        map<uint64_t, pay_result> result_map;
        auto bet_index = _bets.get_index<name("bygameid")>();
        for (auto itr = bet_index.begin(); itr != bet_index.end();) {
            uint8_t bet_type = itr->bet_type;
            asset bet = itr->bet;
            asset payout = bet * payout_array[bet_type - 1];

            history_id++;
            uint64_t id = history_id % HISTORY_SIZE;
            table_upsert(history, _self, id, [&](auto &a) {
                a.id = id;
                a.history_id = history_id;
                a.player = itr->player;
                a.bet = bet;
                a.bet_type = bet_type;
                a.payout = payout;
                a.close_time = timestamp;
            });

            if (payout.amount > 0) {
                auto result_itr = result_map.find(itr->player.value);
                if (result_itr == result_map.end()) {
                    result_map[itr->player.value] = pay_result{bet, payout, itr->referer};
                } else {
                    result_itr->second.bet += bet;
                    result_itr->second.payout += payout;
                }
            }
            itr = bet_index.erase(itr);
        }
        set_global(_globals, G_ID_HISTORY_ID, history_id);

        auto largest_winner = result_map.end();
        int64_t win_amount = 0;
        for (auto itr = result_map.begin(); itr != result_map.end(); itr++) {
            auto current = itr->second;
            if (current.payout.amount > current.bet.amount &&
                current.payout.amount > win_amount &&
                current.payout.symbol == EOS_SYMBOL) {
                largest_winner = itr;
                win_amount = current.payout.amount;
            }
            make_payment(_self, name(itr->first), current.bet, current.payout, current.referer, "[Dapp365] Baccarat win!");
        }

        uint64_t next_game_id = increment_global(_globals, G_ID_GAME_ID);
        name winner_name = largest_winner == result_map.end() ? name() : name(largest_winner->first);
        idx.modify(gm_pos, _self, [&](auto &a) {
            a.id = next_game_id;
            a.status = GAME_STATUS_STANDBY;
            a.end_time = timestamp + GAME_RESOLVE_TIME;
            a.largest_winner = winner_name;
            a.largest_win_amount = asset(win_amount, EOS_SYMBOL);
            a.player_cards = player_cards;
            a.banker_cards = banker_cards;
        });

        uint64_t result_index = increment_global_mod(_globals, G_ID_RESULT_ID, RESULT_SIZE);
        table_upsert(_results, _self, result_index, [&](auto &a) {
            a.id = result_index;
            a.game_id = game_id;
            a.result = result;
        });

        delayed_action(_self, _self, name("newround"), make_tuple(gm_pos->symbol), GAME_RESOLVE_TIME);

        SEND_INLINE_ACTION(*this, receipt, {_self, name("active")},
                {game_id, cards_to_string(player_cards), player_point, cards_to_string(banker_cards), banker_point, result_to_string(result)});
    }

    void baccarat::receipt(uint64_t game_id, string player_cards, uint8_t player_point, string banker_cards, uint8_t banker_point, string result) {
        require_auth(_self);
        require_recipient(_self);
    }
};