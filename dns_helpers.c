/* (c) 2005-2012 Nicholas J. Kain <njkain at gmail dot com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <curl/curl.h>

#include "dns_helpers.h"
#include "defines.h"
#include "log.h"
#include "strl.h"
#include "malloc.h"
#include "util.h"

static void write_dnsfile(char *fn, char *cnts)
{
    int fd, written = 0, oldwritten, len;

    if (!fn || !cnts)
        suicide("%s: received NULL", __func__);

    fd = open(fn, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
    if (fd == -1)
        suicide("%s: failed to open %s for write", __func__, fn);

    len = strlen(cnts);

    while (written < len) {
        oldwritten = written;
        written = write(fd, cnts + written, len - written);
        if (written == -1) {
            if (errno == EINTR) {
                written = oldwritten;
                continue;
            }
            suicide("%s: write() failed on %s", __func__, fn);
        }
    }

    fsync(fd);
    if (close(fd) == -1)
        suicide("%s: error closing %s; possible corruption", __func__, fn);
}

void write_dnsdate(char *host, time_t date)
{
    int len;
    char *file, buf[MAX_BUF];

    if (!host)
        suicide("%s: host is NULL", __func__);

    len = strlen(host) + strlen("-dnsdate") + 5;
    file = xmalloc(len);
    strlcpy(file, "var/", len);
    strlcat(file, host, len);
    strlcat(file, "-dnsdate", len);
    buf[MAX_BUF - 1] = '\0';
    snprintf(buf, sizeof buf - 1, "%u", (unsigned int)date);

    write_dnsfile(file, buf);
    free(file);
}

/* assumes that if ip is non-NULL, it is valid */
void write_dnsip(char *host, char *ip)
{
    int len;
    char *file, buf[MAX_BUF];

    if (!host)
        suicide("%s: host is NULL", __func__);
    if (!ip)
        suicide("%s: ip is NULL", __func__);

    len = strlen(host) + strlen("-dnsip") + 5;
    file = xmalloc(len);
    strlcpy(file, "var/", len);
    strlcat(file, host, len);
    strlcat(file, "-dnsip", len);
    strlcpy(buf, ip, sizeof buf);

    write_dnsfile(file, buf);
    free(file);
}

/* assumes that if ip is non-NULL, it is valid */
void write_dnserr(char *host, return_codes code)
{
    int len;
    char *file, buf[MAX_BUF], *error;

    if (!host)
        suicide("%s: host is NULL", __func__);

    len = strlen(host) + strlen("-dnserr") + 5;
    file = xmalloc(len);
    strlcpy(file, "var/", len);
    strlcat(file, host, len);
    strlcat(file, "-dnserr", len);

    switch (code) {
        case RET_NOTFQDN:
            error = "notfqdn";
            break;
        case RET_NOHOST:
            error = "nohost";
            break;
        case RET_NOTYOURS:
            error = "!yours";
            break;
        case RET_ABUSE:
            error = "abuse";
            break;
        default:
            error = "unknown";
            break;
    }
    strlcpy(buf, error, sizeof buf);

    write_dnsfile(file, buf);
    free(file);
}

/* Returns 0 on success, 1 on temporary error, terminates program on fatal */
int update_ip_curl_errcheck(int val, char *cerr)
{
    switch (val) {
        case CURLE_OK:
            return 0;
        case CURLE_UNSUPPORTED_PROTOCOL:
        case CURLE_FAILED_INIT:
        case CURLE_URL_MALFORMAT:
        case CURLE_URL_MALFORMAT_USER:
        case CURLE_SSL_CONNECT_ERROR:
        case CURLE_HTTP_RANGE_ERROR:
        case CURLE_HTTP_POST_ERROR:
        case CURLE_ABORTED_BY_CALLBACK:
        case CURLE_BAD_FUNCTION_ARGUMENT:
        case CURLE_BAD_CALLING_ORDER:
        case CURLE_BAD_PASSWORD_ENTERED:
        case CURLE_SSL_PEER_CERTIFICATE:
        case CURLE_SSL_ENGINE_NOTFOUND:
        case CURLE_SSL_ENGINE_SETFAILED:
        case CURLE_SSL_CERTPROBLEM:
        case CURLE_SSL_CIPHER:
        case CURLE_SSL_CACERT:
        case CURLE_BAD_CONTENT_ENCODING:
        case CURLE_SSL_ENGINE_INITFAILED:
        case CURLE_LOGIN_DENIED:
            suicide("Update failed.  cURL returned a fatal error: [%s].  Exiting.", cerr);
            break;
        case CURLE_OUT_OF_MEMORY:
        case CURLE_READ_ERROR:
        case CURLE_TOO_MANY_REDIRECTS:
        case CURLE_RECV_ERROR:
            suicide("Update status unknown.  cURL returned a fatal error: [%s].  Exiting.", cerr);
            break;
        case CURLE_COULDNT_RESOLVE_PROXY:
        case CURLE_COULDNT_RESOLVE_HOST:
        case CURLE_COULDNT_CONNECT:
        case CURLE_OPERATION_TIMEOUTED:
        case CURLE_HTTP_PORT_FAILED:
        case CURLE_GOT_NOTHING:
        case CURLE_SEND_ERROR:
            log_line("Temporary error connecting to host: [%s].  Queuing for retry.", cerr);
            return 1;
        default:
            log_line("cURL returned nonfatal error: [%s]", cerr);
            return 0;
    }
    return -1;
}

void update_ip_buf_error(size_t len, size_t size)
{
    if (len > size)
        suicide("%s: config file would overflow a fixed buffer", __func__);
}

