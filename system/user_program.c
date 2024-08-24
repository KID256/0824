#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>
#include <string.h>
#include <errno.h>
#include <poll.h>

#define DEVICE_FILE "/dev/motion_sensor"
#define BUFFER_SIZE 16

volatile sig_atomic_t keep_running = 1;

void signal_handler(int signo) {
    if (signo == SIGINT || signo == SIGTERM || signo == SIGQUIT) {
        printf("Preparing to exit...\n");
        keep_running = 0;
    }
}

void set_user_space_running(int fd, int state) {
    if (fd < 0) {
        fprintf(stderr, "Invalid file descriptor\n");
        return;
    }
    // Write to the device to set user_space_running state
    if (write(fd, &state, sizeof(state)) < 0) {
        perror("Error setting user space running state");
    }
}

// Declare the capture_image function
int capture_image(const char *filename);

int main() {
    int fd;
    char buffer[BUFFER_SIZE];
    time_t now;
    char time_str[26];
    struct pollfd pfd;

    printf("Motion sensor program starting...\n");

    // Set up signal handling
    struct sigaction sa;
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    if (sigaction(SIGINT, &sa, NULL) == -1 || 
        sigaction(SIGTERM, &sa, NULL) == -1 ||
        sigaction(SIGQUIT, &sa, NULL) == -1) {
        perror("Unable to set signal handler");
        return 1;
    }

    fd = open(DEVICE_FILE, O_RDWR);  // Open for reading and writing
    if (fd < 0) {
        perror("Unable to open device file");
        return 1;
    }

    // Notify kernel module that user space program is running
    set_user_space_running(fd, 1);

    pfd.fd = fd;
    pfd.events = POLLIN;

    printf("Successfully opened device file, entering main loop\n");

    while (keep_running) {
        int poll_result = poll(&pfd, 1, 500);  // 0.5 second timeout

        if (poll_result < 0) {
            if (errno != EINTR) {
                perror("Poll error");
                break;
            }
        } else if (poll_result > 0) {
            if (pfd.revents & POLLIN) {
                int bytes_read = read(fd, buffer, BUFFER_SIZE);
                if (bytes_read <= 0) {
                    if (bytes_read < 0) {
                        perror("Error reading from device");
                    }
                    continue;
                }
                if (buffer[0] == '1') {
                    // Motion detected, turn on LED
                    printf("Motion detected! Time: %s\n", ctime(&now));
                        const char *filename = "../application/public/captured_image.jpg";
    
                    // Capture an image
                    if (capture_image(filename) == EXIT_SUCCESS) {
                        printf("Image captured successfully and saved as %s\n", filename);
                    } else {
                        printf("Failed to capture image.\n");
                    }
                }
            }
        } else {
            // If no motion detected, keep LED off
            printf("No movement detected\n");
        }
    }

    // Notify kernel module that user space program is stopping
    set_user_space_running(fd, 0);

    if (close(fd) < 0) {
        perror("Error closing device file");
    }
    printf("Program exiting normally\n");
    return 0;
}