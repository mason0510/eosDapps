#pragma once

#include <eosiolib/eosio.hpp>
#include "./constants.hpp"

#include <string>

namespace godapp {
    using namespace std;
    using namespace eosio;

    //###############    Globals  ######################
    #define DEFINE_GLOBAL_TABLE \
    TABLE globalvar { \
        uint64_t id; \
        uint64_t val; \
        uint64_t primary_key() const { return id; }; \
    }; \
    typedef multi_index<name("globals"), globalvar> global_index; \
    global_index _globals;

    template<typename T>
    void init_globals(T& globals, uint64_t start, uint64_t end) {
        for (uint64_t i=start; i<=end; i++) {
            auto iter = globals.find(i);
            if (iter == globals.end()) {
                globals.emplace(globals.get_code(), [&](auto &a) {
                    a.id = i;
                    a.val = 0;
                });
            }
        }
    }

    template<typename T>
    void set_global(T& globals, uint64_t key, uint64_t value) {
        auto iter = globals.find(key);

        globals.modify(iter, globals.get_code(), [&](auto& a) {
            a.val = value;
        });
    }

    template<typename T>
    uint64_t get_global(T& globals, uint64_t key) {
        return globals.get(key, "global value does not exist").val;
    }

    template<typename T, typename Lambda>
    uint64_t modify_global(T& globals, uint64_t key, Lambda&& updater) {
        auto iter = globals.get(key);
        auto result = iter.val;

        globals.modify(iter, globals.get_code(), updater);
        return result;
    }

    template<typename T>
    uint64_t increment_global(T& globals, uint64_t key) {
        auto iter = globals.find(key);
        auto result = iter->val + 1;

        globals.modify(iter, globals.get_code(), [&](auto& a) {
            a.val = result;
        });
        return result;
    }

    template<typename T>
    uint64_t increment_global_mod(T& globals, uint64_t key, uint64_t mod) {
        auto iter = globals.find(key);
        auto result = (iter->val + 1) % mod;

        globals.modify(iter, globals.get_code(), [&](auto& a) {
            a.val = result;
        });
        return result;
    }

    //############### TOKENS ######################
    TABLE account {
        asset balance;
        uint64_t primary_key() const { return balance.symbol.code().raw(); }
    };
    typedef multi_index<name("accounts"), account> eosio_token_account;


    asset get_token_balance(name contract, symbol symbol) {
        eosio_token_account accounts(EOS_TOKEN_CONTRACT, contract.value);
        return accounts.get(symbol.code().raw()).balance;
    }

    #define DEFINE_TOKEN_TABLE \
    TABLE token { \
        symbol sym; \
        name contract; \
        uint64_t in; \
        uint64_t out; \
        uint64_t protect; \
        uint64_t play_times; \
        uint64_t divi_time; \
        uint64_t divi_balance; \
        uint64_t min; \
        uint64_t max; \
        uint64_t primary_key() const { return sym.raw(); }; \
    }; \
    typedef multi_index<name("tokens"), token> token_index; \
    token_index _tokens;

    template<typename T>
    void init_token(T& tokens, symbol sym, name token_contract) {
        auto iter = tokens.find(sym.raw());
        if (iter == tokens.end()) {
            tokens.emplace(tokens.get_code(), [&](auto &a) {
                a.sym = sym;
                a.contract = token_contract;
                a.in = 0;
                a.out = 0;
                a.protect = 0;
                a.play_times = 0;
                a.divi_time = current_time();
                a.divi_balance = 0;
            });
        }
    }


    //###############    Transactions  ######################
    transaction get_transaction() {
        auto tx_size = transaction_size();
        char tx[tx_size];
        auto read_size = read_transaction(tx, tx_size);
        eosio_assert( tx_size == read_size, "read_transaction failed");

        return eosio::unpack<transaction>( tx, read_size );
    }

    #define EOSIO_ABI_EX( TYPE, MEMBERS ) \
    extern "C" { \
       void apply( uint64_t receiver, uint64_t code, uint64_t action ) { \
          auto self = receiver; \
          if( (code == self && action != name("transfer").value) || \
                (code == name("eosio.token").value && action == name("transfer").value) || \
                (code == name("eosio").value && action == name("onerror").value) ) { \
             switch( action ) { \
                EOSIO_DISPATCH_HELPER( TYPE, MEMBERS ) \
             } \
          } \
       } \
    }


    bool check_transfer(const contract* self, name from, name to, asset quantity, const string& memo) {
        name self_name = self->get_self();
        if (from == self_name || to != self_name) {
            return false;
        }

        if (from == name("eosio.stake")) {
            return false;
        }

        transaction trx = get_transaction();
        action first_action = trx.actions.front();
        eosio_assert(first_action.name == name("transfer") && first_action.account == self->get_code(), "wrong transaction");
        eosio_assert(quantity.is_valid(), "Invalid transfer amount.");
        eosio_assert(quantity.amount > 0, "Transfer amount not positive");
        eosio_assert(!memo.empty(), "Memo is required");

        if (memo == "deposit") {
            return false;
        }

        return true;
    }
}