#include <stdio.h>

#include <curl/curl.h>

int
main(void)
{
    CURL *curl;
    CURLcode res;
    int ret = 1;

    curl_global_init(CURL_GLOBAL_DEFAULT);

    curl = curl_easy_init();
    if (curl == NULL) {
        fprintf(stderr, "curl_global_init() failed\n");
        goto end;
    }

    curl_easy_setopt(curl, CURLOPT_URL, "https://www.google.com");
    res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        fprintf(stderr, "curl_easy_perform() failed: %s\n",
                curl_easy_strerror(res));
        goto end;
    }
    ret = 0;

end:
    if (curl != NULL)
        curl_easy_cleanup(curl);

    curl_global_cleanup();

    return ret;
}