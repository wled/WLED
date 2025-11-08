#include "mbedtls/sha1.h"
#include "SHA1Builder.h"

// Wrapper functions to map mbedtls SHA1 calls to Arduino SHA1Builder
// This is needed because ESP-IDF 5.x disables SHA1 in mbedtls by default

struct mbedtls_sha1_context_wrapper {
    SHA1Builder builder;
};

extern "C" {

void mbedtls_sha1_init(mbedtls_sha1_context *ctx) {
    // Allocate wrapper
    auto* wrapper = new mbedtls_sha1_context_wrapper();
    *(void**)ctx = wrapper;
}

int mbedtls_sha1_starts(mbedtls_sha1_context *ctx) {
    auto* wrapper = *(mbedtls_sha1_context_wrapper**)ctx;
    wrapper->builder.begin();
    return 0;
}

int mbedtls_sha1_update(mbedtls_sha1_context *ctx, const unsigned char *input, size_t ilen) {
    auto* wrapper = *(mbedtls_sha1_context_wrapper**)ctx;
    wrapper->builder.add((const uint8_t*)input, ilen);
    return 0;
}

int mbedtls_sha1_finish(mbedtls_sha1_context *ctx, unsigned char output[20]) {
    auto* wrapper = *(mbedtls_sha1_context_wrapper**)ctx;
    wrapper->builder.calculate();
    wrapper->builder.getBytes(output);
    return 0;
}

void mbedtls_sha1_free(mbedtls_sha1_context *ctx) {
    auto* wrapper = *(mbedtls_sha1_context_wrapper**)ctx;
    delete wrapper;
}

} // extern "C"

