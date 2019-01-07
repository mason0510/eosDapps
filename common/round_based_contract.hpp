#include <eosiolib/eosio.hpp>
#include <eosiolib/print.hpp>
#include "contracts.hpp"
#include "game_contracts.hpp"

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
        ACTION resolve(uint64_t game_id); \
        ACTION reveal(uint64_t game_id); \
        ACTION newround(symbol symbol_type); \
        ACTION hardclose(uint64_t game_id); \
        ACTION transfer(name from, name to, asset quantity, string memo); \
private: \
        void initsymbol(symbol sym); \
        void bet(name player, name referer, uint64_t game_id, uint8_t bet_type, asset amount); \


#define STANDARD_ACTIONS (init)(reveal)(transfer)(resolve)(newround)(setglobal)(hardclose)


#define DEFINE_INIT_FUNCTION(NAME) \
    void NAME::init() { \
        require_auth(HOUSE_ACCOUNT); \
        init_globals(_globals, G_ID_START, G_ID_END); \
        initsymbol(EOS_SYMBOL); \
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

#define DEFINE_NEW_ROUND_FUNCTION(NAME) \
    void NAME::newround(symbol symbol_type) { \
        require_auth(_self); \
        uint32_t timestamp = now(); \
        auto game_iter = _games.find(symbol_type.raw()); \
        eosio_assert(game_iter->status == GAME_STATUS_STANDBY, "Round already started"); \
        eosio_assert(timestamp >= game_iter->end_time, "Game resolving, please wait"); \
        _games.modify(game_iter, _self, [&](auto &a) { \
            a.end_time = now() + GAME_LENGTH; \
            a.status = GAME_STATUS_ACTIVE; \
        }); \
        delayed_action(_self, _self, name("resolve"), make_tuple(game_iter->id), GAME_LENGTH); \
    }

#define DEFINE_RESOLVE_FUNCTION(NAME) \
    void NAME::resolve(uint64_t game_id) { \
        require_auth(_self); \
        delayed_action(_self, _self, name("reveal"), make_tuple(game_id)); \
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

#define DEFINE_TRANSFER_FUNCTION(NAME) \
    void NAME::transfer(name from, name to, asset quantity, string memo) { \
        if (!check_transfer(this, from, to, quantity, memo)) { \
            return; \
        }; \
        transfer_to_house(_self, quantity, from, quantity.amount); \
        param_reader reader(memo); \
        auto game_id = (uint64_t) atol( reader.next_param("Game ID cannot be empty!").c_str() ); \
        auto referer = reader.get_referer(from); \
        auto game_iter = _games.find(quantity.symbol.raw()); \
        eosio_assert(game_iter->id == game_id, "Game is no longer active"); \
        uint32_t timestamp = now(); \
        uint8_t status = game_iter->status; \
        switch (status) { \
            case GAME_STATUS_STANDBY: { \
                SEND_INLINE_ACTION(*this, newround, {_self, name("active")}, {quantity.symbol}); \
                break; \
            } \
            case GAME_STATUS_ACTIVE: \
                eosio_assert(timestamp < game_iter->end_time, "Game already finished, please wait for next round"); \
                break; \
            default: \
                eosio_assert(false, "Invalid game state"); \
        } \
        asset total = asset(0, quantity.symbol); \
        while(reader.has_next()) { \
            auto bet_type = (uint8_t) atoi( reader.next_param("Bet type cannot be empty!").c_str() ); \
            auto amount = (uint64_t) atoi( reader.next_param("Bet amount cannot be empty!").c_str() ); \
            asset bet_amount(amount, quantity.symbol); \
            total += bet_amount; \
            uint64_t next_bet_id = increment_global(_globals, G_ID_BET_ID); \
            _bets.emplace(_self, [&](auto &a) { \
                a.id = next_bet_id; \
                a.game_id = game_id; \
                a.player = from; \
                a.referer = referer; \
                a.bet = bet_amount; \
                a.bet_type = bet_type; \
            }); \
        } \
        eosio_assert(quantity == total, "bet amount does not match transfer amount"); \
    } \



#define DEFINE_STANDARD_FUNCTIONS(NAME) \
        DEFINE_INIT_FUNCTION(NAME) \
        DEFINE_SET_GLOBAL(NAME) \
        DEFINE_RESOLVE_FUNCTION(NAME) \
        DEFINE_NEW_ROUND_FUNCTION(NAME) \
        DEFINE_INIT_SYMBOL_FUNCTION(NAME) \
        DEFINE_HARDCLOSE_FUNCTION(NAME) \
        DEFINE_TRANSFER_FUNCTION(NAME)