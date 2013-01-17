/* nisc - Not Intelligent SMTP Client
 * Copyright (c) 2013, Quoc-Viet Nguyen
 * See LICENSE file for copyright and license details.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "nisc.h"

#define SAVE_ARG_(var, arg, list, idx) {    \
        if (arg[2] == '\0') {               \
            var = list[++idx];              \
        } else {                            \
            var = &arg[2];                  \
        }                                   \
    }

#define IS_EMPTY_(str)          ((str) == NULL || (str)[0] == '\0')

static char buf_[NISC_MAX_RESPONSE_LEN];

static struct smtp_t smtp_ = {
        .host       = "127.0.0.1",
        .port       = "27",
        .user       = NULL,
        .pass       = NULL,
        .domain     = "nisc",
        .auth       = NULL,
        .options    = NISC_OPTION_SSL,
        .fd         = -1,
        .ssl_ctx    = NULL,
        .ssl        = NULL,
};

/* Helpers */

static void parse_args(int argc, char **argv) {
    int i = 1;
    char *arg;

    while (i < argc) {
        arg = argv[i];
        if (arg[0] == '-') {
            switch (arg[1]) {
            case 'U':   /* Username */
                SAVE_ARG_(smtp_.user, arg, argv, i);
                break;
            case 'P':   /* Password */
                SAVE_ARG_(smtp_.pass, arg, argv, i);
                break;
            case 'a':   /* Authentication method */
                SAVE_ARG_(smtp_.auth, arg, argv, i);
                break;
            case 'd':   /* Domain */
                SAVE_ARG_(smtp_.domain, arg, argv, i);
                break;
            case 'h':   /* Host */
                SAVE_ARG_(smtp_.host, arg, argv, i);
                break;
            case 'p':   /* Port */
                SAVE_ARG_(smtp_.port, arg, argv, i);
                break;
            }
        }
        /* Skip this argument */
        ++i;
    }
}

static int check_settings() {
    if (IS_EMPTY_(smtp_.host)) {
        fprintf(stderr, "Host must not be empty.\n");
        return -1;
    }
    if (IS_EMPTY_(smtp_.port)) {
        fprintf(stderr, "Port must not be empty.\n");
        return -1;
    }
    if (IS_EMPTY_(smtp_.domain)) {
        fprintf(stderr, "Domain must not be empty.\n");
        return -1;
    }
    return 0;
}

/* Find the last line in a buffer */
static const char *find_lastline(const char *data, int len) {
    const char *p;

    --len;
    p = data + len;
    while (len >= 0) {
        if (*p == '\n') {
            return p + 1;
        }
        --len;
        --p;
    }
    return data;
}

/* Get SMTP response code
 E.g:
   C: EHLO my.abc.org
   S: 250-smtp.xyz.com Hello my.abc.org
   S: 250-SIZE 12800000
   S: 250 HELP
*/
static int get_smtp_code() {
    int len = 0;
    int rv;
    const char *lastline;

    while (len < (int)sizeof(buf_)) {
        rv = smtp_read(&smtp_, buf_ + len, sizeof(buf_) - len);
        if (rv == 0) {
            continue;       /* Again */
        }
        if (rv < 0) {
            return -1;      /* Error */
        }
        len += rv;
        /* Find the last line */
        if (buf_[len - 1] == '\n') {
            NISC_LOG("[NISC] smtp read:\n%.*s\n", len, buf_);
            lastline = find_lastline(buf_, len - 1);
            if (lastline[3] == ' ') {
                break;
            }
        }
    }
    if (len >= (int)sizeof(buf_)) {
        /* Not completed */
        len = sizeof(buf_);
        lastline = find_lastline(buf_, len - 1);
    }
    /* Terminate this */
    buf_[len - 1] = '\0';
    /* Parse response code */
    return atoi(lastline);
}

static int auth_login() {
    int len, rv;

    /* Send authentication */
    if (smtp_write(&smtp_, "AUTH LOGIN\r\n", sizeof("AUTH LOGIN\r\n")) <= 0) {
        return -1;
    }
    rv = get_smtp_code();
    if (rv < 300 || rv >= 400) {
        NISC_ERR("AUTH LOGIN failed %d.\n", rv);
        return -1;
    }
    /* Username */
    len = base64_encode(smtp_.user, strlen(smtp_.user), buf_,
            sizeof(buf_) - 2);      /* Reserved for \r\n */
    if (len < 0) {
        NISC_ERR("Could not encode username.\n");
        return -1;
    }
    buf_[len++] = '\r';
    buf_[len++] = '\n';
    if (smtp_write(&smtp_, buf_, len) <= 0) {
        return -1;
    }
    rv = get_smtp_code();
    if (rv < 300 || rv >= 400) {
        NISC_ERR("AUTH/Username failed %d.\n", rv);
        return -1;
    }
    /* Password */
    if (smtp_.pass != NULL) {
        len = base64_encode(smtp_.pass, strlen(smtp_.pass), buf_,
                sizeof(buf_) - 2);  /* Reserved for \r\n */
        if (len < 0) {
            NISC_ERR("Could not encode password.\n");
            return -1;
        }
    } else {
        len = 0;
    }
    buf_[len++] = '\r';
    buf_[len++] = '\n';
    if (smtp_write(&smtp_, buf_, len) <= 0) {
        return -1;
    }
    rv = get_smtp_code();
    if (rv < 200 || rv >= 300) {
        NISC_ERR("Authentication failed %d.\n", rv);
        return -1;
    }
    return 0;
}

/*
 * AUTH PLAIN authorization-id\0authentication-id\0passwd
 */
static int auth_plain() {
    int len, rv;
    char message[512];          /* Combination of username and password */

    /* Send authentication */
    if (smtp_write(&smtp_, "AUTH PLAIN\r\n", sizeof("AUTH PLAIN\r\n")) <= 0) {
        return -1;
    }
    rv = get_smtp_code();
    if (rv < 300 || rv >= 400) {
        NISC_ERR("AUTH PLAIN failed %d.\n", rv);
        return -1;
    }
    /* Authorization */
    len = snprintf(message, sizeof(message), "%s", smtp_.user);
    message[len++] = '\0';
    /* Authentication */
    len += snprintf(message + len, sizeof(message) - len, "%s", smtp_.user);
    message[len++] = '\0';
    /* Password */
    if (smtp_.pass != NULL) {
        len += snprintf(message + len, sizeof(message) - len, "%s", smtp_.pass);
    }
    message[len++] = '\0';

    /* Encode and send */
    len = base64_encode(message, len, buf_, sizeof(buf_) - 2);
    if (len < 0) {
        NISC_ERR("Could not encode authentication.\n");
        return -1;
    }
    buf_[len++] = '\r';
    buf_[len++] = '\n';
    if (smtp_write(&smtp_, buf_, len) <= 0) {
        return -1;
    }
    rv = get_smtp_code();
    if (rv < 200 || rv >= 300) {
        NISC_ERR("Authentication failed %d.\n", rv);
        return -1;
    }
    return 0;
}

static int mail_from() {
    return 0;
}

static int mail_to() {
    return 0;
}

/* Main functions */

static int nisc_connect() {
    int rv;

    if (smtp_open(&smtp_) != 0) {
        NISC_ERR("Could not connect to SMTP server.\n");
        return 1;
    }
    rv = get_smtp_code();
    if (rv < 200 || rv >= 300) {
        smtp_close(&smtp_);
        NISC_ERR("Invalid response %d.\n", rv);
        return -1;
    }
    return 0;
}

static void nisc_disconnect() {
    smtp_write(&smtp_, "QUIT\r\n", sizeof("QUIT\r\n"));
    smtp_close(&smtp_);

    NISC_LOG("Bye.\n");
}

/* Server requires login
 * username must not be empty
 */
static int nisc_auth() {
    int len, rv;

    /* Try EHLO if username is provided */
    len = snprintf(buf_, sizeof(buf_),
            (smtp_.user != NULL) ? "EHLO %s\r\n" : "HELO %s\r\n", smtp_.domain);
    if (smtp_write(&smtp_, buf_, len) <= 0) {
        return -1;
    }
    /* Check response code */
    rv = get_smtp_code();
    if (rv < 200 || rv >= 300) {
        fprintf(stderr, "HELO failed %d.\n", rv);
        return -1;
    }
    if (smtp_.user != NULL) {
        if (smtp_.auth == NULL || strcasecmp(smtp_.auth, "login") == 0) {
            return auth_login();
        }
        if (strcasecmp(smtp_.auth, "plain") == 0) {
            return auth_plain();
        }
        NISC_ERR("Authentication method is not supported: %s.\n", smtp_.auth);
        return -1;
    }
    return 0;
}

static int nisc_mail() {
    mail_from();

    mail_to();

    return 0;
}

int main(int argc, char **argv) {
    parse_args(argc, argv);

    if (check_settings() != 0) {
        return 1;
    }
    if ((nisc_connect() == 0) && (nisc_auth() == 0)) {
        nisc_mail();
    }
    nisc_disconnect();
    return 0;
}

/* vim: set ts=4 sw=4 expandtab: */
