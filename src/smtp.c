/* niscy - Not Intelligent SMTP Client Yet
 * Copyright (c) 2013, Quoc-Viet Nguyen
 * See LICENSE file for copyright and license details.
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>

#ifdef NI_AXTLS
#include <axTLS/ssl.h>
#endif

#include "niscy.h"

static int create_socket(const char *addr, const char *port) {
    struct addrinfo hints, *info, *p;
    struct timeval timeout;
    int rv;
    int fd;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    rv = getaddrinfo(addr, port, &hints, &info);
    if (rv != 0) {
        NI_ERR("getaddrinfo: %s.\n", gai_strerror(rv));
        return -1;
    }

    /* Connect to the first address */
    for (p = info; p != NULL; p = p->ai_next) {
        fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (fd == -1) {
            NI_LOG("socket: %s.\n", strerror(errno));
            continue;
        }
        if (connect(fd, p->ai_addr, p->ai_addrlen) == -1) {
            close(fd);
            NI_LOG("connect: %s.\n", strerror(errno));
            continue;
        }
        break;
    }
    freeaddrinfo(info);
    if (p == NULL) {
        NI_ERR("Failed to connect.\n");
        return -1;
    }

    /* Socket timeout */
    timeout.tv_sec = NI_TIMEOUT;
    timeout.tv_usec = 0;
    if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, (char *) &timeout,
            sizeof(timeout)) < 0) {
        NI_ERR("setsockopt SO_RCVTIMEO: %s.\n", strerror(errno));
    }
    if (setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, (char *) &timeout,
            sizeof(timeout)) < 0) {
        NI_ERR("setsockopt SO_SNDTIMEO: %s.\n", strerror(errno));
    }

    return fd;
}

int smtp_write(struct smtp_t *self, const char *data, int len) {
    if (self->options & NI_OPTION_TLS) {
#ifdef NI_AXTLS
        return ssl_write(self->ssl, (const uint8_t *)data, len);
#endif  /* NI_AXTLS */
    }
    return send(self->fd, data, len, MSG_NOSIGNAL);
}

int smtp_read(struct smtp_t *self, char *data, int sz) {
    if (self->options & NI_OPTION_TLS) {
#ifdef NI_AXTLS
        uint8_t *out_data;
        int len;

        len = ssl_read(self->ssl, &out_data);
        if (len > 0) {
            memcpy(data, out_data, (len > sz) ? sz : len);
        }
        return len;
#endif  /* NI_AXTLS */
    }
    return recv(self->fd, data, sz, 0);
}

int smtp_open(struct smtp_t *self) {
    /* Create socket descriptor. The socket is checked as this function
     * is reinvoked in STARTTLS mode */
    if (self->fd == -1) {
        self->fd = create_socket(self->host, self->port);
        if (self->fd < 0) {
            return -1;
        }
    }
    if (self->options & NI_OPTION_TLS) {  /* AxTLS */
#ifdef NI_AXTLS
        uint32_t options = SSL_SERVER_VERIFY_LATER;

#if 0
        options |= SSL_DISPLAY_STATES;
#endif
        /* SSL context */
        self->ssl_ctx = ssl_ctx_new(options, 5);
        if (self->ssl_ctx == NULL) {
            smtp_close(self);
            return -1;
        }
        /* Certificate */
        if (self->cert != NULL
                && ssl_obj_load(self->ssl_ctx, SSL_OBJ_X509_CERT,
                    self->cert, NULL) != SSL_OK) {
            NI_ERR("Load certificate failed %s.\n", self->cert);
            smtp_close(self);
            return -1;
        }
        /* SSL */
        self->ssl = ssl_client_new(self->ssl_ctx, self->fd, NULL, 0);
        if (self->ssl_ctx == NULL
                || ssl_handshake_status(self->ssl) != SSL_OK) {
            smtp_close(self);
            return -1;
        }
#endif  /* NI_AXTLS */
    }
    return 0;
}

void smtp_close(struct smtp_t *self) {
#ifdef NI_AXTLS
    if (self->ssl != NULL) {
        ssl_free(self->ssl);
        self->ssl = NULL;
    }
    if (self->ssl_ctx != NULL) {
        ssl_ctx_free(self->ssl_ctx);
        self->ssl_ctx = NULL;
    }
#endif  /* NI_AXTLS */
    if (self->fd != -1) {
        close(self->fd);
        self->fd = -1;
    }
}

/* vim: set ts=4 sw=4 expandtab: */
