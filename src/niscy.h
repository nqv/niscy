/* niscy - Not Intelligent SMTP Client Yet
 * Copyright (c) 2013, Quoc-Viet Nguyen
 * See LICENSE file for copyright and license details.
 */

#ifndef NISCY_H_
#define NISCY_H_

#include "version.h"

#define NI_ERR(...)                     fprintf(stderr, __VA_ARGS__)
#ifdef DEBUG
# define NI_LOG(...)                    fprintf(stdout, __VA_ARGS__)
#else
# define NI_LOG(...)
#endif

#define NI_MAX_RESPONSE_LEN             1024
#define NI_TIMEOUT                      30      /* Seconds */

#define NI_OPTION_TLS                   (1 << 0)
#define NI_OPTION_STARTTLS              (1 << 1)

struct smtp_t {
    const char *host;       /* Server */
    const char *port;       /* Port */
    const char *security;   /* TLS/SSL */

    const char *user;       /* Username */
    const char *pass;       /* Password */
    const char *domain;     /* Port */
    const char *auth;       /* Authentication method */
    const char *cert;       /* Certificate file */

    const char *mail_from;  /* FROM address */
    const char **mail_to;   /* List of TO addresses */

    int options;
    int fd;                 /* Socket fd */

    void *ssl_ctx;          /* SSL_CTX */
    void *ssl;              /* SSL */
};

/* smtp.c */
int smtp_open(struct smtp_t *self);
void smtp_close(struct smtp_t *self);
int smtp_write(struct smtp_t *self, const char *data, int len);
int smtp_read(struct smtp_t *self, char *data, int sz);

/* base64.c */
int base64_encode(const char *in, int len, char *out, int sz);

#endif /* NISCY_H_ */

/* vim: set ts=4 sw=4 expandtab: */
