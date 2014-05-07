/*
 * Copyright (c) 2011-2014 libbitcoin developers (see AUTHORS)
 *
 * This file is part of libbitcoin.
 *
 * libbitcoin is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License with
 * additional permissions to the one published by the Free Software
 * Foundation, either version 3 of the License, or (at your option)
 * any later version. For more information see LICENSE.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
#include <bitcoin/utility/ec_keys.hpp>

#include <secp256k1.h>
#include <bitcoin/utility/assert.hpp>

// TODO: temporary! Remove once adding points is resolved!
#include <openssl/ec.h>
#include <openssl/bn.h>
// -------------------------------

namespace libbitcoin {

/**
 * Internal class to start and stop the secp256k1 library.
 */
class init_singleton
{
public:
    init_singleton()
      : init_done_(false)
    {}
    ~init_singleton()
    {
        if (init_done_)
            secp256k1_stop();
    }
    void init()
    {
        if (init_done_)
            return;
        secp256k1_start();
        init_done_ = true;
    }

private:
    bool init_done_;
};

static init_singleton init;

BC_API ec_point secret_to_public_key(const ec_secret& secret,
    bool compressed)
{
    init.init();
    size_t size = ec_uncompressed_size;
    if (compressed)
        size = ec_compressed_size;

    ec_point out(size);
    int out_size;
    if (!secp256k1_ecdsa_pubkey_create(out.data(), &out_size, secret.data(),
            compressed))
        return ec_point();
    BITCOIN_ASSERT(size == static_cast<size_t>(out_size));
    return out;
}

BC_API bool verify_public_key(const ec_point& public_key)
{
    init.init();
    return secp256k1_ecdsa_pubkey_verify(public_key.data(), public_key.size());
}

BC_API bool verify_private_key(const ec_secret& private_key)
{
    init.init();
    return secp256k1_ecdsa_seckey_verify(private_key.data());
}

BC_API data_chunk sign(ec_secret secret, hash_digest hash, ec_secret nonce)
{
    init.init();
    int out_size = 72;
    data_chunk signature(out_size);

    if (!verify_private_key(nonce)) // Needed because of upstream bug
        return data_chunk();
    bool valid = secp256k1_ecdsa_sign(hash.data(), hash.size(),
        signature.data(), &out_size, secret.data(), nonce.data());
    if (!valid)
        return data_chunk();

    signature.resize(out_size);
    return signature;
}

BC_API bool verify_signature(const ec_point& public_key, hash_digest hash,
    const data_chunk& signature)
{
    init.init();
    return 1 == secp256k1_ecdsa_verify(
        hash.data(), hash.size(),
        signature.data(), signature.size(),
        public_key.data(), public_key.size()
    );
}

BC_API bool operator+=(ec_point& a, const ec_point& b)
{
#define NID_secp256k1 714
    bool success = false;
    EC_GROUP* group = EC_GROUP_new_by_curve_name(NID_secp256k1);
    BN_CTX* ctx = BN_CTX_new();
    EC_POINT* point_a = EC_POINT_new(group);
    EC_POINT* point_b = EC_POINT_new(group);
    size_t data_size = 0;
    if (!EC_POINT_oct2point(group, point_a, a.data(), a.size(), ctx))
        goto fail;
    if (!EC_POINT_oct2point(group, point_b, b.data(), b.size(), ctx))
        goto fail;
    if (!EC_POINT_add(group, point_a, point_a, point_b, ctx))
        goto fail;
    data_size = EC_POINT_point2oct(group, point_a,
        POINT_CONVERSION_COMPRESSED, NULL, 0, ctx);
    a.resize(data_size);
    if (!EC_POINT_point2oct(group, point_a,
        POINT_CONVERSION_COMPRESSED, a.data(), a.size(), ctx))
        goto fail;
    success = true;
fail:
    EC_POINT_free(point_b);
    EC_POINT_free(point_a);
    BN_CTX_free(ctx);
    EC_GROUP_free(group);
    return success;
}

BC_API bool operator+=(ec_point& a, const ec_secret& b)
{
    init.init();
    return secp256k1_ecdsa_pubkey_tweak_add(a.data(), a.size(), b.data());
}

BC_API bool operator*=(ec_point& a, const ec_secret& b)
{
    init.init();
    return secp256k1_ecdsa_pubkey_tweak_mul(a.data(), a.size(), b.data());
}

BC_API bool operator+=(ec_secret& a, const ec_secret& b)
{
    init.init();
    return secp256k1_ecdsa_privkey_tweak_add(a.data(), b.data());
}

BC_API bool operator*=(ec_secret& a, const ec_secret& b)
{
    init.init();
    return secp256k1_ecdsa_privkey_tweak_mul(a.data(), b.data());
}

} // namespace libbitcoin

