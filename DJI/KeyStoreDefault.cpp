//
// Created by lining on 3/12/25.
//

#include "KeyStoreDefault.h"

#define GLOG_NO_ABBREVIATED_SEVERITIES

#include <glog/logging.h>
#include <unistd.h>
#include <memory>
#include "init.h"
#include "openssl/rsa.h"
#include <fstream>

namespace dji {
    const char *kPathPublickKey = "./dji_esdk_pub_key";
    const char *kPathPrivateKey = "./dji_esdk_private_key";
    std::shared_ptr<std::string> rsa2048_public_key_;
    std::shared_ptr<std::string> rsa2048_private_key_;

    KeyStoreDefault::KeyStoreDefault() {
        //设置证书路径
        _pathPrivateKey = kPathPrivateKey;
        _pathPublicKey = kPathPublickKey;
        //如果有已知的key，用已知的key，否则重新生成
        if (ReadKeys()) {
            return;
        }

        auto ret = GenerateKeys();
        if (!ret) {
            LOG(ERROR) << "Failed to generate keys";
        } else {
            ReadKeys();
        }

    }

    edge_sdk::ErrorCode KeyStoreDefault::RSA2048_GetDERPrivateKey(std::string &private_key) const {
        if (rsa2048_public_key_->empty()) {
            return edge_sdk::kErrorParamGetFailure;
        }
        private_key = *rsa2048_private_key_;
        return edge_sdk::kOk;
    }

    edge_sdk::ErrorCode KeyStoreDefault::RSA2048_GetDERPublicKey(std::string &public_key) const {
        if (rsa2048_public_key_->empty()) {
            return edge_sdk::kErrorParamGetFailure;
        }
        public_key = *rsa2048_public_key_;
        return edge_sdk::kOk;
    }

    bool KeyStoreDefault::ReadKeys() {
        //依次从配置文件中读取
        uint32_t len = 0;
        FILE *file = fopen(_pathPublicKey.c_str(), "rb");
        if (file) {
            auto ret = fread(_buffer, 1, sizeof(_buffer), file);
            if (ret > 0) {
                len = ret;
            }
            LOG(WARNING) << "public key read len: " << ret;
            fclose(file);
        } else {
            return false;
        }
        rsa2048_public_key_ = std::make_shared<std::string>((char *) _buffer, len);

        file = fopen(_pathPrivateKey.c_str(), "rb");
        if (file) {
            auto ret = fread(_buffer, 1, sizeof(_buffer), file);
            if (ret > 0) {
                len = ret;
            }
            LOG(WARNING) << "private key read len: " << ret;
            fclose(file);
        } else {
            return false;
        }
        rsa2048_private_key_ = std::make_shared<std::string>((char *) _buffer, len);
        return true;
    }

    bool KeyStoreDefault::GenerateKeys() {
        RSA *rsa2048 = RSA_generate_key(2048, RSA_F4, NULL, NULL);
        if (!rsa2048) {
            return false;
        }
        int key_len = 0;
        //生成public key
        uint8_t *Pkey = _buffer;
        key_len = i2d_RSAPublicKey(rsa2048, &Pkey);
        rsa2048_public_key_ = std::make_shared<std::string>((char *) _buffer, key_len);
        //保存到文件
        {
            FILE *pkey = fopen(_pathPublicKey.c_str(), "wb");
            if (pkey) {
                fwrite(_buffer, 1, key_len, pkey);
                fclose(pkey);
            }
        }

        //生成private key
        uint8_t *privKey = _buffer;
        key_len = i2d_RSAPrivateKey(rsa2048, &privKey);
        LOG(WARNING) << "Private key len: " << key_len;
        rsa2048_private_key_ = std::make_shared<std::string>((char *) _buffer, key_len);
        //保存到文件
        {
            FILE *pkey = fopen(_pathPrivateKey.c_str(), "wb");
            if (pkey) {
                fwrite(_buffer, 1, key_len, pkey);
                fclose(pkey);
            }
        }

        RSA_free(rsa2048);
        return true;
    }

}