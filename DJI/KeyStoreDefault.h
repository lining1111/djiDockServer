//
// Created by lining on 3/12/25.
//

#ifndef KEYSTOREDEFAULT_H
#define KEYSTOREDEFAULT_H

#include "init/key_store.h"

namespace dji {
    class KeyStoreDefault : public edge_sdk::KeyStore {
    public:
        KeyStoreDefault();

        virtual ~KeyStoreDefault() override {}

        edge_sdk::ErrorCode RSA2048_GetDERPrivateKey(std::string &private_key) const override;

        edge_sdk::ErrorCode RSA2048_GetDERPublicKey(std::string &public_key) const override;

    private:
        std::string _pathPublicKey;
        std::string _pathPrivateKey;
        uint8_t _buffer[1500];
        bool ReadKeys();

        bool GenerateKeys();
    };
}


#endif //KEYSTOREDEFAULT_H
