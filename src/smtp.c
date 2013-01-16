/* nisc - Not Intelligent SMTP Client
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

#include <axTLS/ssl.h>
#include "nisc.h"

static int create_socket(const char *addr, const char *port) {
    struct addrinfo hints, *info, *p;
    int rv;
    int fd;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    rv = getaddrinfo(addr, port, &hints, &info);
    if (rv != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return -1;
    }

    // loop through all the results and connect to the first we can
    for (p = info; p != NULL; p = p->ai_next) {
        fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (fd == -1) {
            perror("client: socket");
            continue;
        }
        if (connect(fd, p->ai_addr, p->ai_addrlen) == -1) {
            close(fd);
            perror("client: connect");
            continue;
        }
        break;
    }
    if (p == NULL) {
        fprintf(stderr, "client: failed to connect\n");
        return 2;
    }

    freeaddrinfo(info); // all done with this structure

    return fd;
}

int smtp_write(struct smtp_t *self, const char *data, int len) {
    return ssl_write(self->ssl, (const uint8_t *)data, len);
}

int smtp_read(struct smtp_t *self, char *data, int sz) {
    uint8_t *out_data;
    int len;

    len = ssl_read(self->ssl, &out_data);
    if (len > 0) {
        memcpy(data, out_data, (len > sz) ? sz : len);
    }
    return len;
}

int smtp_open(struct smtp_t *self) {
    uint32_t options = SSL_SERVER_VERIFY_LATER | SSL_DISPLAY_STATES;

    /* Create socket descriptor */
    self->fd = create_socket(self->host, self->port);
    if (self->fd < 0) {
        return -1;
    }
    /* SSL context */
    self->ssl_ctx = ssl_ctx_new(options, 5);
    if (self->ssl_ctx == NULL) {
        smtp_close(self);
        return -1;
    }
    /* SSL */
    self->ssl = ssl_client_new(self->ssl_ctx, self->fd, NULL, 0);
    if (self->ssl_ctx == NULL || ssl_handshake_status(self->ssl) != SSL_OK) {
        smtp_close(self);
        return -1;
    }
    return 0;
}

void smtp_close(struct smtp_t *self) {
    if (self->ssl != NULL) {
        ssl_free(self->ssl);
        self->ssl = NULL;
    }
    if (self->ssl_ctx != NULL) {
        ssl_ctx_free(self->ssl_ctx);
        self->ssl_ctx = NULL;
    }
    if (self->fd != -1) {
        close(self->fd);
        self->fd = -1;
    }
}

/* vim: set ts=4 sw=4 expandtab: */
