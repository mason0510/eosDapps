#pragma once

#include <eosiolib/eosio.hpp>

namespace godapp {
    using namespace eosio;

    template<typename T, typename Lambda>
    void table_upsert(T& table, const name& payer, uint64_t key, Lambda&& updater){
        auto iter = table.find(key);

        if (iter == table.end()) {
            table.emplace(payer, updater);
        } else {
            table.modify(iter, payer, updater);
        }
    }

    template<typename T, typename Lambda>
    void table_insert(T& table, const name& payer, uint64_t key, Lambda updater){
        auto iter = table.find(key);

        eosio_assert(iter == table.end(), "Item already exist");
        table.emplace(payer, updater);
    }

    template<typename T, typename Lambda>
    void table_modify(T& table, const name& payer, uint64_t key, Lambda updater){
        auto iter = table.find(key);

        eosio_assert(iter != table.end(), "item does not exist");
        table.modify(iter, payer, updater);
    }
}
