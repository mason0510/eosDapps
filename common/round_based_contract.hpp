#include <eosiolib/eosio.hpp>
#include "contracts.hpp"

#define GAME_STATUS_STANDBY         1
#define GAME_STATUS_ACTIVE          2

#define DEFINE_GAMES_TABLE(TYPE, LEFT, RIGHT)  \
        TABLE game { \
            uint64_t id; \
            uint64_t end_time; \
            TYPE LEFT; \
            TYPE RIGHT; \
            symbol symbol; \
            uint8_t status; \
            name largest_winner; \
            asset largest_win_amount; \
            uint64_t primary_key() const { return symbol.raw(); } \
            uint64_t byid()const {return id;} \
        }; \
        typedef multi_index<name("gametable"), game, \
            indexed_by< name("byid"), const_mem_fun<game, uint64_t, &game::byid> > \
        > games_table; \
        games_table _games; \
        \
        struct pay_result { \
            asset bet; \
            asset payout; \
            name referer; \
        };


#define DEFINE_BETS_TABLE \
        TABLE bet { \
            uint64_t id; \
            uint64_t game_id; \
            name player; \
            name referer; \
            asset bet; \
            uint8_t bet_type; \
            uint64_t primary_key() const { return id; } \
            uint64_t bygameid() const {return game_id;} \
        }; \
        typedef multi_index<name("bets"), bet, \
                indexed_by< name("bygameid"), const_mem_fun<bet, uint64_t, &bet::bygameid> > \
        > bet_table; \
        bet_table _bets;

#define DEFINE_RESULTS_TABLE \
        TABLE result { \
            uint64_t id; \
            uint64_t game_id; \
            uint8_t result; \
            uint64_t primary_key() const { return id; } \
        }; \
        typedef multi_index<name("results"), result> result_table; \
        result_table _results;

#define DEFINE_STANDARD_ACTIONS(NAME) \
public: \
        NAME(name receiver, name code, datastream<const char*> ds); \
        ACTION init(); \
        ACTION setglobal(uint64_t key, uint64_t value); \
        ACTION play(name player, asset bet, string memo); \
        ACTION reveal(uint64_t game_id); \
        ACTION hardclose(uint64_t game_id); \
        ACTION transfer(name from, name to, asset quantity, string memo); \
private: \
        void initsymbol(symbol sym); \
        void bet(name player, name referer, uint64_t game_id, uint8_t bet_type, asset amount); \


#define STANDARD_ACTIONS (init)(reveal)(transfer)(setglobal)(hardclose)

#define DEFINE_STANDARD_FUNCTIONS(NAME) \
        DEFINE_INIT_FUNCTION(NAME) \
        DEFINE_SET_GLOBAL(NAME) \
        DEFINE_INIT_SYMBOL_FUNCTION(NAME) \
        DEFINE_TRANSFER_FUNCTION(NAME) \
        DEFINE_HARDCLOSE_FUNCTION(NAME)


#define DEFINE_INIT_FUNCTION(NAME) \
    void NAME::init() { \
        require_auth(HOUSE_ACCOUNT); \
        init_globals(_globals, G_ID_START, G_ID_END); \
        initsymbol(EOS_SYMBOL); \
    }

#define DEFINE_SET_GLOBAL(NAME) \
    void NAME::setglobal(uint64_t key, uint64_t value) { \
        require_auth(_self); \
        set_global(_globals, key, value); \
    }

#define DEFINE_INIT_SYMBOL_FUNCTION(NAME) \
    void NAME::initsymbol(symbol sym) { \
        auto iter = _games.find(sym.raw()); \
        if (iter == _games.end()) { \
            uint64_t next_id = increment_global(_globals, G_ID_GAME_ID); \
            _games.emplace(_self, [&](auto &a) { \
                a.id = next_id; \
                a.symbol = sym; \
                a.status = GAME_STATUS_STANDBY; \
            }); \
        } \
    }

#define DEFINE_TRANSFER_FUNCTION(NAME) \
     void NAME::transfer(name from, name to, asset quantity, string memo) { \
        if (!check_transfer(this, from, to, quantity, memo)) { \
                return; \
        }; \
        INLINE_ACTION_SENDER(eosio::token, transfer)(EOS_TOKEN_CONTRACT, {_self, name("active")}, \
            {_self, HOUSE_ACCOUNT, quantity, from.to_string()}); \
        param_reader reader(memo); \
        auto game_id = (uint64_t) atol( reader.next_param("Game ID cannot be empty!").c_str() ); \
        auto bet_type = (uint8_t) atoi( reader.next_param("Bet type cannot be empty!").c_str() ); \
        name referer = reader.get_referer(from); \
        auto game_iter = _games.find(quantity.symbol.raw()); \
        eosio_assert(game_iter->id == game_id, "Game is no longer active"); \
        uint32_t timestamp = now(); \
        uint8_t status = game_iter->status; \
        switch (status) { \
            case GAME_STATUS_STANDBY: { \
                eosio_assert(timestamp >= game_iter->end_time, "Game resolving, please wait"); \
                _games.modify(game_iter, _self, [&](auto &a) { \
                    a.end_time = now() + GAME_LENGTH; \
                    a.status = GAME_STATUS_ACTIVE; \
                }); \
                transaction close_trx; \
                close_trx.actions.emplace_back(permission_level{ _self, name("active") }, _self, name("reveal"), make_tuple(game_id)); \
                close_trx.delay_sec = GAME_LENGTH; \
                close_trx.send(from.value, _self); \
                break; \
            } \
            case GAME_STATUS_ACTIVE: \
                eosio_assert(timestamp < game_iter->end_time, "Game already finished, please wait for next round"); \
                break; \
            default: \
                eosio_assert(false, "Invalid game state"); \
        } \
        uint64_t next_bet_id = increment_global(_globals, G_ID_BET_ID); \
        _bets.emplace(_self, [&](auto &a) { \
            a.id = next_bet_id; \
            a.game_id = game_id; \
            a.player = from; \
            a.referer = referer; \
            a.bet = quantity; \
            a.bet_type = bet_type; \
        }); \
    }

#define DEFINE_HARDCLOSE_FUNCTION(NAME) \
    void NAME::hardclose(uint64_t game_id) { \
        require_auth(_self); \
        auto idx = _games.get_index<name("byid")>(); \
        auto gm_pos = idx.find(game_id); \
        uint64_t next_game_id = increment_global(_globals, G_ID_GAME_ID); \
        idx.modify(gm_pos, _self, [&](auto &a) { \
            a.id = next_game_id; \
            a.status = GAME_STATUS_STANDBY; \
            a.end_time = now(); \
        }); \
    }

#define DEFINE_FINALIZE_BLOCK(NAME, LEFT, RIGHT) \
        map<uint64_t, pay_result> result_map; \
        auto bet_index = _bets.get_index<name("bygameid")>(); \
        for (auto itr = bet_index.begin(); itr != bet_index.end();) { \
            uint8_t bet_type = itr->bet_type; \
            if (bet_type & result) { \
                asset bet = itr->bet; \
                asset payout = bet * payout_rate(bet_type); \
                auto result_itr = result_map.find(itr->player.value); \
                if (result_itr == result_map.end()) { \
                    result_map[itr->player.value] = pay_result{bet, payout, itr->referer}; \
                } else { \
                    result_itr->second.bet += bet; \
                    result_itr->second.payout += payout; \
                } \
            } \
            itr = bet_index.erase(itr); \
        } \
        char buff[128]; \
        string msg(buff); \
        sprintf(buff, "[GoDapp] " NAME " game win!"); \
        auto largest_winner = result_map.end(); \
        int64_t win_amount = 0; \
        for (auto itr = result_map.begin(); itr != result_map.end(); itr++) { \
            auto current = itr->second; \
            if (current.payout.amount > win_amount && current.payout.symbol == EOS_SYMBOL) { \
                largest_winner = itr; \
                win_amount = current.payout.amount; \
            } \
            INLINE_ACTION_SENDER(house, pay)(HOUSE_ACCOUNT, {_self, name("active")}, \
                {_self, name(itr->first), current.bet, current.payout, msg, current.referer} ); \
            } \
            uint64_t next_game_id = increment_global(_globals, G_ID_GAME_ID); \
            name winner_name = largest_winner == result_map.end() ? name() : name(largest_winner->first); \
            idx.modify(gm_pos, _self, [&](auto &a) { \
                a.id = next_game_id; \
                a.status = GAME_STATUS_STANDBY; \
                a.end_time = timestamp + GAME_RESOLVE_TIME; \
                a.largest_winner = winner_name; \
                a.largest_win_amount = asset(win_amount, EOS_SYMBOL); \
                a.LEFT = LEFT; \
                a.RIGHT = RIGHT; \
            }); \
            uint64_t result_index = increment_global_mod(_globals, G_ID_RESULT_ID, RESULT_SIZE); \
            table_upsert(_results, _self, result_index, [&](auto &a) { \
                a.id = result_index; \
                a.game_id = game_id; \
                a.result = result; \
            });