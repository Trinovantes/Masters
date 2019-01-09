int main() {
    // Initialize N non-blocking cURL calls
    for (int i = 0; i < N; i++) {
        curl_multi_add_handle();
    }

    do {
        // Make the non-blocking cURL calls
        curl_multi_perform();
        do {
            curl_multi_wait();
            curl_multi_perform(&still_running);
        } while (still_running);

        // Read the N results
        while (curl_multi_info_read()) {
            if (CURLMSG_DONE) {
                // Fetch an image fragment from remote web server
                ...
                // Write the image fragment to global image
                ...
            }
        }
    } while (!received_all_fragments);
}