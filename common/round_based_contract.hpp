#include <eosiolib/eosio.hpp>
#include "contracts.hpp"

#define GAME_STATUS_STANDBY         1
#define GAME_STATUS_ACTIVE          2

#define DEFINE_GAMES_TABLE(TYPE, LEFT, RIGHT)  \
        TABLE game { \
            uint64_t id; \
            uint64_t end_time; \
            symbol symbol; \
            TYPE LEFT; \
            TYPE RIGHT; \
            uint8_t status; \
            uint64_t primary_key() const { return symbol.raw(); } \
            uint64_t byid()const {return id;} \
        }; \
        typedef multi_index<name("games"), game, \
            indexed_by< name("byid"), const_mem_fun<game, uint64_t, &game::byid> > \
        > games_table; \
        games_table _games;


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
private: \
        void initsymbol(symbol sym); \
        void bet(name player, name referer, uint64_t game_id, uint8_t bet_type, asset amount); \


#define STANDARD_ACTIONS (init)(reveal)(play)(setglobal)(hardclose)

#define DEFINE_STANDARD_FUNCTIONS(NAME) \
        DEFINE_INIT_FUNCTION(NAME) \
        DEFINE_SET_GLOBAL(NAME) \
        DEFINE_INIT_SYMBOL_FUNCTION(NAME) \
        DEFINE_PLAY_FUNCTION(NAME) \
        DEFINE_HARDCLOSE_FUNCTION(NAME)


#define DEFINE_INIT_FUNCTION(NAME) \
    void NAME::init() { \
        require_auth(_self); \
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

#define DEFINE_PLAY_FUNCTION(NAME) \
    void NAME::play(name player, asset bet, string memo) { \
        param_reader reader(memo); \
        auto game_id = (uint64_t) atol( reader.next_param("Game ID cannot be empty!").c_str() ); \
        auto bet_type = (uint8_t) atoi( reader.next_param("Bet type cannot be empty!").c_str() ); \
        name referer = reader.get_referer(player); \
        auto game_iter = _games.find(bet.symbol.raw()); \
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
                close_trx.send(player.value, _self); \
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
            a.player = player; \
            a.referer = referer; \
            a.bet = bet; \
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

#define DEFINE_PAYMENT_BLOCK(NAME) \
        auto bet_index = _bets.get_index<name("bygameid")>(); \
        char buff[128]; \
        sprintf(buff, "[GoDapp] " NAME " game win!"); \
        string msg(buff); \
        for (auto itr = bet_index.begin(); itr != bet_index.end();) { \
            uint8_t bet_type = itr->bet_type; \
            if (bet_type & result) { \
                asset payout(itr->bet.amount * payout_rate(bet_type), itr->bet.symbol); \
                transaction trx; \
                trx.actions.emplace_back(permission_level{ _self, name("active") }, HOUSE_ACCOUNT, name("pay"), \
                        make_tuple(_self, itr->player, itr->bet, payout, msg, itr->referer)); \
                trx.delay_sec = 1; \
                trx.send(_self.value, _self); \
            } \
            itr = bet_index.erase(itr); \
        }