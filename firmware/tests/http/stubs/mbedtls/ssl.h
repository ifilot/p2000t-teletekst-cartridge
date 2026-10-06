#include <stddef.h>
#define MBEDTLS_SSL_VERIFY_NONE 0
typedef struct {
    int unused;
} mbedtls_ssl_context;
int mbedtls_ssl_set_hostname(mbedtls_ssl_context *, const char *);
void mbedtls_ssl_set_hs_authmode(mbedtls_ssl_context *, int);
