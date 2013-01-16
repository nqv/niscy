/* nisc - Not Intelligent SMTP Client
 */

#include <inttypes.h>
#include <string.h>

/**
 * http://en.wikibooks.org/wiki/Algorithm_implementation/Miscellaneous/Base64#C
 */
int base64_encode(const char *in, int len, char *out, int sz) {
    const char BASE64_CHARS[] =
            "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    int idx = 0;
    int i;
    uint32_t n = 0;
    int count = len % 3;
    uint8_t n0, n1, n2, n3;

    /* increment over the length of the string, three characters at a time */
    for (i = 0; i < len; i += 3) {
        /* these three 8-bit (ASCII) characters become one 24-bit number */
        n = in[i] << 16;

        if ((i + 1) < len)
            n += in[i + 1] << 8;

        if ((i + 2) < len)
            n += in[i + 2];

        /* this 24-bit number gets separated into four 6-bit numbers */
        n0 = (uint8_t) (n >> 18) & 63;
        n1 = (uint8_t) (n >> 12) & 63;
        n2 = (uint8_t) (n >> 6) & 63;
        n3 = (uint8_t) n & 63;

        /* if we have one byte available, then its encoding is spread
         * out over two characters
         */
        if (idx >= sz)
            return -1; /* indicate failure: buffer too small */
        out[idx++] = BASE64_CHARS[n0];
        if (idx >= sz)
            return -1; /* indicate failure: buffer too small */
        out[idx++] = BASE64_CHARS[n1];

        /* if we have only two bytes available, then their encoding is
         * spread out over three chars
         */
        if ((i + 1) < len) {
            if (idx >= sz)
                return -1; /* indicate failure: buffer too small */
            out[idx++] = BASE64_CHARS[n2];
        }

        /* if we have all three bytes available, then their encoding is spread
         * out over four characters
         */
        if ((i + 2) < len) {
            if (idx >= sz)
                return -1; /* indicate failure: buffer too small */
            out[idx++] = BASE64_CHARS[n3];
        }
    }

    /* create and add padding that is required if we did not have a multiple of
     * 3 number of characters available
     */
    if (count > 0) {
        for (; count < 3; count++) {
            if (idx >= sz)
                return -1; /* indicate failure: buffer too small */
            out[idx++] = '=';
        }
    }
    if (idx >= sz)
        return -1; /* indicate failure: buffer too small */
    out[idx] = 0;
    return idx; /* return length of encoded string */
}

/* vim: set ts=4 sw=4 expandtab: */
