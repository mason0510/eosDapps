#pragma once
#include <eosiolib/eosio.hpp>
#include <eosiolib/crypto.h>
#include <eosiolib/transaction.hpp>

/**
 * Random number generator picked from EOS.WIN
 */
namespace godapp {
    class random {
    public:
        template<class T>
        struct data {
            T content;
            int block;
            int prefix;

            data(T t) {
                content = t;
                block = tapos_block_num();
                prefix = tapos_block_prefix();
            }
        };

        struct st_seeds {
            capi_checksum256 seed1;
            capi_checksum256 seed2;
        };

    public:
        random(uint64_t mixed = 0);
        random(capi_checksum256 seed);
        ~random();

        void seed(capi_checksum256 sseed, capi_checksum256 useed);
        void mixseed(capi_checksum256 &sseed, capi_checksum256 &useed, capi_checksum256 &result) const;

        // generator number ranged [0, max-1]
        uint64_t generator(uint64_t max = 101);

        uint64_t gen(capi_checksum256 &seed, uint64_t max = 101) const;

    private:
        capi_checksum256 _mixed;
        capi_checksum256 _seed;
    };

    random::random(capi_checksum256 seed) {
        _seed = seed;
        _mixed = seed;
    }

    random::~random() {}

    void random::seed(capi_checksum256 sseed, capi_checksum256 useed) {
        mixseed(sseed, useed, _mixed);
        _seed = _mixed;
    }


    void random::mixseed(capi_checksum256 &sseed, capi_checksum256 &useed, capi_checksum256 &result) const {
        st_seeds seeds;
        seeds.seed1 = sseed;
        seeds.seed2 = useed;
        sha256((char *) &seeds.seed1, sizeof(seeds.seed1) * 2, &result);
    }

    uint64_t random::generator(uint64_t max) {
        mixseed(_mixed, _seed, _seed);

        uint64_t r = gen(_seed, max);

        return r;
    }

    uint64_t random::gen(capi_checksum256 &seed, uint64_t max) const {
        const uint64_t *p64 = reinterpret_cast<const uint64_t *>(&seed);
        return max <= 0 ? p64[1] : p64[1] % max;
    }
}