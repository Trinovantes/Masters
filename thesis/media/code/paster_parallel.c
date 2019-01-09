pthread_mutex_t mutex;

void thread_function() {
    // Fetch an image fragment from remote web server
    ...

    pthread_mutex_lock(mutex)
    // Write the image fragment to global image
    ...
    pthread_mutex_unlock(mutex)
}

int main() {
    // Initialize mutex
    pthread_mutex_init(mutex)

    // Start N threads
    for (int i = 0; i < N; i++) {
        pthread_create(thread_function);
    }

    // Wait for the N threads to finish
    for (int i = 0; i < N; i++) {
        pthread_join();
    }
}