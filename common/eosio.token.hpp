/**
 *  @file
 *  @copyright defined in eos/LICENSE.txt
 */
#pragma once

#include <eosiolib/asset.hpp>
#include <eosiolib/eosio.hpp>

#include <string>

namespace eosiosystem {
   class system_contract;
}

namespace eosio {

   using std::string;

   class token : public contract {
      public:
         token(name receiver, name code, datastream<const char*> ds): contract(receiver, code, ds) {
         }
      
         void create( name issuer,
                      asset        maximum_supply);

         void issue( name to, asset quantity, string memo );

         void transfer( name from,
                        name to,
                        asset        quantity,
                        string       memo );
      
      
         inline asset get_supply( symbol sym )const;
         
         inline asset get_balance( name owner, symbol sym )const;

      private:
         struct account {
            asset    balance;

            uint64_t primary_key()const { return balance.symbol.raw(); }
         };
         
         struct currency_stat {
            asset          supply;
            asset          max_supply;
            name           issuer;

            uint64_t primary_key()const { return supply.symbol.raw(); }
         };
         
        
         typedef eosio::multi_index<name("accounts"), account> accounts;
         
         typedef eosio::multi_index<name("stat"), currency_stat> stats;

         void sub_balance( name owner, asset value );
         void add_balance( name owner, asset value, name ram_payer );

      public:
         struct transfer_args {
            name  from;
            name  to;
            asset         quantity;
            string        memo;
         };
   };

   asset token::get_supply( symbol sym )const
   {
      stats statstable( _self, sym.raw() );
      const auto& st = statstable.get( sym.raw() );
      return st.supply;
   }

   asset token::get_balance( name owner, symbol sym )const
   {
      accounts accountstable( _self, owner.value );
      const auto& ac = accountstable.get( sym.raw() );
      return ac.balance;
   }

} /// namespace eosio
