#pragma once

#include <eosiolib/eosio.hpp>
#include <string>
#include <vector>

#include "../common/constants.hpp"
#include "../common/random.hpp"
#include "../common/utils.hpp"
#include "../common/contracts.hpp"

namespace godapp {
    using namespace eosio;
    using namespace std;

    CONTRACT dice: public contract {
    public:
        using contract::contract;

        DEFINE_GLOBAL_TABLE(globals)
        DEFINE_TOKEN_TABLE(tokens)

        TABLE bet
        {
            uint64_t id;
            uint64_t bet_id;
            name contract;
            name bettor;
            name inviter;
            uint64_t bet_amt;
            vector<asset> payout;
            uint8_t roll_type;
            uint64_t roll_border;
            uint64_t roll_value;
            capi_checksum256 seed;
            time_point_sec time;
            uint64_t primary_key() const { return id; };
        };
        typedef eosio::multi_index<name("activebets"), bet> bet_index;

        ACTION init();
        ACTION setactive(bool active);
        ACTION start(name bettor, asset bet_asset, uint8_t roll_type, uint8_t bet_number, name referrer);
        ACTION resolve(name bettor, asset bet_asset, uint8_t roll_type, uint8_t bet_number, name referrer);
        ACTION receipt(uint64_t bet_id, name bettor, asset bet_amt, vector<asset> payout_list,
                capi_checksum256 seed, uint8_t roll_type, uint64_t roll_border, uint64_t roll_value);
        ACTION transfer(name from, name to, asset quantity, string memo);

        dice(name receiver, name code, datastream<const char*> ds): contract(receiver, code, ds) {}
    private:
        void save_bet(
                uint64_t bet_id, name bettor, name inviter,
                asset bet_quantity, const vector<asset>& payout_list, uint8_t roll_type,
                uint64_t roll_border, uint64_t roll_value, capi_checksum256 seed, time_point_sec time);
    };

    EOSIO_ABI_EX(dice, (init)(setactive)(start)(resolve)(receipt)(transfer))
}
