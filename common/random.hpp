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

            data(T t, int block_in = tapos_block_num(), int prefix_in = tapos_block_prefix()) {
                content = t;
                block = block_in;
                prefix = prefix_in;
            }
        };

        struct st_seeds {
            capi_checksum256 seed1;
            capi_checksum256 seed2;
        };

    public:
        random(uint64_t mixed = 0);
        random(uint64_t seed, capi_checksum256 seed_hash, int block, int prefix);
        ~random();

        template<class T>
        capi_checksum256 create_sys_seed(T mixed) const;

        void seed(capi_checksum256 sseed, capi_checksum256 useed);
        void update_seed_from_tx(uint64_t mixed);

        void mixseed(capi_checksum256 &sseed, capi_checksum256 &useed, capi_checksum256 &result) const;

        // generator number ranged [0, max-1]
        uint64_t generator(uint64_t max = 101);

        uint64_t gen(capi_checksum256 &seed, uint64_t max = 101) const;

        capi_checksum256 get_sys_seed() const;

        capi_checksum256 get_user_seed() const;

        capi_checksum256 get_mixed() const;

        capi_checksum256 get_seed() const;

    private:
        capi_checksum256 _seed;
    };

    random::random(uint64_t mixed) {
        update_seed_from_tx(mixed);
    }

    random::random(uint64_t seed, capi_checksum256 seed_hash, int block, int prefix) {
        assert_sha256((char *)&seed, sizeof(seed), &seed_hash);
        data<uint64_t> seed_data(seed, block, prefix);
        const char *mixed_char = reinterpret_cast<const char *>(&seed_data);
        sha256((char *) mixed_char, sizeof(seed_data), &_seed);
    }

    random::~random() {}

    template<class T>
    capi_checksum256 random::create_sys_seed(T mixed) const {
        capi_checksum256 result;
        data<T> mixed_block(mixed);
        const char *mixed_char = reinterpret_cast<const char *>(&mixed_block);
        sha256((char *) mixed_char, sizeof(mixed_block), &result);
        return result;
    }

    void random::seed(capi_checksum256 sseed, capi_checksum256 useed) {
        mixseed(_seed, sseed, useed);
    }

    void random::update_seed_from_tx(uint64_t mixed) {
        auto s = transaction_size();
        char *tx = (char *)malloc(s);
        read_transaction(tx, s);

        capi_checksum256 txid;
        sha256(tx, s, &txid);

        capi_checksum256 sseed = create_sys_seed(mixed);
        seed(sseed, txid);
    }

    void random::mixseed(capi_checksum256 &sseed, capi_checksum256 &useed, capi_checksum256 &result) const {
        st_seeds seeds;
        seeds.seed1 = sseed;
        seeds.seed2 = useed;
        sha256((char *) &seeds.seed1, sizeof(seeds.seed1) * 2, &result);
    }

    uint64_t random::generator(uint64_t max) {
        uint64_t r = gen(_seed, max);
        return r;
    }

    uint64_t random::gen(capi_checksum256 &seed, uint64_t max) const {
        if (max <= 0) {
            return 0;
        }
        const uint64_t *p64 = reinterpret_cast<const uint64_t *>(&seed);
        uint64_t r = p64[1] % max;
        return r;
    }

    capi_checksum256 random::get_seed() const {
        return _seed;
    }
}