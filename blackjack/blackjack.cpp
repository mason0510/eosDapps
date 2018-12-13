#include "blackjack.hpp"

#define card_t uint8_t
#include "../common/cards.hpp"
#include "../common/tables.hpp"
#include "../common/param_reader.hpp"
#include "../common/eosio.token.hpp"
#include "../house/house.hpp"
#include <eosiolib/print.hpp>

#define G_ID_START                  101

#define G_ID_ACTIVE                 101
#define G_ID_GAME_ID                102
#define G_ID_HISTORY_INDEX          103
#define G_ID_END                    103

#define GAME_MAX_TIME               1200*1e6
#define GAME_MAX_HISTORY_SIZE       50
#define GAME_MAX_POINTS             21

#define GAME_STATUS_ACTIVE          1
#define GAME_STATUS_STOOD           2
#define GAME_STATUS_CLOSED          3

#define NUM_CARDS                   52
#define BANKER_STAND_POINT          17
#define MAX_CARD_VALUE              10

#define PLAYER_ACTION_NEW           0
#define PLAYER_ACTION_HIT           1
#define PLAYER_ACTION_DOUBLE        2
#define PLAYER_ACTION_INSURE        3
#define PLAYER_ACTION_STAND         4
#define PLAYER_ACTION_SURRENDER     5

#define GAME_RESULT_SURRENDER       1
#define GAME_RESULT_WIN             2
#define GAME_RESULT_LOSE            3
#define GAME_RESULT_PUSH            4

namespace godapp {
	blackjack::blackjack(name receiver, name code, datastream<const char*> ds):
	contract(receiver, code, ds),
	_globals(_self, _self.value),
	_results(_self, _self.value),
	_games(_self, _self.value) {
	}

	void blackjack::init() {
        require_auth(TEAM_ACCOUNT);
        init_globals(_globals, G_ID_START, G_ID_END);
	}

	const char* result_string(uint8_t result) {
	    switch (result) {
	        case GAME_RESULT_SURRENDER: return "surrender";
            case GAME_RESULT_WIN: return "win";
            case GAME_RESULT_LOSE: return "lose";
            default: return "push";
	    }
	}

	uint8_t card_point(uint8_t card) {
	    return min(card_value(card), (uint8_t) MAX_CARD_VALUE);
	}

    uint8_t cal_points(const vector<uint8_t>& cards) {
        uint8_t points = 0;
        bool has_A = false;

        for (uint8_t card : cards) {
            uint8_t point = card_point(card);
            if (point == 1) {
                has_A = true;
            }
            points += point;
        }

        if (has_A && points + 10 <= GAME_MAX_POINTS) {
            points += 10;
        }
        return points;
    }

    uint8_t random_card(random& random_gen) {
        return (uint8_t) random_gen.generator(NUM_CARDS);
	}

    void blackjack::setglobal(uint64_t key, uint64_t value) {
        require_auth(_self);
        set_global(_globals, key, value);
    }

	void blackjack::play(name player, asset bet, string memo){
        param_reader reader(memo);
        string action_name = reader.next_param("action is missing");
        name referrer = reader.get_referer(player);

        if (action_name == "new") {
            new_game(player, bet, referrer);
        } else {
            eosio_assert(false, "unknown action to play");
        }
    }

	void blackjack::new_game(name player, asset& bet, name referer) {
		auto pos = _games.find(player.value);
		eosio_assert(pos == _games.end() || pos->status == GAME_STATUS_CLOSED , "your last game is in progress!");

		game_item gm;
        gm.player  = player;
        gm.id = increment_global(_globals, G_ID_GAME_ID);
        gm.start_time = current_time();
        gm.close_time = 0;
        gm.referer = referer;
        gm.bet     = bet;
        gm.insured = false;
        gm.status  = GAME_STATUS_ACTIVE;

		table_upsert(_games, _self, player.value, [&](auto& info) {
            info = gm;
		});

        SEND_INLINE_ACTION(*this, playeraction, {_self, name("active")}, make_tuple(player, PLAYER_ACTION_NEW) );
	}

	void blackjack::deal(uint64_t id, uint8_t action) {
		require_auth(_self);

		auto idx = _games.get_index<name("byid")>();
		auto gm_pos = idx.find(id);
		eosio_assert(gm_pos != idx.end() && gm_pos->id == id, "deal: game id does't exist!");
		auto gm = *gm_pos;

		random random_gen;
		switch (action) {
		    case PLAYER_ACTION_NEW: {
                gm.banker_cards.push_back(random_card(random_gen));

                gm.player_cards.push_back(random_card(random_gen));
                gm.player_cards.push_back(random_card(random_gen));

                uint8_t player_points = cal_points(gm.player_cards);
                eosio_assert(player_points <= GAME_MAX_POINTS, "it's impossible that points larger than 21 in first deal! ");

                if (player_points == GAME_MAX_POINTS) {
                    gm.status = GAME_STATUS_STOOD;
                }
                break;
		    }
		    case PLAYER_ACTION_HIT: {
                gm.player_cards.push_back(random_card(random_gen));
                uint8_t player_points = cal_points(gm.player_cards);
                if (player_points >= GAME_MAX_POINTS) {
                    gm.status = GAME_STATUS_STOOD;
                }
		        break;
		    }
		    case PLAYER_ACTION_STAND: {
                gm.status = GAME_STATUS_STOOD;
		        break;
		    }
		    case PLAYER_ACTION_DOUBLE: {
                eosio_assert(gm.player_cards.size() == 2, "can double only after first deal!");
                gm.player_cards.push_back(random_card(random_gen));

                gm.status = GAME_STATUS_STOOD;
                break;
		    }
            case PLAYER_ACTION_SURRENDER: {
                eosio_assert(gm.player_cards.size() == 2, "can surrender only after first deal!");
                gm.status = GAME_STATUS_STOOD;
                gm.result = GAME_RESULT_SURRENDER;
                break;
            }
		    case PLAYER_ACTION_INSURE: {
                eosio_assert(gm.player_cards.size() == 2, "can insure only after first deal!");
                break;
		    }
            default: {
                eosio_assert(false, "dealed: unknown action");
            }
		}

        idx.modify(gm_pos, _self, [&](auto& info) {
            info = gm;
        });

        if (gm.status == GAME_STATUS_STOOD) {
            SEND_INLINE_ACTION(*this, close, {_self, name("active")}, make_tuple(gm.id) );
        }
	}

	void blackjack::playeraction(name player, uint8_t action) {
        if(action == PLAYER_ACTION_NEW || action == PLAYER_ACTION_INSURE || action == PLAYER_ACTION_DOUBLE) {
            require_auth(_self);
        } else {
            require_auth(player);
        }

		auto game = _games.get(player.value, "you have no game in progress!");
		eosio_assert(game.status == GAME_STATUS_ACTIVE, "Your game is not active!");

		transaction deal_trx;
		deal_trx.actions.emplace_back(permission_level{ _self, name("active") }, _self, name("deal"), make_tuple(game.id, action));
		deal_trx.delay_sec = 1;
		deal_trx.send(player.value, _self);
	}

	void blackjack::close(uint64_t id) {
		require_auth(_self);

		random random_gen;
		auto ct = current_time();

		auto game_iter = _games.get_index<name("byid")>();
		auto gm_pos = game_iter.find(id);
		eosio_assert(gm_pos != game_iter.end() && gm_pos->id == id, "game id doesn't exist!");
		auto gm = *gm_pos;

		asset payout(0, gm.bet.symbol);

		if (gm.status != GAME_STATUS_STOOD || gm.result == GAME_RESULT_SURRENDER) {
            payout = gm.bet / 2;
		} else {
            auto player_points = cal_points(gm.player_cards);
            if (player_points > GAME_MAX_POINTS) {
                payout = asset(0, gm.bet.symbol);
                gm.result = GAME_RESULT_LOSE;
            } else if (player_points == GAME_MAX_POINTS && gm.player_cards.size() == 2 ) {
                // player blackjack
                gm.banker_cards.push_back(random_card(random_gen));
                if (cal_points(gm.banker_cards) == GAME_MAX_POINTS) {
                    payout = gm.bet;
                    gm.result = GAME_RESULT_PUSH;
                } else {
                    payout = gm.bet * 25 / 10;
                    gm.result = GAME_RESULT_WIN;
                }
            } else {
                auto banker_points = cal_points(gm.banker_cards);
                while (banker_points < BANKER_STAND_POINT) {
                    gm.banker_cards.push_back(random_card(random_gen));
                    banker_points = cal_points(gm.banker_cards);
                }

                if (banker_points > GAME_MAX_POINTS || banker_points < player_points) {
                    payout = gm.bet * 2;
                    gm.result = GAME_RESULT_WIN;
                } else if (banker_points == player_points) {
                    payout = gm.bet;
                    gm.result = GAME_RESULT_PUSH;
                } else {
                    gm.result = GAME_RESULT_LOSE;
                }
            }
		}

		gm.status = GAME_STATUS_CLOSED;
		gm.close_time = ct;

		game_iter.modify(gm_pos, _self, [&](auto& info) {
			info = gm;
		});

        SEND_INLINE_ACTION(*this, receipt, {_self, name("active")},
		        make_tuple(gm, cards_to_string(gm.banker_cards), cards_to_string(gm.player_cards), "normal close") );

		uint64_t history_index = increment_global_mod(_globals, G_ID_HISTORY_INDEX, GAME_MAX_HISTORY_SIZE);
		table_upsert(_results, _self, history_index, [&](auto& info) {
            info.id = history_index;
            info.close_time = gm.close_time;

            info.player_cards = gm.player_cards;
            info.banker_cards = gm.banker_cards;

            info.player = gm.player;
            info.insured = gm.insured;

            info.bet = gm.bet;
            info.payout = payout;
            info.result = gm.result;
        });

        char msg[128];
        sprintf(msg, "[GoDapp] Game result: %s!", result_string(gm.result));
        transaction deal_trx;
        deal_trx.actions.emplace_back(permission_level{ _self, name("active") }, HOUSE_ACCOUNT, name("pay"),
                make_tuple(_self, gm.player, gm.bet, payout, string(msg), gm.referer));
        deal_trx.delay_sec = 0;
        deal_trx.send(_self.value, _self);
	}

	void blackjack::hardclose(uint64_t id, string reason) {
		require_auth(_self);

		eosio_assert(reason.size() <= 256, "reason size must <= 256");

		auto idx = _games.get_index<name("byid")>();
		auto gm_pos = idx.find(id);

		eosio_assert(gm_pos != idx.end() && gm_pos->id == id, "game id doesn't exist!");

		idx.modify(gm_pos, _self, [&](auto& info) {
			info.status = GAME_STATUS_CLOSED;
		});
	}

    void blackjack::cleargames(uint32_t num) {
		require_auth(_self);
		auto idx = _games.get_index<name("bytime")>();
		auto ct = current_time();
		uint32_t count = 0;
		for (auto itr=idx.begin(); itr!=idx.end() && count < num; count++) {
			if (itr->start_time + GAME_MAX_TIME <= ct) {
				if (itr->status != GAME_STATUS_CLOSED) {
                    close(itr->id);
                }
                itr = idx.erase(itr);
			}
		}
	}

	void blackjack::receipt(game_item gm, string banker_cards, string player_cards, string memo) {
		require_auth(_self);
		require_recipient(gm.player);
	}
};