/* nisc - Not Intelligent SMTP Client
 * Copyright (c) 2013, Quoc-Viet Nguyen
 * See LICENSE file for copyright and license details.
 */

#ifndef NISC_H_
#define NISC_H_

#ifdef DEBUG
# define NISC_LOG(...)                  fprintf(stdout, __VA_ARGS__)
#else
# define NISC_LOG(...)
#endif

#define NISC_MAX_RESPONSE_LEN           1024

#define NISC_OPTION_SSL                 (1 << 0)
#define NISC_OPTION_STARTTLS            (1 << 1)

struct smtp_t {
    const char *host;       /* Server */
    const char *port;       /* Port */
    const char *user;       /* Username */
    const char *pass;       /* Password */
    const char *domain;     /* Port */
    const char *auth;       /* Authentication method */
    int options;
    int fd;                 /* Socket fd */
    void *ssl_ctx;          /* SSL_CTX */
    void *ssl;              /* SSL */
};

/* smtp.c */
void smtp_init(struct smtp_t *self);
int smtp_open(struct smtp_t *self);
void smtp_close(struct smtp_t *self);
int smtp_write(struct smtp_t *self, const char *data, int len);
int smtp_read(struct smtp_t *self, char *data, int sz);

/* base64.c */
int base64_encode(const char *in, int len, char *out, int sz);

#endif /* NISC_H_ */

/* vim: set ts=4 sw=4 expandtab: */
