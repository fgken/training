#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

#include <curl/curl.h>

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

    add_new_handle(multi_handle, "https://google.com/");

    while (1) {
        CURLMcode mc;
        int numfds;
        int running_handles;

        fprintf(stderr, "TRACE: curl_multi_poll start\n");
        mc = curl_multi_poll(multi_handle, NULL, 0, 3000, &numfds);
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
    }

    curl_multi_remove_handle(multi_handle, http_handle);
    curl_easy_cleanup(http_handle);
    curl_multi_cleanup(multi_handle);
    curl_global_cleanup();

    ret = 0;
end:
    return ret;
}