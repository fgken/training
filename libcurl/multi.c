#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/un.h>
#include <unistd.h>

#include <curl/curl.h>

#define PATH_UNIX_SOCKET "/tmp/test-libcurl-multi.sock"

static int
set_socket_timeout(int fd)
{
    struct timeval tv = {
        .tv_sec = 15,
        .tv_usec = 0,
    };

    if (setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)) == -1) {
        fprintf(stderr, "Failed to set SO_SNDTIMEO to socket: %s",
                strerror(errno));
        return -1;
    }
    if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) == -1) {
        fprintf(stderr, "Failed to set SO_RCVTIMEO to socket: %s",
                strerror(errno));
        return -1;
    }

    return 0;
}

int
create_unix_socket(void)
{
    bool success = false;
    int fd = -1;
    const char *path = PATH_UNIX_SOCKET;
    size_t n;
    struct sockaddr_un sa;

    unlink(path);

    fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd == -1) {
        fprintf(stderr, "Failed to create socket: %s", strerror(errno));
        goto end;
    }

    if (set_socket_timeout(fd) != 0) {
        goto end;
    }

    memset(&sa, 0x00, sizeof(sa));
    sa.sun_family = AF_UNIX;
    n = strlcpy(sa.sun_path, path, sizeof(sa.sun_path));
    if (n < strlen(path)) {
        fprintf(stderr, "Failed to bind socket: too long path (%s)", path);
        goto end;
    }
    if (bind(fd, (struct sockaddr *)&sa, sizeof(sa)) == -1) {
        fprintf(stderr, "Failed to bind socket: %s", strerror(errno));
        goto end;
    }
    if (listen(fd, 4) == -1) {
        fprintf(stderr, "Failed to listen socket: %s", strerror(errno));
        goto end;
    }

    success = true;
end:

    if (!success && fd < 0) {
        close(fd);
        fd = -1;
    }

    return fd;
}

static int
add_new_handle(CURLM *multi_handle, const char *url)
{
    CURLMcode mc;
    CURL *http_handle;

    http_handle = curl_easy_init();
    if (http_handle == NULL) {
        fprintf(stderr, "Failed to curl_easy_init\n");
        return -1;
    }
    curl_easy_setopt(http_handle, CURLOPT_URL, url);
    mc = curl_multi_add_handle(multi_handle, http_handle);
    if (mc != CURLM_OK) {
        fprintf(stderr, "Failed to add http_handle to multi\n");
        return -1;
    }
    return 0;
}

int
main(void)
{
    int ret = 1;
    CURL *http_handle;
    CURLM *multi_handle;
    CURLcode res;
    int control_fd = -1;
    struct curl_waitfd extra_fds[1];

    memset(&extra_fds, 0x00, sizeof(extra_fds));

    res = curl_global_init(CURL_GLOBAL_DEFAULT);
    if (res != CURLE_OK) {
        fprintf(stderr, "Failed to initialize libcurl: %s\n",
                curl_easy_strerror(res));
        goto end;
    }
    multi_handle = curl_multi_init();
    if (multi_handle == NULL) {
        fprintf(stderr, "Failed to initialize multi handle\n");
        goto end;
    }

    control_fd = create_unix_socket();
    if (control_fd == -1) {
        fprintf(stderr, "Failed to create control socket\n");
        goto end;
    }
    extra_fds[0].fd = control_fd;
    extra_fds[0].events = CURL_WAIT_POLLIN;

    while (1) {
        CURLMcode mc;
        int numfds;
        int running_handles;

        fprintf(stderr, "TRACE: curl_multi_poll start\n");
        extra_fds[0].revents = 0; // XXX FIXME XXX
        mc = curl_multi_poll(multi_handle, extra_fds, 1, 3000, &numfds);
        if (mc != CURLM_OK) {
            fprintf(stderr, "Failed to curl_multi_poll: %s\n",
                    curl_multi_strerror(mc));
            // goto end;
        }
        fprintf(stderr, "TRACE: curl_multi_poll end: numfds = %d\n", numfds);
        fprintf(stderr, "TRACE: curl_multi_perform start\n");
        mc = curl_multi_perform(multi_handle, &running_handles);
        if (mc != CURLM_OK) {
            fprintf(stderr, "Failed to curl_multi_perform: %s\n",
                    curl_multi_strerror(mc));
            // goto end;
        }
        fprintf(stderr, "TRACE: curl_multi_perform end: running_handles=%d\n",
                running_handles);

        if (extra_fds[0].revents & CURL_WAIT_POLLIN) {
            struct sockaddr_un sa;
            socklen_t salen = sizeof(sa);
            int acpt_fd;

            memset(&sa, 0x00, sizeof(sa));
            fprintf(stderr, "TRACE: extra_fds[0] => CURL_WAIT_POLLIN\n");
            acpt_fd = accept(control_fd, (struct sockaddr *)&sa, &salen);
            if (acpt_fd == -1) {
                fprintf(stderr, "Failed to accept: %s", strerror(errno));
                // goto end;
            }
            fprintf(stderr, "TRCE: acpt_fd = %d\n", acpt_fd);
            close(acpt_fd);
        }
    }

    curl_multi_remove_handle(multi_handle, http_handle);
    curl_easy_cleanup(http_handle);
    curl_multi_cleanup(multi_handle);
    curl_global_cleanup();

    ret = 0;
end:
    if (0 <= control_fd) {
        close(control_fd);
    }
    return ret;
}