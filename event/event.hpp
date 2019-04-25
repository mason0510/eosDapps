#include <eosiolib/eosio.hpp>
#include <string>
#include <vector>

#include "../common/constants.hpp"
#include "../common/random.hpp"
#include "../common/utils.hpp"
#include "../common/game_contracts.hpp"
#include "../common/contracts.hpp"

namespace godapp {
    using namespace eosio;
    using namespace std;

    CONTRACT event: public contract {
    public:
    using contract::contract;

    DEFINE_GLOBAL_TABLE

    TABLE event_table {
        uint64_t id;
        std::string event_name;
        uint64_t resolve_time;
        std::vector<uint64_t> rates;
        std::vector<uint64_t> bets;
        uint64_t payout;
        uint8_t result;
        std::string memo;
        bool active;

        uint64_t primary_key() const { return id; };
    };
    typedef eosio::multi_index<name("eventstable"), event_table> event_index;
    event_index _events;

    TABLE active_bet {
        uint64_t id;
        uint64_t game_id;
        name player;
        uint8_t bet_type;
        asset bet_asset;
        uint64_t time;

        uint64_t primary_key() const { return id; };
        uint64_t bygameid() const { return game_id; };
        uint64_t byplayer() const { return player.value; };
    };
    typedef multi_index<name("activebets"), active_bet,
        indexed_by< name("bygameid"), const_mem_fun<active_bet, uint64_t, &active_bet::bygameid> >,
        indexed_by< name("byplayer"), const_mem_fun<active_bet, uint64_t, &active_bet::byplayer> >
    > active_bet_index;
    active_bet_index _active_bets;

    event(name receiver, name code, datastream<const char *> ds):
        contract(receiver, code, ds),
        _events(_self, _self.value),
        _active_bets(_self, _self.value),
        _globals(_self, _self.value) {
    };

    ACTION init();
    ACTION setglobal(uint64_t key, uint64_t value);
    ACTION addevent(const std::string& event_name, const std::vector<uint64_t>& rates, uint64_t resolve_time);
    ACTION setactive(uint64_t id, const std::string& event_name, const std::vector<uint64_t>& rates, uint64_t resolve_time, bool active);
    ACTION transfer(name from, name to, asset quantity, string memo);
    ACTION closeevent(uint64_t id, const std::string& event_name, uint8_t result);
    ACTION resolve(uint64_t id, const std::string& event_name, uint8_t result, uint64_t payout, const std::string& memo);
    ACTION payment(uint64_t id, name player, const std::string& event_name, uint8_t result, asset bet, asset payout);
};

EOSIO_ABI_EX(event, (init)(transfer)(setglobal)(addevent)(setactive)(closeevent)(resolve)(payment))
}
