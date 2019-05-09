#define DEFINE_DISPATCHER 1

#include "../house/house.hpp"
#include "../common/tables.hpp"
#include "../common/param_reader.hpp"
#include "../common/eosio.token.hpp"
#include "../common/random.hpp"
#include "../common/game_contracts.hpp"


#define EVENT_LENGTH                86400
#define REFERRAL_BONUS              5
#define DELAYED_PAYMENT_LIMIT       1000000

#define BULLFIGHT_ID                9

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
                    a.max_payout = 2000000;
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

    void house::updategame(name game, uint64_t id) {
        require_auth(_self);

        game_index games(_self, _self.value);
        auto iter = games.find(game.value);
        if (iter != games.end()) {
            games.modify(iter, _self, [&](auto& a) {
                a.id = id;
            });
        }
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

    void house::setrandkey(capi_public_key randomness_key){
        require_auth(_self);

        randkeys_index random_keys(_self, _self.value);
        table_upsert(random_keys, _self, 0, [&](auto& k){
            k.key = randomness_key;
        });
    }

    /**
     * Receive transfer from games, and check payment status
     */
    void house::transfer(name from, name to, asset quantity, string memo) {
        if (!check_transfer(this, from, to, quantity, memo, false)) {
            return;
        };

        game_index games(_self, _self.value);
        auto game_itr = games.find(from.value);

        // if this is from a game
        if (game_itr != games.end()) {
            eosio_assert(game_itr->active, "game is not active");

            // check that the token is supported and amount is within limit, update record accordingly
            token_index game_token(_self, from.value);
            auto token_iter = game_token.find(quantity.symbol.raw());
            game_token.modify(token_iter, _self, [&](auto &a) {
                a.in += quantity.amount;
                a.balance += quantity.amount;
                a.play_times += 1;
            });

            // update the player table for book-keeping
            player_record_index game_player(_self, _self.value);
            name player = name(memo);
            eosio_assert(is_account(player), "invalid player account");
            // we only keep track of EOS cash flow
            uint64_t amount = quantity.symbol == EOS_SYMBOL ? quantity.amount : 0;

            auto player_iter = game_player.find(player.value);
            if (player_iter == game_player.end()) {
                game_player.emplace(_self, [&](auto &a) {
                    a.player = player;
                    a.in = amount;
                    a.daily_in = amount;
                    a.out = 0;
                    a.play_times = 1;

                    a.last_play_time = now();
                    a.game_played_flag |= 1 << game_itr->id;
                });
            } else {
                uint32_t timestamp = now();
                // reset event play amount if it has past event (day) boundary
                uint64_t daily_in = ((timestamp / EVENT_LENGTH) > (player_iter->last_play_time / EVENT_LENGTH)) ?
                                    0 : player_iter->daily_in;
                daily_in += amount;
                game_player.modify(player_iter, _self, [&](auto &a) {
                    a.in += amount;
                    a.play_times += 1;
                    a.last_play_time = timestamp;
                    a.daily_in = daily_in;
                    a.game_played_flag |= 1 << game_itr->id;
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
                if (token_iter != game_token.end()) {
                    game_token.modify(token_iter, _self, [&](auto &a) {
                        a.balance += quantity.amount;
                    });
                }
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
        struct game game_value = games.get(game.value, "Game does not exist");

        player_record_index game_player(_self, _self.value);
        token_index game_token(_self, game.value);

        auto token_iter = game_token.find(payout.symbol.raw());
        eosio_assert(token_iter != game_token.end(), "Token not supported");

        int64_t pay_amount = payout.amount;
        if (token_iter->balance < pay_amount || (payout.symbol == EOS_SYMBOL && (pay_amount - bet.amount) >= DELAYED_PAYMENT_LIMIT)) {
            unpaid_index delayed = unpaid_index(_self, _self.value);
            delayed.emplace(_self, [&](auto &a) {
                a.id = delayed.available_primary_key();
                a.game = game;
                a.player = to;
                a.bet = bet;
                a.payout = payout;
                a.referer = referer;
            });
        } else {
            player_record_index game_player(_self, _self.value);
            if (payout.symbol == EOS_SYMBOL) {
                if (referer.value == _self.value) {
                    referer = name(0);
                }
                auto player_iter = game_player.find(to.value);
                if (player_iter != game_player.end()) {
                    if (player_iter->referer.value != 0) {
                        referer = player_iter->referer;
                    }

                    asset refer_bonus(0, EOS_SYMBOL);
                    if (referer.value != 0) {
                        refer_bonus = bet * REFERRAL_BONUS / 1000;
                        if (game_value.id == BULLFIGHT_ID) {
                            refer_bonus /= 5;
                        }
                        pay_amount += refer_bonus.amount;
                        if (refer_bonus.amount > 0) {
                            INLINE_ACTION_SENDER(eosio::token, transfer)(token_iter->contract, {_self, name("active")}, {_self, referer, refer_bonus, "Dapp365 Referral Bonus"} );
                        }
                    }

                    game_player.modify(player_iter, _self, [&](auto &a) {
                        a.out += payout.amount;
                        a.referer = referer;
                        a.referer_payout += refer_bonus.amount;
                    });
                }
            }

            game_token.modify(token_iter, _self, [&](auto &a) {
                a.out += pay_amount;
                a.balance -= pay_amount;
            });

            if (payout.amount > 0) {
                INLINE_ACTION_SENDER(eosio::token, transfer)(token_iter->contract, {_self, name("active")}, {_self, to, payout, memo} );
            }
        }
    }

    void house::settleunpaid(uint64_t id, bool pay) {
        require_auth(_self);

        unpaid_index delayed = unpaid_index(_self, _self.value);
        auto itr = delayed.find(id);
        eosio_assert(itr != delayed.end(), "Unpaid record does not exist");

        if (pay) {
            name player = itr->player;
            asset payout = itr->payout;

            player_record_index game_player(_self, _self.value);
            auto player_iter = game_player.find(player.value);
            game_player.modify(player_iter, _self, [&](auto &a) {
                a.out += payout.amount;
            });

            token_index game_token(_self, itr->game.value);
            auto token_iter = game_token.find(payout.symbol.raw());
            eosio_assert(token_iter != game_token.end(), "Token not supported");
            game_token.modify(token_iter, _self, [&](auto &a) {
                a.out = payout.amount;
                a.balance -= payout.amount;
            });

            if (payout.amount > 0) {
                INLINE_ACTION_SENDER(eosio::token, transfer)(token_iter->contract, {_self, name("active")},
                    {_self, player, payout, "Dapp365 settle payment"} );
            }
        }
        delayed.erase(itr);
    }

    void house::updatetoken(name game, symbol token, name contract, uint64_t min, uint64_t max_payout, uint64_t balance) {
        require_auth(_self);
        token_index game_token(_self, game.value);

        table_upsert(game_token, _self, token.raw(), [&](auto &a) {
            a.sym = token;
            a.contract = contract;
            a.min = min;
            a.max_payout = max_payout;
            a.balance = balance;
        });
    }

    void house::cleartoken(name game) {
        require_auth(_self);
        token_index game_token(_self, game.value);
        for (auto iter = game_token.begin(); iter != game_token.end();) {
            if (iter->sym.code() != EOS_SYMBOL.code()) {
                iter = game_token.erase(iter);
            } else {
                iter++;
            }
        }
    }

    void house::setreferer(name player, name referer) {
        require_auth(player);

        player_record_index game_player(_self, _self.value);
        auto itr = game_player.find(player.value);
        eosio_assert(referer.value != player.value && referer.value != _self.value, "Invalid Player Name");
        if (itr != game_player.end()) {
            if (referer.value != itr->referer.value) {
                game_player.modify(itr, _self, [&](auto &a) {
                    a.referer = referer;
                    a.referer_payout = 0;
                });
            }
        } else {
            game_player.emplace(_self, [&](auto &a) {
                a.player = player;
                a.referer = referer;
            });
        }
    }


#define SET_REFERER             0
#define PLAY_ANY_GAME           1
#define PLAY_ALL_GAMES          2
#define TOTAL_PLAYED_START      20
#define TOTAL_PLAYED_END        27

#define CHEST_OPEN_END          4

    struct played_reward {
        uint64_t amount;
        uint64_t points;
    };

    played_reward rewards[] = {
        {10, 10}, {100, 100}, {500, 300}, {1000, 500}, {2000, 1000}, {5000, 3000},
        {10000, 5000}, {100000, 10000}
    };

    void house::claimreward(name player, uint8_t reward_type) {
        require_auth(player);
        uint64_t reward_mask = ((uint32_t) 1) << reward_type;

        player_record_index game_player(_self, _self.value);
        auto itr = game_player.find(player.value);
        eosio_assert(itr != game_player.end(), "Player does not exist");
        eosio_assert((itr->bonus_claimed & reward_mask) == 0, "reward already claimed");

        uint64_t points = 0;
        switch (reward_type) {
            case SET_REFERER:
                if (itr->referer.value != 0) {
                    points = 50;
                }
                break;
            case PLAY_ANY_GAME:
                if (itr->game_played_flag != 0) {
                    points = 20;
                }
                break;
            case PLAY_ALL_GAMES:
                if ((itr->game_played_flag & 1022) == 1022) {
                    points = 20;
                }
                break;
            default:
                if (reward_type >= TOTAL_PLAYED_START && reward_type <= TOTAL_PLAYED_END) {
                    const played_reward& reward = rewards[reward_type - TOTAL_PLAYED_START];
                    if (itr->in >= reward.amount * 10000) {
                        points = reward.points;
                    }
                }
        }

        game_player.modify(itr, _self, [&](auto &a) {
            a.bonus_point += points;
            a.bonus_claimed |= reward_mask;
        });
    }

    struct chest_reward {
        uint64_t amount;
        uint64_t limit;
    };

    chest_reward chest_amount[] = {
        {500, 100}, {5000, 500}, {10000, 1000}, {20000, 5000}, {100000, 20000}
    };

    void house::openchest(name player, uint8_t chest_type) {
        require_auth(player);

        uint64_t reward_mask = ((uint32_t) 1) << chest_type;
        eosio_assert(chest_type <= CHEST_OPEN_END, "Invalid Chest type");
        chest_reward reward_value = chest_amount[chest_type];
        asset reward(reward_value.amount, EOS_SYMBOL);

        player_record_index game_player(_self, _self.value);
        auto itr = game_player.find(player.value);
        eosio_assert(itr != game_player.end(), "Player does not exist");
        eosio_assert(itr->bonus_point >= reward_value.limit, "Not enough bonus point");
        eosio_assert((itr->chest_opened & reward_mask) == 0, "Reward already claimed");

        random rng(create_seed(0, player.value));
        uint64_t roll = rng.generator(101);
        reward += reward * roll / 100;

        game_player.modify(itr, _self, [&](auto &a) {
            a.out += reward.amount;
            a.bonus_payout += reward.amount;
            a.chest_opened |= reward_mask;
        });

        INLINE_ACTION_SENDER(eosio::token, transfer)(EOS_TOKEN_CONTRACT,
            {_self, name("active")}, {_self, player, reward, "Dapp365 Chest Reward"} );
    }
}
