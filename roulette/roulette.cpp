#include "../roulette/roulette.hpp"
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

#define GAME_LENGTH                 40
#define GAME_RESOLVE_TIME           15

#define RESULT_SIZE                 60
#define HISTORY_SIZE                40

#define BET_NUMBER_ZERO             1
#define BET_NUMBER_END              37
#define BET_EVEN                    38
#define BET_ODD                     39
#define BET_LARGE                   40
#define BET_SMALL                   41
#define BET_FRONT                   42
#define BET_MID                     43
#define BET_BACK                    44
#define BET_LINE_ONE                45
#define BET_LINE_TWO                46
#define BET_LINE_THREE              47


namespace godapp {
    roulette::roulette(name receiver, name code, datastream<const char*> ds):
    contract(receiver, code, ds),
    _globals(_self, _self.value),
    _games(_self, _self.value),
    _bets(_self, _self.value),
    _results(_self, _self.value){
    }

    DEFINE_STANDARD_FUNCTIONS(roulette)

    bool IS_RED[BET_NUMBER_END] = {
            false,              // 0
            true, false, true,  // 1
            false, true, false, // 4
            true, false, true,  // 7
            false, false, true, // 10
            false, true, false, // 13
            true, false, true,  // 16
            true, false, true,  // 19
            false, true, false, // 22
            true, false, true,  // 25
            false, false, true, // 28
            false, true, false, // 31
            true, false, true   // 34
    };


    uint8_t payout_rate(uint8_t result, uint8_t  bet, bool is_red) {
        if (result == 0) {
            return (bet == BET_NUMBER_ZERO) ? 36 : 0;
        }

        switch (bet) {
            case BET_EVEN:
                return (result % 2 == 0) ? 2 : 0;
            case BET_ODD:
                return (result % 2 == 1) ? 2 : 0;
            case BET_LARGE:
                return (result > 18) ? 2 : 0;
            case BET_SMALL:
                return (result <= 18) ? 2 : 0;
            case BET_FRONT:
                return (result <= 12) ? 3 : 0;
            case BET_MID:
                return (result > 12 && result <= 24) ? 3 : 0;
            case BET_BACK:
                return (result > 24) ? 3 : 0;
            case BET_LINE_ONE:
                return (result % 3 == 1) ? 3 : 0;
            case BET_LINE_TWO:
                return (result % 3 == 2) ? 3 : 0;
            case BET_LINE_THREE:
                return (result % 3 == 0) ? 3 : 0;
            default:
                return (result == bet - 1) ? 36 : 0;
        }
    }

    void roulette::reveal(uint64_t game_id) {
        require_auth(_self);

        auto idx = _games.get_index<name("byid")>();
        auto gm_pos = idx.find(game_id);
        uint32_t timestamp = now();

        eosio_assert(gm_pos != idx.end() && gm_pos->id == game_id, "reveal: game id does't exist!");
        eosio_assert(gm_pos->status == GAME_STATUS_ACTIVE && timestamp >= gm_pos->end_time, "Can not reveal yet");


        random random_gen;
        uint8_t result = random_gen.generator(BET_NUMBER_END);
        bool is_red = IS_RED[result];
        history_table history(_self, _self.value);
        uint64_t history_id = get_global(_globals, G_ID_HISTORY_ID);

        map<uint64_t, pay_result> result_map;
        auto bet_index = _bets.get_index<name("bygameid")>();
        for (auto itr = bet_index.begin(); itr != bet_index.end();) {
            uint8_t bet_type = itr->bet_type;
            asset bet = itr->bet;
            asset payout = bet * payout_rate(result, bet_type, is_red);

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
            if (current.payout.amount > win_amount && current.payout.symbol == EOS_SYMBOL) {
                largest_winner = itr;
                win_amount = current.payout.amount;
            }
            make_payment(_self, name(itr->first), current.bet, current.payout, current.referer, "[GoDapp] Roulette win!");
        }

        uint64_t next_game_id = increment_global(_globals, G_ID_GAME_ID);
        name winner_name = largest_winner == result_map.end() ? name() : name(largest_winner->first);
        idx.modify(gm_pos, _self, [&](auto &a) {
            a.id = next_game_id;
            a.status = GAME_STATUS_STANDBY;
            a.end_time = timestamp + GAME_RESOLVE_TIME;
            a.largest_winner = winner_name;
            a.largest_win_amount = asset(win_amount, EOS_SYMBOL);
            a.result = result;
        });

        SEND_INLINE_ACTION(*this, receipt, {_self, name("active")}, {game_id, result});

        uint64_t result_index = increment_global_mod(_globals, G_ID_RESULT_ID, RESULT_SIZE);
        table_upsert(_results, _self, result_index, [&](auto &a) {
            a.id = result_index;
            a.game_id = game_id;
            a.result = result;
        });

        delayed_action(_self, _self, name("newround"), make_tuple(gm_pos->symbol), GAME_RESOLVE_TIME);
    }

    void roulette::receipt(uint64_t game_id, uint8_t result) {
        require_auth(_self);
        require_recipient(_self);
    }
};