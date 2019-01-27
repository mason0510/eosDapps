#include "../baccarat/baccarat.hpp"
#include "../common/cards.hpp"
#include "../common/tables.hpp"
#include "../common/param_reader.hpp"
#include "../common/eosio.token.hpp"
#include "../house/house.hpp"

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
            {0,     2,      0,      0,      26} // PANDA
    };
    //      BANKER, PLAYER, TIE,    DRAGON, PANDA

    baccarat::baccarat(name receiver, name code, datastream<const char*> ds):
            contract(receiver, code, ds),
            _globals(_self, _self.value),
            _games(_self, _self.value),
            _bets(_self, _self.value),
            _results(_self, _self.value),
            _bet_amount(_self, _self.value) {
    }

    DEFINE_STANDARD_FUNCTIONS(baccarat)

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

    void baccarat::reveal(uint64_t game_id) {
        require_auth(_self);

        auto idx = _games.get_index<name("byid")>();
        auto gm_pos = idx.find(game_id);
        uint32_t timestamp = now();

        eosio_assert(gm_pos != idx.end() && gm_pos->id == game_id, "reveal: game id does't exist!");
        eosio_assert(gm_pos->status == GAME_STATUS_ACTIVE && timestamp >= gm_pos->end_time, "Can not reveal yet");
        vector<card_t> banker_cards, player_cards;
        uint8_t banker_point, player_point;
        draw_cards(banker_cards, banker_point, player_cards, player_point);

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

            auto result_itr = result_map.find(itr->player.value);
            if (result_itr == result_map.end()) {
                result_map[itr->player.value] = pay_result{bet, payout, itr->referer};
            } else {
                result_itr->second.bet += bet;
                result_itr->second.payout += payout;
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
            make_payment(_self, name(itr->first), current.bet, current.payout, current.referer,
                current.payout.amount >= current.bet.amount ? "[Dapp365] Baccarat win!" : "[Dapp365] Baccarat lose!" );
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

        for (auto itr = _bet_amount.begin(); itr != _bet_amount.end();) {
            itr = _bet_amount.erase(itr);
        }

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