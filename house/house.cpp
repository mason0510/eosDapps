#define DEFINE_DISPATCHER 1

#include "../house/house.hpp"
#include "../common/tables.hpp"
#include "../common/param_reader.hpp"
#include "../common/eosio.token.hpp"


#define EVENT_LENGTH                86400

namespace godapp {
    void house::addgame(name game) {
        require_auth(_self);

        eosio_assert(is_account(game), "must be an eos account");

        game_index games(_self, _self.value);
        auto iter = games.find(game.value);
        if (iter == games.end()) {
            games.emplace(_self, [&](auto &a) {
                a.name = game;
                a.active = true;
            });

            token_index game_token(_self, game.value);
            game_token.emplace(_self, [&](auto &a) {
                a.sym = EOS_SYMBOL;
                a.contract = EOS_TOKEN_CONTRACT;

                a.min = 1000;
                a.max = 500000;
                a.balance = 0;
            });
        }

        eosio::transaction r_out;
        auto t_data = make_tuple();
        r_out.actions.emplace_back(eosio::permission_level{_self, name("active")}, _self, name("init"), t_data);
        r_out.delay_sec = 0;
        r_out.send(_self.value, _self);
    }

    void house::setactive(name game, bool active) {
        require_auth(_self);

        game_index games(_self, _self.value);
        auto iter = games.find(game.value);
        eosio_assert(iter != games.end(), "game does not exist");

        games.modify(iter, _self, [&](auto &a) {
            a.active = active;
        });
    }

    void house::transfer(name from, name to, asset quantity, string memo) {
        if (from == _self || to != _self) {
            return;
        }

        if (from == name("eosio.stake")) {
            return;
        }

        transaction trx = get_transaction();
        action first_action = trx.actions.front();
        eosio_assert(first_action.name == name("transfer") && first_action.account == _code, "wrong transaction");
        eosio_assert(quantity.is_valid(), "Invalid transfer amount.");
        eosio_assert(quantity.amount > 0, "Transfer amount not positive");
        eosio_assert(!memo.empty(), "Memo is required");

        param_reader reader(memo);
        string target = reader.next_param("target can not be empty");
        if (target == "deposit") {
            target = reader.next_param();
            uint64_t game_code = name(target).value;
            token_index game_token(_self, game_code);

            auto token_iter = game_token.find(quantity.symbol.raw());
            eosio_assert(token_iter != game_token.end(), "token is not supported");
            game_token.modify(token_iter, _self, [&](auto &a) {
                a.balance += quantity.amount;
            });
        } else {
            game_index games(_self, _self.value);
            name game_name = name(target);
            uint64_t game_code = game_name.value;

            auto game = games.get(game_code, "Game does not exist");
            eosio_assert(game.active, "game is not active");

            token_index game_token(_self, game_code);
            auto token_iter = game_token.find(quantity.symbol.raw());
            eosio_assert(token_iter != game_token.end(), "token is not supported");
            eosio_assert(quantity.amount >= token_iter->min && quantity.amount <= token_iter->max, "Invalid amount");
            game_token.modify(token_iter, _self, [&](auto &a) {
                a.in += quantity.amount;
                a.balance += quantity.amount;
                a.play_times += 1;
            });

            player_index game_player(_self, game_code);
            auto player_iter = game_player.find(from.value);
            if (player_iter == game_player.end()) {
                game_player.emplace(_self, [&](auto &a) {
                    a.name = from;
                    a.in = quantity.amount;
                    a.out = 0;
                    a.play_times = 1;
                    a.last_play_time = now();
                    a.event_in = quantity.amount;
                });
            } else {
                uint32_t timestamp = now();
                uint64_t event_in = (player_iter->last_play_time / EVENT_LENGTH) > (timestamp / EVENT_LENGTH) ?
                        0 : player_iter->event_in;
                event_in += quantity.amount;

                game_player.modify(player_iter, _self, [&](auto &a) {
                    a.in += quantity.amount;
                    a.play_times += 1;
                    a.last_play_time = timestamp;
                    a.event_in = event_in;
                });
            }

            eosio::transaction r_out;
            auto t_data = make_tuple(from, quantity, reader.rest());
            r_out.actions.emplace_back(eosio::permission_level{_self, name("active")}, game_name, name("play"), t_data);
            r_out.delay_sec = 0;
            r_out.send(from.value, _self);
        }
    }

    void house::pay(name game, name to, asset quantity, string memo, name referer) {
        player_index game_player(_self, game.value);
        token_index game_token(_self, game.value);

        auto player_iter = game_player.find(to.value);
        eosio_assert(player_iter != game_player.end(), "Player does not exist");
        game_player.modify(player_iter, _self, [&](auto &a) {
            a.out += quantity.amount;
        });

        auto token_iter = game_token.find(quantity.symbol.raw());
        eosio_assert(token_iter != game_token.end(), "Token not supported");
        eosio_assert(token_iter->balance > quantity.amount, "token balance depleted");
        game_token.modify(token_iter, _self, [&](auto &a) {
            a.out += quantity.amount;
            a.balance -= quantity.amount;
        });

        INLINE_ACTION_SENDER(eosio::token, transfer)(token_iter->contract, {_self, name("active")}, {_self, to, quantity, memo} );
    }

    void house::updatetoken(name game, symbol token, name contract, uint64_t min, uint64_t max, uint64_t balance) {
        require_auth(_self);
        token_index game_token(_self, game.value);

        table_upsert(game_token, _self, token.raw(), [&](auto &a) {
            a.sym = token;
            a.contract = contract;
            a.min = min;
            a.max = max;
            a.balance = balance;
        });
    }
};
