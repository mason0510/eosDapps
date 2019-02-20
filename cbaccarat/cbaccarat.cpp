#include "../cbaccarat/cbaccarat.hpp"
#include "../common/tables.hpp"
#include "../common/param_reader.hpp"
#include "../common/eosio.token.hpp"
#include "../house/house.hpp"

#define BET_BANKER_WIN              1
#define BET_PLAYER_WIN              2
#define BET_TIE                     4
#define BET_BANKER_PAIR             8
#define BET_PLAYER_PAIR             16

namespace godapp {
    DEFINE_STANDARD_FUNCTIONS(cbaccarat)

    class cbaccarat_result {
    public:
        vector<card_t> banker_cards, player_cards;
        bool banker_pair, player_pair;
        uint8_t banker_point, player_point;
        uint8_t game_result;
        uint8_t result;

        cbaccarat_result(random &random_gen) {
            draw_cards(banker_cards, banker_point, player_cards, player_point, random_gen);

            if (player_point > banker_point) {
                game_result = BET_PLAYER_WIN;
            } else if (banker_point > player_point) {
                game_result = BET_BANKER_WIN;
            } else {
                game_result = BET_TIE;
            }

            banker_pair = card_value(banker_cards[0]) == card_value(banker_cards[1]);
            player_pair = card_value(player_cards[0]) == card_value(player_cards[1]);

            result = game_result | (banker_pair ? BET_BANKER_PAIR : 0) | (player_pair ? BET_PLAYER_PAIR : 0);
        }

        asset get_payout(const cbaccarat::bet &bet_item) {
            asset bet = bet_item.bet;
            switch (bet_item.bet_type) {
                case BET_BANKER_WIN:
                    if (game_result == BET_BANKER_WIN) {
                        return bet * 195 / 100;
                    } else if (game_result == BET_TIE) {
                        return bet;
                    } else {
                        return asset(0, bet.symbol);
                    }
                case BET_PLAYER_WIN:
                    if (game_result == BET_PLAYER_WIN) {
                        return bet * 2;
                    } else if (game_result == BET_TIE) {
                        return bet;
                    } else {
                        return asset(0, bet.symbol);
                    }
                case BET_TIE:
                    return game_result == BET_TIE ? (bet * 9) : asset(0, bet.symbol);
                case BET_BANKER_PAIR:
                    return banker_pair ? (bet * 12) : asset(0, bet.symbol);
                case BET_PLAYER_PAIR:
                    return player_pair ? (bet * 12) : asset(0, bet.symbol);
                default:
                    return asset(0, bet.symbol);
            }
        }

        string result_string() {
            switch (game_result) {
                case BET_BANKER_WIN:
                    return "Banker Wins";
                case BET_PLAYER_WIN:
                    return "Player Wins";
                case BET_TIE:
                    return "Tie";
                default:
                    return "";
            }
        }

        void update_game(cbaccarat::game &game) {
            game.player_cards = player_cards;
            game.banker_cards = banker_cards;
        }

        void set_receipt(cbaccarat &contract, uint64_t game_id, capi_checksum256 seed) {
            SEND_INLINE_ACTION(contract, receipt, { contract.get_self(), name("active") }, { game_id, seed,
                                   cards_to_string(player_cards), player_point,
                                   cards_to_string(banker_cards), banker_point, result_string() });
        }
    };

    void cbaccarat::reveal(uint64_t game_id, capi_signature signature) {
        auto idx = _games.get_index<name("byid")>();
        auto gm_pos = idx.find(game_id);
        uint32_t timestamp = now();

        eosio_assert(gm_pos != idx.end() && gm_pos->id == game_id, "reveal: game id does't exist!");
        eosio_assert(gm_pos->status == GAME_STATUS_ACTIVE && timestamp >= gm_pos->end_time, "Can not reveal yet");
        capi_public_key random_key = _random_keys.get(0, "random key is not set").key;
        random random_gen = random_from_sig(random_key, gm_pos->seed, signature);
        cbaccarat_result result(random_gen);
        history_table history(_self, _self.value);
        uint64_t history_id = get_global(_globals, G_ID_HISTORY_ID);

        map<uint64_t, pay_result> result_map;
        auto bet_index = _bets.get_index<name("bygameid")>();
        for (auto itr = bet_index.begin(); itr != bet_index.end();) {
            uint8_t bet_type = itr->bet_type;
            asset bet = itr->bet;
            asset payout = result.get_payout(*itr);
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
                         current.payout.amount >= current.bet.amount ?
                         "[Dapp365] win!" : "[Dapp365] lose!" );
        }
        uint64_t next_game_id = increment_global(_globals, G_ID_GAME_ID);
        name winner_name = largest_winner == result_map.end() ? name() : name(largest_winner->first);
        idx.modify(gm_pos, _self, [&](auto &a) {
            a.id = next_game_id;
            a.status = GAME_STATUS_STANDBY;
            a.end_time = timestamp + GAME_RESOLVE_TIME;
            a.largest_winner = winner_name;
            a.largest_win_amount = asset(win_amount, EOS_SYMBOL);
            result.update_game(a);
        });
        for (auto itr = _bet_amount.begin(); itr != _bet_amount.end();) {
            itr = _bet_amount.erase(itr);
        }
        uint64_t result_index = increment_global_mod(_globals, G_ID_RESULT_ID, RESULT_SIZE);
        table_upsert(_results, _self, result_index, [&](auto &a) {
            a.id = result_index;
            a.game_id = game_id;
            a.result = result.result;
        });
        delayed_action(_self, _self, name("newround"), make_tuple(gm_pos->symbol), GAME_RESOLVE_TIME);
        result.set_receipt(*this, game_id, gm_pos->seed);
    }

    void cbaccarat::receipt(uint64_t game_id, capi_checksum256 seed, string player_cards, uint8_t player_point,
        string banker_cards,uint8_t banker_point, string result) {
        require_auth(_self);
        require_recipient(_self);
    }
};