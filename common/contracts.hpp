#pragma once

#include <eosiolib/eosio.hpp>
#include <eosiolib/transaction.hpp>
#include "./constants.hpp"
#include "tables.hpp"

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
        table_upsert(globals, globals.get_code(), key, [&](auto& a) {
            a.id = key;
            a.val = value;
        });
    }

    template<typename T>
    uint64_t get_global(T& globals, uint64_t key, uint64_t default_value = 0) {
        auto iter = globals.find(key);
        if (iter == globals.end()) {
            return default_value;
        } else {
            return iter->val;
        }
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
        uint64_t result = 1;
        if (iter != globals.end()) {
            result = iter->val + 1;
            globals.modify(iter, globals.get_code(), [&](auto& a) {
                a.val = result;
            });
        } else {
            globals.emplace(globals.get_code(), [&](auto& a) {
                a.id = key;
                a.val = result;
            });
        }
        return result;
    }

    template<typename T>
    uint64_t increment_global_mod(T& globals, uint64_t key, uint64_t mod) {
        auto iter = globals.find(key);
        uint64_t result = 1;
        if (iter != globals.end()) {
            result = (iter->val + 1) % mod;
            globals.modify(iter, globals.get_code(), [&](auto& a) {
                a.val = result;
            });
        } else {
            globals.emplace(globals.get_code(), [&](auto& a) {
                a.id = key;
                a.val = result;
            });
        }
        return result;
    }

    #define DEFINE_SET_GLOBAL(NAME) \
    void NAME::setglobal(uint64_t key, uint64_t value) { \
        require_auth(_self); \
        set_global(_globals, key, value); \
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
    bool check_transfer(const contract* self, name from, name to, asset quantity, const string& memo, bool block_contract = true) {
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

        if (block_contract) {
            eosio::action act = eosio::get_action( 1, 0 );
            eosio_assert(act.name == name("transfer") && act.authorization[0].actor == from, "Contract not allowed");
        }

        return true;
    }

    /**
     * Call the house contract to make a payment to the target player
     * @param self Name of the calling contract
     * @param player Player to be paid
     * @param bet_asset Amount of the asset of the original bet
     * @param payout Amount to be paid to the player as win
     * @param referer Name of the referer
     * @param memo Memo to be used for the win message
     */
    void make_payment(name self, name player, asset bet_asset, asset payout, name referer, const string& memo) {
        transaction deal_trx;
        deal_trx.actions.emplace_back(permission_level{self, name("active") }, HOUSE_ACCOUNT, name("pay"),
                                      make_tuple(self, player, bet_asset, payout, memo, referer));
        deal_trx.delay_sec = 0;
        deal_trx.send(player.value, self);
    }

    /**
     * Perform a action under a default 1 second delay
     * @param self Name of the calling contract
     * @param action name of the action to be performed
     * @param data Data used for the action
     */
    template<typename T>
    void delayed_action(name self, name sender, name action, T&& data, uint8_t delay = 1) {
        eosio::transaction r_out;
        r_out.actions.emplace_back(eosio::permission_level{self, name("active")}, self, action, data);
        r_out.delay_sec = delay;
        r_out.send(sender.value, self);
    }

    #define DEFINE_RANDOM_KEY_TABLE \
        TABLE randkey { \
            uint64_t id; \
            capi_public_key key; \
            uint64_t primary_key() const { return id; } \
        }; \
        typedef multi_index<name("randkeys"), randkey> randkeys_index;
}