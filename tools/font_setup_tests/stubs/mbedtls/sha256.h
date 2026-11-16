#pragma once
#include <cstddef>
#include <cstdint>
struct mbedtls_sha256_context {};
inline void mbedtls_sha256_init(mbedtls_sha256_context*){}
inline void mbedtls_sha256_free(mbedtls_sha256_context*){}
inline void mbedtls_sha256_starts(mbedtls_sha256_context*,int){}
inline void mbedtls_sha256_update(mbedtls_sha256_context*,const uint8_t*,size_t){}
inline void mbedtls_sha256_finish(mbedtls_sha256_context*,uint8_t*){}
