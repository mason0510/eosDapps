#pragma once

#include <eosiolib/eosio.hpp>
#include <eosiolib/transaction.hpp>
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

    //###############    Transactions  ######################
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


    /**
     * Check a transfer against normal issues
     * @param self A pointer for the calling contract
     * @param from The sender
     * @param to The receiver
     * @param quantity The amount to be transfered
     * @param memo Memo used for transfer
     * @return True if the transaction should be further processed, false otherwise
     */
    bool check_transfer(const contract* self, name from, name to, asset quantity, const string& memo) {
        if (from == self->get_self() || to != self->get_self()) {
            return false;
        }
        if (from == name("eosio.stake")) {
            return false;
        }
        require_auth(from);

        eosio_assert(quantity.is_valid(), "Invalid transfer amount.");
        eosio_assert(quantity.amount > 0, "Transfer amount not positive");
        eosio_assert(!memo.empty(), "Memo is required");

        return true;
    }
}