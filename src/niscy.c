/* niscy - Not Intelligent SMTP Client Yet
 * Copyright (c) 2013, Quoc-Viet Nguyen
 * See LICENSE file for copyright and license details.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "niscy.h"

#define SAVE_ARG_(var, arg, list, idx)      \
        var = (arg[2] == '\0') ? list[++idx] : &arg[2]

#define IS_EMPTY_(str)          ((str) == NULL || (str)[0] == '\0')

static char buf_[NI_MAX_RESPONSE_LEN];

static struct smtp_t smtp_ = {
        .host       = "127.0.0.1",
        .port       = "25",
        .security   = NULL,
        .user       = NULL,
        .pass       = NULL,
        .domain     = "niscy",
        .auth       = NULL,

        .mail_from  = NULL,
        .mail_to    = NULL,

        .options    = 0,
        .fd         = -1,

        .ssl_ctx    = NULL,
        .ssl        = NULL,
};

/* Helpers */

static void parse_args(int argc, char **argv) {
    int i;
    char *arg;

    for (i = 1; i < argc; ++i) {
        arg = argv[i];
        /* Assume that the receipt addresses are from the first argument
         * that does not start with '-' */
        if (arg[0] != '-') {
            smtp_.mail_to = (const char **)&argv[i];
            break;
        }
        switch (arg[1]) {
        case 'U':   /* Username */
            SAVE_ARG_(smtp_.user, arg, argv, i);
            break;
        case 'P':   /* Password */
            SAVE_ARG_(smtp_.pass, arg, argv, i);
            break;
        case 'F':   /* From */
            SAVE_ARG_(smtp_.mail_from, arg, argv, i);
            break;
        case 'a':   /* Authentication method */
            SAVE_ARG_(smtp_.auth, arg, argv, i);
            break;
        case 'c':   /* Certificate file */
            SAVE_ARG_(smtp_.cert, arg, argv, i);
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
        case 's':   /* Security */
            SAVE_ARG_(smtp_.security, arg, argv, i);
            break;
        }
    }
}

static int check_settings() {
    if (IS_EMPTY_(smtp_.host)) {
        NI_ERR("Host must not be empty.\n");
        return -1;
    }
    if (IS_EMPTY_(smtp_.port)) {
        NI_ERR("Port must not be empty.\n");
        return -1;
    }
    if (IS_EMPTY_(smtp_.domain)) {
        NI_ERR("Domain must not be empty.\n");
        return -1;
    }
    if (IS_EMPTY_(smtp_.mail_from)) {
        NI_ERR("FROM address must not be empty.\n");
        return -1;
    }
    if ((smtp_.mail_to == NULL) || (*smtp_.mail_to == NULL)) {
        NI_ERR("Receipt address must not be empty.\n");
        return -1;
    }
    /* Enable TLS */
    if (smtp_.security != NULL) {
        if (strcasecmp(smtp_.security, "starttls") == 0) {
            smtp_.options |= (NI_OPTION_TLS | NI_OPTION_STARTTLS);
        } else if (strcasecmp(smtp_.security, "tls") == 0) {
            smtp_.options |= NI_OPTION_TLS;
        } else {
            NI_ERR("Encrypted method is not supported.\n");
            return -1;
        }
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

static int format_date(char *data, int sz) {
    time_t now;
    struct tm *tm;
    const char * const FORMAT = "%a, %d %b %Y %H:%M:%S %z";

    now = time(NULL);
    tm = localtime(&now);

    return strftime(data, sz, FORMAT, tm);
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
            NI_LOG("[NISCY] smtp read:\n%.*s\n", len, buf_);
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

static int probe() {
    int len;

    if (smtp_open(&smtp_) != 0) {
        NI_ERR("Could not connect to server.\n");
        return 1;
    }
    len = get_smtp_code();
    if (len < 200 || len >= 300) {
        NI_ERR("Unexpected server response: %d.\n", len);
        return -1;
    }
    return 0;
}

static int ehlo() {
    int len;

    /* Try EHLO if username is provided */
    len = snprintf(buf_, sizeof(buf_),
            (smtp_.user != NULL) ? "EHLO %s\r\n" : "HELO %s\r\n", smtp_.domain);
    if (len >= (int)sizeof(buf_)) {
        return -1;              /* Buffer is not large enough */
    }
    if (smtp_write(&smtp_, buf_, len) <= 0) {
        return -1;
    }
    /* Check response code */
    len = get_smtp_code();
    if (len < 200 || len >= 300) {
        NI_ERR("Unexpected EHLO response: %d.\n", len);
        return -1;
    }
    return 0;
}

static int starttls() {
    int len;

    if (smtp_write(&smtp_, "STARTTLS\r\n", 10) <= 0) {
        smtp_close(&smtp_);
        return -1;
    }
    len = get_smtp_code();
    if (len < 200 || len >= 300) {
        NI_ERR("Server does not accept STARTTLS: %d.\n", len);
        return -1;
    }
    return 0;
}

static int auth_login() {
    int len;

    /* Send authentication */
    if (smtp_write(&smtp_, "AUTH LOGIN\r\n", 12) <= 0) {
        return -1;
    }
    len = get_smtp_code();
    if (len < 300 || len >= 400) {
        NI_ERR("Unexpected AUTH LOGIN response: %d.\n", len);
        return -1;
    }
    /* Username */
    len = base64_encode(smtp_.user, strlen(smtp_.user), buf_,
            sizeof(buf_) - 2);      /* Reserved for \r\n */
    NI_LOG(">>> User [%s] / %d\n", buf_, len);
    if (len < 0) {
        return -1;
    }
    buf_[len++] = '\r';
    buf_[len++] = '\n';
    if (smtp_write(&smtp_, buf_, len) <= 0) {
        return -1;
    }
    len = get_smtp_code();
    if (len < 300 || len >= 400) {
        NI_ERR("Unexpected AUTH/Username response: %d.\n", len);
        return -1;
    }
    /* Password */
    if (smtp_.pass != NULL) {
        len = base64_encode(smtp_.pass, strlen(smtp_.pass), buf_,
                sizeof(buf_) - 2);  /* Reserved for \r\n */
        if (len < 0) {
            NI_ERR("Could not encode password.\n");
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
    len = get_smtp_code();
    if (len < 200 || len >= 300) {
        NI_ERR("Authentication failed %d.\n", len);
        return -1;
    }
    return 0;
}

/*
 * AUTH PLAIN authorization-id\0authentication-id\0passwd
 */
static int auth_plain() {
    int len;
    char message[512];          /* Combination of username and password */

    /* Send authentication */
    if (smtp_write(&smtp_, "AUTH PLAIN\r\n", 12) <= 0) {
        return -1;
    }
    len = get_smtp_code();
    if (len < 300 || len >= 400) {
        NI_ERR("Unexpected AUTH PLAIN response: %d.\n", len);
        return -1;
    }
    /* Authorization & authentication */
    len = snprintf(message, sizeof(message), "%s%c%s%c%s",
            smtp_.user, '\0', smtp_.user, '\0',
            /* Password might be empty */
            (smtp_.pass != NULL) ? smtp_.pass : "");
    if (len >= (int)sizeof(message)) {
        return -1;              /* Buffer is not large enough */
    }
    /* Encode and send */
    len = base64_encode(message, len, buf_, sizeof(buf_) - 2);
    if (len < 0) {
        NI_ERR("Could not encode authentication.\n");
        return -1;
    }
    buf_[len++] = '\r';
    buf_[len++] = '\n';
    if (smtp_write(&smtp_, buf_, len) <= 0) {
        return -1;
    }
    len = get_smtp_code();
    if (len < 200 || len >= 300) {
        NI_ERR("Authentication failed %d.\n", len);
        return -1;
    }
    return 0;
}

static int mail_from() {
    int len;

    len = snprintf(buf_, sizeof(buf_), "MAIL FROM:<%s>\r\n", smtp_.mail_from);
    if (len >= (int)sizeof(buf_)) {
        return -1;              /* Buffer is not large enough */
    }
    if (smtp_write(&smtp_, buf_, len) <= 0) {
        return -1;
    }
    len = get_smtp_code();
    if (len < 200 || len >= 300) {
        NI_ERR("Invalid MAIL FROM %d.\n", len);
        return -1;
    }
    return 0;
}

static int rcpt_to() {
    int len;
    int i;
    int count = 0;
    const char *addr;

    for (i = 0; (addr = smtp_.mail_to[i]) != NULL; ++i) {
        /* Ignore email starting with '-' */
        if (addr[0] == '-') {
            continue;
        }
        len = snprintf(buf_, sizeof(buf_), "RCPT TO:<%s>\r\n", addr);
        if (len >= (int)sizeof(buf_)) {
            return -1;          /* Buffer is not large enough */
        }
        if (smtp_write(&smtp_, buf_, len) <= 0) {
            return -1;
        }
        len = get_smtp_code();
        if (len < 200 || len >= 300) {
            NI_ERR("Invalid RCPT TO <%s>: %d.\n", addr, len);
            /* Continue */
        } else {
            ++count;
        }
    }
    return (count == 0) ? -1 : 0;
}

static int mail_data() {
    int len;

    if (smtp_write(&smtp_, "DATA\r\n", 6) <= 0) {
        return -1;
    }
    len = get_smtp_code();
    if (len < 300 || len >= 400) {
        NI_ERR("Unexpected DATA response: %d.\n", len);
        return -1;
    }
    /* Append our headers */
    len = snprintf(buf_, sizeof(buf_), "Received: by %s"
            " (NISCY v" NISCY_VERSION "); ", smtp_.domain);
    if (len >= (int)sizeof(buf_)) {
        return -1;              /* Buffer is not large enough */
    }

    len += format_date(buf_ + len, sizeof(buf_) - len);
    buf_[len++] = '\r';
    buf_[len++] = '\n';
    if (smtp_write(&smtp_, buf_, len) <= 0) {
        return -1;
    }
    return 0;
}

/* RFC 2821: If a line starts with a period, additional period must be
 * inserted */
static int mail_stream(FILE *stream) {
    int is_chunk;
    int len;
    char *line;

    buf_[0] = '.';
    is_chunk = 0;
    while (!feof(stream)) {
        /* Reserved for leading period & CRLF */
        line = buf_ + 1;
        if (fgets(line, sizeof(buf_) - 3, stream) == NULL) {
            break;
        }
        len = strlen(line);
        /* Check the first char only in a whole line */
        if (!is_chunk) {
            if (line[0] == '.') {
                line = buf_;    /* Including the first char from the buffer */
                ++len;
            }
        }
        /* Clear new-line from stream */
        if (len > 0) {
            if (line[len - 1] == '\n') {
                --len;
                if ((len > 0) && (line[len - 1] == '\r')) {
                    --len;
                }
                is_chunk = 0;
            } else {
                is_chunk = 1;
            }
        }
        /* Append ours if end of line */
        if (!is_chunk) {
            line[len++] = '\r';
            line[len++] = '\n';
        }
        if (smtp_write(&smtp_, line, len) <= 0) {
            return -1;
        }
    }
    /* Terminate */
    if (is_chunk) {     /* Not yet having CRLF */
        smtp_write(&smtp_, "\r\n", 2);
    }
    smtp_write(&smtp_, ".\r\n", 3);

    len = get_smtp_code();
    if (len < 200 || len >= 300) {
        NI_ERR("Could not send message: %d.\n", len);
        return -1;
    }
    return 0;
}

/* Main functions */

static int niscy_connect() {
    if (smtp_.options & NI_OPTION_STARTTLS) {
        /* Temporary disable SSL handshaking to ask server for STARTTLS */
        smtp_.options &= ~NI_OPTION_TLS;
        if ((probe() != 0) || (ehlo() != 0) || (starttls() != 0)) {
            smtp_close(&smtp_);
            return -1;
        }
        /* Re-enable SSL and negotiate again */
        smtp_.options |= NI_OPTION_TLS;
        if (smtp_open(&smtp_) != 0) {
            NI_ERR("Could not negotiate secure connection.\n");
            smtp_close(&smtp_);
            return -1;
        }
    } else {
        if ((probe() != 0) || (ehlo() != 0)) {
            smtp_close(&smtp_);
            return -1;
        }
    }
    return 0;
}

static void niscy_disconnect() {
    smtp_write(&smtp_, "QUIT\r\n", 6);
    smtp_close(&smtp_);

    NI_LOG("Bye.\n");
}

/* Server requires login
 * username must not be empty
 */
static int niscy_auth() {
    if (smtp_.user != NULL) {
        if (smtp_.auth == NULL || strcasecmp(smtp_.auth, "login") == 0) {
            return auth_login();
        }
        if (strcasecmp(smtp_.auth, "plain") == 0) {
            return auth_plain();
        }
        NI_ERR("Authentication method is not supported: %s.\n", smtp_.auth);
        return -1;
    } else {
        NI_LOG("Not to authenticate.\n");
    }
    return 0;
}

/* Send mail body */
static int niscy_mail() {
    if ((mail_from() != 0)
            || (rcpt_to() != 0)
            || (mail_data() != 0)
            || (mail_stream(stdin) != 0)) {
        return -1;
    }
    return 0;
}

int main(int argc, char **argv) {
    parse_args(argc, argv);

    if (check_settings() != 0 || (niscy_connect() != 0)) {
        return 1;
    }
    if ((niscy_auth() != 0) || (niscy_mail() != 0)) {
        niscy_disconnect();
        return 1;
    }
    niscy_disconnect();
    return 0;
}

/* vim: set ts=4 sw=4 expandtab: */
