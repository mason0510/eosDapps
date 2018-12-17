#define DEFINE_DISPATCHER 1

#include "../house/house.hpp"
#include "../common/tables.hpp"
#include "../common/param_reader.hpp"
#include "../common/eosio.token.hpp"


#define EVENT_LENGTH                86400
#define REFERRAL_BOUUS              5

namespace godapp {
    /**
     * Register a game to the list of supported games in the house
     * @param game Name of the game contract
     * @param id Internal tracking id used in transfer memo
     */
    void house::addgame(name game, uint64_t id) {
        require_auth(_self);

        eosio_assert(is_account(game), "must be an eos account");

        // Register the game into the game table
        game_index games(_self, _self.value);
        auto iter = games.find(game.value);
        if (iter == games.end()) {
            games.emplace(_self, [&](auto &a) {
                a.name = game;
                a.id = id;
                a.active = true;
            });

            // be default every game accepts EOS token
            token_index game_token(_self, game.value);
            auto token_iter = game_token.find(EOS_SYMBOL.raw());
            if (token_iter == game_token.end()) {
                game_token.emplace(_self, [&](auto &a) {
                    a.sym = EOS_SYMBOL;
                    a.contract = EOS_TOKEN_CONTRACT;

                    a.min = 1000;
                    a.max = 500000;
                    a.balance = 0;
                });
            }
        }

        // call the game to initialize itself
        eosio::transaction r_out;
        auto t_data = make_tuple();
        r_out.actions.emplace_back(eosio::permission_level{_self, name("active")}, game, name("init"), t_data);
        r_out.delay_sec = 0;
        r_out.send(_self.value, _self);
    }

    /**
     * Set the active state of a game, only active games can be played
     * @param game Name of the game to modify
     * @param active The new active state
     */
    void house::setactive(name game, bool active) {
        require_auth(_self);

        game_index games(_self, _self.value);
        table_modify(games, _self, game.value, [&](auto &a) {
            a.active = active;
        });
    }

    void house::transfer(name from, name to, asset quantity, string memo) {
        if (!check_transfer(this, from, to, quantity, memo)) {
            return;
        };

        game_index games(_self, _self.value);
        auto game_itr = games.find(from.value);
        eosio_assert(game_itr->active, "game is not active");

        // if this is from a game
        if (game_itr != games.end()) {
            // check that the token is supported and amount is within limit, update record accordingly
            token_index game_token(_self, from.value);
            auto token_iter = game_token.find(quantity.symbol.raw());
            eosio_assert(token_iter != game_token.end(), "token is not supported");
            eosio_assert(quantity.amount >= token_iter->min && quantity.amount <= token_iter->max, "Invalid amount");
            game_token.modify(token_iter, _self, [&](auto &a) {
                a.in += quantity.amount;
                a.balance += quantity.amount;
                a.play_times += 1;
            });

            // update the player table for book-keeping
            player_index game_player(_self, from.value);
            name player = name(memo);
            eosio_assert(is_account(player), "invalid player account");

            auto player_iter = game_player.find(player.value);
            if (player_iter == game_player.end()) {
                game_player.emplace(_self, [&](auto &a) {
                    a.name = player;
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
        } else {
            param_reader reader(memo);
            string target = reader.next_param("target can not be empty");

            if (target == "deposit") {
                // this is a deposit for the game, increase the balance (payout limit) automatically
                target = reader.next_param();
                uint64_t game_code = name(target).value;
                token_index game_token(_self, game_code);

                auto token_iter = game_token.find(quantity.symbol.raw());
                eosio_assert(token_iter != game_token.end(), "token is not supported");
                game_token.modify(token_iter, _self, [&](auto &a) {
                    a.balance += quantity.amount;
                });
            } else {
                eosio_assert(false, "invalid action");
            }
        }
    }

    /**
     * Pay player and referer on behalf of a game
     * @param game Name of the game
     * @param to Name of the payee
     * @param bet The amount of original bet
     * @param payout The amount to pay the user
     * @param memo Memo to include in the payment
     * @param referer Name of the referer to pay
     */
    void house::pay(name game, name to, asset bet, asset payout, string memo, name referer) {
        require_auth(game);

        // check that the game is indeed registered so we wouldn't pay for an outside contract
        game_index games(_self, _self.value);
        games.get(game.value, "Game does not exist");

        player_index game_player(_self, game.value);
        token_index game_token(_self, game.value);

        if (payout.amount > 0) {
            auto player_iter = game_player.find(to.value);
            eosio_assert(player_iter != game_player.end(), "Player does not exist");
            game_player.modify(player_iter, _self, [&](auto &a) {
                a.out += payout.amount;
            });
        }

        auto token_iter = game_token.find(payout.symbol.raw());
        eosio_assert(token_iter != game_token.end(), "Token not supported");

        bool bonus_needed = referer.value != _self.value && payout.symbol.raw() == EOS_SYMBOL.raw();
        asset refer_bonus = bonus_needed ? (bet * REFERRAL_BOUUS / 1000) : asset(0, EOS_SYMBOL);
        int64_t pay_amount = payout.amount + refer_bonus.amount;

        eosio_assert(token_iter->balance > pay_amount, "token balance depleted");
        game_token.modify(token_iter, _self, [&](auto &a) {
            a.out += pay_amount;
            a.balance -= pay_amount;
        });

        if (payout.amount > 0) {
            INLINE_ACTION_SENDER(eosio::token, transfer)(token_iter->contract, {_self, name("active")}, {_self, to, payout, memo} );
        }
        if (refer_bonus.amount > 0) {
            INLINE_ACTION_SENDER(eosio::token, transfer)(token_iter->contract, {_self, name("active")}, {_self, referer, refer_bonus, "GoDapp Referral Bonus"} );
        }
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
