#pragma once

#define MAIN_NET

#define EOS_TOKEN_CONTRACT name("eosio.token")
#define EOS_SYMBOL symbol("EOS", 4)

#ifdef MAIN_NET
    #define HOUSE_ACCOUNT name("godapphouse1")
#else
    #define HOUSE_ACCOUNT name("houseaccount")
#endif