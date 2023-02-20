#include <eosiolib/eosio.hpp>
#include <eosiolib/print.hpp>
#include "contracts.hpp"
#include "game_contracts.hpp"

#define GAME_STATUS_STANDBY         1
#define GAME_STATUS_ACTIVE          2
#define GAME_REVEAL_PRESET          5

#define DEFINE_GAMES_TABLE(GAME_DATA)  \
        TABLE game { \
            uint64_t id; \
            uint64_t end_time; \
            GAME_DATA \
            symbol symbol; \
            uint8_t status; \
            name largest_winner; \
            asset largest_win_amount; \
            capi_checksum256 seed; \
            uint64_t primary_key() const { return symbol.raw(); } \
            uint64_t byid() const {return id;} \
        }; \
        typedef multi_index<name("activegame"), game, \
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

#define DEFINE_HISTORY_TABLE \
        TABLE history { \
            uint64_t id; \
            uint64_t history_id; \
            name player; \
            asset bet; \
            uint8_t bet_type; \
            asset payout; \
            uint64_t close_time; \
            uint64_t result; \
            uint64_t primary_key() const { return id; } \
        }; \
        typedef multi_index<name("histories"), history> history_table; \

#define DEFINE_RESULTS_TABLE \
        TABLE result { \
            uint64_t id; \
            uint64_t game_id; \
            uint8_t result; \
            uint64_t primary_key() const { return id; } \
        }; \
        typedef multi_index<name("results"), result> result_table; \
        result_table _results;

#define DECLARE_STANDARD_ACTIONS(NAME) \
public: \
        NAME(name receiver, name code, datastream<const char*> ds); \
        ACTION init(); \
        ACTION setglobal(uint64_t key, uint64_t value); \
        ACTION reveal(uint64_t game_id, capi_signature signature); \
        ACTION newround(symbol symbol_type); \
        ACTION hardclose(uint64_t game_id); \
        ACTION transfer(name from, name to, asset quantity, string memo); \
private: \
        void initsymbol(symbol sym); \
        void doReveal(uint64_t game_id, random& random);

#define STANDARD_ACTIONS (init)(reveal)(transfer)(newround)(setglobal)(hardclose)

#define DEFINE_CONSTRUCTOR(NAME) \
    NAME::NAME(name receiver, name code, datastream<const char*> ds): \
        contract(receiver, code, ds), \
        _globals(_self, _self.value), \
        _games(_self, _self.value), \
        _bets(_self, _self.value), \
        _results(_self, _self.value), \
        _bet_amount(_self, _self.value) { \
    }

#define DEFINE_INIT_FUNCTION(NAME) \
    void NAME::init() { \
        require_auth(HOUSE_ACCOUNT); \
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
            a.end_time = timestamp + GAME_LENGTH; \
            a.seed = create_seed(_self.value, a.id); \
            a.status = GAME_STATUS_ACTIVE; \
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

#define DEFINE_TRANSFER_FUNCTION(NAME) \
    void NAME::transfer(name from, name to, asset quantity, string memo) { \
        if (!check_transfer(this, from, to, quantity, memo)) { \
            return; \
        }; \
        asset total_bet = quantity; \
        auto bet_iter = _bet_amount.find(from.value); \
        if (bet_iter != _bet_amount.end()) { \
            total_bet += bet_iter->bet; \
        } \
        table_upsert(_bet_amount, _self, from.value, [&](auto &a) { \
            a.player = from; \
            a.bet = total_bet; \
        }); \
        transfer_to_house(_self, quantity, from, total_bet.amount); \
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
                eosio_assert((timestamp + GAME_REVEAL_PRESET) < game_iter->end_time, "Game already finished, please wait for next round"); \
                break; \
            default: \
                eosio_assert(false, "Invalid game state"); \
        } \
        asset total = asset(0, quantity.symbol); \
        while(reader.has_next()) { \
            uint8_t bet_type = (uint8_t) atoi( reader.next_param("Bet type cannot be empty!").c_str() ); \
            uint64_t amount = (uint64_t) atoi( reader.next_param("Bet amount cannot be empty!").c_str() ); \
            asset bet_amount(amount, quantity.symbol); \
            eosio_assert(bet_amount.amount > 0, "Bet amount must be positive"); \
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

#define DEFINE_REVEAL_FUNCTION(NAME, DISPLAYNAME, RESULT, REFERRAL_FACTOR) \
    void NAME::reveal(uint64_t game_id, capi_signature signature) { \
        auto idx = _games.get_index<name("byid")>(); \
        auto gm_pos = idx.find(game_id); \
        uint32_t timestamp = now(); \
        \
        eosio_assert(gm_pos != idx.end() && gm_pos->id == game_id, "reveal: game id does't exist!"); \
        eosio_assert(gm_pos->status == GAME_STATUS_ACTIVE && (timestamp + GAME_REVEAL_PRESET) >= gm_pos->end_time, "Can not reveal yet"); \
        randkeys_index random_keys(HOUSE_ACCOUNT, HOUSE_ACCOUNT.value); \
        capi_public_key random_key = random_keys.get(0, "random key is not set").key; \
        random random_gen = random_from_sig(random_key, gm_pos->seed, signature); \
        doReveal(game_id, random_gen); \
        delayed_action(_self, _self, name("newround"), make_tuple(gm_pos->symbol), GAME_RESOLVE_TIME); \
    } \
    \
    void NAME::doReveal(uint64_t game_id, random &random_gen) { \
        auto idx = _games.get_index<name("byid")>(); \
        auto gm_pos = idx.find(game_id); \
        uint32_t timestamp = now(); \
        RESULT result(random_gen); \
        auto bet_index = _bets.get_index<name("bygameid")>(); \
        map<uint64_t, pay_result> result_map; \
        for (auto itr = bet_index.begin(); itr != bet_index.end(); itr++) { \
            uint8_t bet_type = itr->bet_type; \
            asset bet = itr->bet; \
            asset payout = result.get_payout(*itr); \
            auto result_itr = result_map.find(itr->player.value); \
            if (result_itr == result_map.end()) { \
                result_map[itr->player.value] = pay_result{bet, payout, itr->referer}; \
            } else { \
                result_itr->second.bet += bet; \
                result_itr->second.payout += payout; \
            } \
        } \
        history_table history(_self, _self.value); \
        uint64_t history_id = get_global(_globals, G_ID_HISTORY_ID); \
        for (auto itr = bet_index.begin(); itr != bet_index.end();) { \
            uint8_t bet_type = itr->bet_type; \
            asset bet = itr->bet; \
            asset payout = result.get_payout(*itr); \
            history_id++; \
            uint64_t id = history_id % HISTORY_SIZE; \
            table_upsert(history, _self, id, [&](auto &a) { \
                a.id = id; \
                a.history_id = history_id; \
                a.player = itr->player; \
                a.bet = bet; \
                a.result = result.result; \
                a.bet_type = bet_type; \
                a.payout = payout; \
                a.close_time = timestamp; \
            }); \
            itr = bet_index.erase(itr); \
        } \
        set_global(_globals, G_ID_HISTORY_ID, history_id); \
        auto largest_winner = result_map.end(); \
        int64_t win_amount = 0; \
        for (auto itr = result_map.begin(); itr != result_map.end(); itr++) { \
            auto current = itr->second; \
            if (current.payout.amount > current.bet.amount && \
                current.payout.amount > win_amount && \
                current.payout.symbol == EOS_SYMBOL) { \
                largest_winner = itr; \
                win_amount = current.payout.amount; \
            } \
            make_payment(_self, name(itr->first), current.bet / REFERRAL_FACTOR, current.payout, current.referer, \
                         current.payout.amount >= current.bet.amount ? \
                        "[Dapp365] " #DISPLAYNAME " win!" : "[Dapp365] " #DISPLAYNAME " lose!" ); \
        } \
        uint64_t next_game_id = increment_global(_globals, G_ID_GAME_ID); \
        name winner_name = largest_winner == result_map.end() ? name() : name(largest_winner->first); \
        idx.modify(gm_pos, _self, [&](auto &a) { \
            a.id = next_game_id; \
            a.status = GAME_STATUS_STANDBY; \
            a.end_time = timestamp + GAME_RESOLVE_TIME; \
            a.largest_winner = winner_name; \
            a.largest_win_amount = asset(win_amount, EOS_SYMBOL); \
            result.update_game(a); \
        }); \
        for (auto itr = _bet_amount.begin(); itr != _bet_amount.end();) { \
            itr = _bet_amount.erase(itr); \
        } \
        uint64_t result_index = increment_global_mod(_globals, G_ID_RESULT_ID, RESULT_SIZE); \
        table_upsert(_results, _self, result_index, [&](auto &a) { \
            a.id = result_index; \
            a.game_id = game_id; \
            a.result = result.roundResult; \
        }); \
        result.set_receipt(*this, game_id, gm_pos->seed); \
    }

#define DEFINE_STANDARD_FUNCTIONS(NAME) \
        DEFINE_CONSTRUCTOR(NAME) \
        DEFINE_INIT_FUNCTION(NAME) \
        DEFINE_SET_GLOBAL(NAME) \
        DEFINE_NEW_ROUND_FUNCTION(NAME) \
        DEFINE_INIT_SYMBOL_FUNCTION(NAME) \
        DEFINE_HARDCLOSE_FUNCTION(NAME) \
        DEFINE_TRANSFER_FUNCTION(NAME)