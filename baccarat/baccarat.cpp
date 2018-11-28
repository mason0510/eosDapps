#include "../baccarat/baccarat.hpp"
#include "../common/cards.hpp"
#include "../common/tables.hpp"
#include "../common/param_reader.hpp"
#include "../common/eosio.token.hpp"
#include <eosiolib/print.hpp>

#define G_ID_START                  101

#define G_ID_ACTIVE                 101
#define G_ID_GAME_ID                102
#define G_ID_BET_ID                 103
#define G_ID_END                    103

#define GAME_LENGTH                 30
#define GAME_RESOLVE_TIME           5

#define GAME_STATUS_UNKNOWN         0
#define GAME_STATUS_STANDBY         1
#define GAME_STATUS_ACTIVE          2


#define MAX_BET_FEE                 0.005
#define NUM_CARDS                   52 * 8

#define RATE_PLAYER_WIN             1
#define RATE_BANKER_WIN             0.95
#define RATE_PAIR                   11
#define RATE_TIE                    8

#define BET_BANKER_WIN              1
#define BET_PLAYER_WIN              2
#define BET_TIE                     4
#define BET_BANKER_PAIR             8
#define BET_PLAYER_PAIR             16

namespace godapp {
	baccarat::baccarat(name receiver, name code, datastream<const char*> ds):
	contract(receiver, code, ds),
	_globals(_self, _self.value),
	_tokens(_self, _self.value),
	_games(_self, _self.value),
	_bets(_self, _self.value){
	}

	void baccarat::init() {
        require_auth(_self);

        init_globals(_globals, G_ID_START, G_ID_END);
        init_token(_tokens, EOS_SYMBOL, EOS_TOKEN_CONTRACT);
        init_game(EOS_SYMBOL);
	}

    void baccarat::init_game(symbol sym) {
        auto iter = _games.find(sym.raw());
        if (iter == _games.end()) {
            uint64_t next_id = increment_global(_globals, G_ID_GAME_ID);
            _games.emplace(_self, [&](auto &a) {
                a.id = next_id;
                a.symbol = sym;
                a.status = GAME_STATUS_STANDBY;
            });
        }
	}

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
                eosio_assert(false, "Invalid bet type");
                return 0;
	    }
	}

    void baccarat::setglobal(uint64_t key, uint64_t value) {
        require_auth(_self);
        set_global(_globals, key, value);
    }

	void baccarat::transfer(name from, name to, asset quantity, string memo) {
        if (check_transfer(this, from, to, quantity, memo)) {
            eosio_assert(get_global(_globals, G_ID_ACTIVE), "game is not active!");
            _tokens.get(quantity.symbol.raw(), "game do not support the token");

            param_reader reader(memo);
            auto game_id = (uint64_t) atol( reader.next_param("Game ID cannot be empty!").c_str() );
            auto bet_type = (uint8_t) atoi( reader.next_param("Bet type cannot be empty!").c_str() );
            name referer = reader.get_referrer(from);
            bet(from, referer, game_id, bet_type, quantity);
        }
    }

    void baccarat::bet(name player, name referer, uint64_t game_id, uint8_t bet_type, asset amount) {
		auto game_iter = _games.find(amount.symbol.raw());
		eosio_assert(game_iter->id == game_id, "Game is no longer active");

        uint32_t timestamp = now();
		uint8_t status = game_iter->status;
		switch (status) {
		    case GAME_STATUS_STANDBY: {
                eosio_assert(timestamp > game_iter->end_time, "Game resolving, please wait");

                _games.modify(game_iter, _self, [&](auto &a) {
                    a.end_time = now() + GAME_LENGTH * 1e6;
                    a.status = GAME_STATUS_ACTIVE;
                });

                transaction close_trx;
                close_trx.actions.emplace_back(permission_level{ _self, name("active") }, _self, name("reveal"), make_tuple(game_id));
                close_trx.delay_sec = GAME_LENGTH;
                close_trx.send(player.value, _self);
                break;
		    }
            case GAME_STATUS_ACTIVE:
                eosio_assert(timestamp < game_iter->end_time, "Game already finished, please wait for next round");
                break;
            default:
                eosio_assert(false, "Invalid game state");
		}

		uint64_t next_bet_id = increment_global(_globals, G_ID_BET_ID);
		_bets.emplace(_self, [&](auto &a) {
		   a.id = next_bet_id;
		   a.game_id = game_id;
		   a.player = player;
		   a.referer = referer;
		   a.bet = amount;
		   a.bet_type = bet_type;
		});
	}

	card_t add_card(random& random_gen, vector<card_t>& target, vector<card_t>& existing) {
        sort(existing.begin(), existing.end());

        card_t card = draw_random_card(random_gen, existing, NUM_CARDS);
        target.push_back(card);
        existing.push_back(card);

        return card;
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
        eosio_assert(gm_pos->status == GAME_STATUS_ACTIVE && timestamp > gm_pos->end_time, "Can not reveal yet");

        vector<card_t> cards, banker_cards, player_cards;
        random random_gen;
        add_card(random_gen, banker_cards, cards);
        add_card(random_gen, banker_cards, cards);

        add_card(random_gen, player_cards, cards);
        add_card(random_gen, player_cards, cards);

        uint8_t banker_point = cards_point(banker_cards);
        uint8_t player_point = cards_point(player_cards);

        if (banker_point < 8 && player_point < 6) {
            uint8_t player_third_card = card_point(add_card(random_gen, player_cards, cards));
            player_point = cards_point(player_cards);

            if (banker_draw_third_card(banker_point, player_third_card)) {
                add_card(random_gen, banker_cards, cards);
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

        idx.modify(gm_pos, _self, [&](auto &a) {
            a.status = GAME_STATUS_STANDBY;
            a.end_time = timestamp + GAME_RESOLVE_TIME * 1e6;

            a.player_cards = player_cards;
            a.banker_cards = banker_cards;
        });

        auto bet_iter = _bets.get_index<name("bygameid")>();
        char msg[128];
        sprintf(msg, "[GoDapp] Baccarat game win!");

        for (auto iter = bet_iter.begin(); iter != bet_iter.end();) {
            uint8_t bet_type = iter->bet_type;
            if (bet_type | result) {
                asset payout(iter->bet.amount * payout_rate(bet_type), iter->bet.symbol);
                INLINE_ACTION_SENDER(eosio::token, transfer)(_self, {_self, name("active")}, {_self, iter->player, payout, msg} );
            }
        }
    }
};