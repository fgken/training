#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

#include <curl/curl.h>

int
main(void)
{
    CURL *http_handle;
    CURLM *multi_handle;
    int still_running = 1;

    curl_global_init(CURL_GLOBAL_DEFAULT);
    http_handle = curl_easy_init();
    curl_easy_setopt(http_handle, CURLOPT_URL, "https://www.iij.ad.jp/");
    multi_handle = curl_multi_init();
    curl_multi_add_handle(multi_handle, http_handle);

    while (still_running) {
        CURLMcode mc;
        int numfds;

        mc = curl_multi_perform(multi_handle, &still_running);

        if (still_running) {
            mc = curl_multi_poll(multi_handle, NULL, 0, 1000, &numfds);
        }

        if (mc != CURLM_OK) {
            fprintf(stderr, "failed, code %d\n", mc);
        }
    }

    curl_multi_remove_handle(multi_handle, http_handle);
    curl_easy_cleanup(http_handle);
    curl_multi_cleanup(multi_handle);
    curl_global_cleanup();

    return 0;
}