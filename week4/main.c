// Hint for PA2: You can use this as the base code for cat, head, and tail

#include <unistd.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdbool.h>

enum { BATCH_SIZE = 4028 };

char do_not_touch_this_array[21]; // Global variables should typically be avoided, but this variable is used to make it easier to use to_string function and avoids the use of malloc (which you should use in the future but avoided in this exercise for simplicity).
char* to_string(uint64_t integer);
size_t w4_strlen(const char* str);
void get_output_filename(const char* input_filename, char* output_filename);

// Add more functions here if needed
ssize_t write_all(int fd, const char* buffer, size_t length);
int write_line_prefix(int fd, uint64_t line_number);

int main(int argc, const char* argv[]) {
    if (argc < 2) {
        const char* err_msg = "argument error\n";
        write(STDOUT_FILENO, err_msg, w4_strlen(err_msg));
        return 1;
    }

    const char* input_filename = argv[1];
    char output_filename[w4_strlen(input_filename) + 4];
    get_output_filename(input_filename, output_filename);
    uint64_t line_number = 1;
    bool at_line_start = true;
    char buffer[BATCH_SIZE];
    int input_fd = open(input_filename, O_RDONLY);

    if (input_fd < 0) {
        const char* err_msg = "input open error\n";
        write(STDOUT_FILENO, err_msg, w4_strlen(err_msg));
        return 1;
    }

    int output_fd = open(output_filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (output_fd < 0) {
        const char* err_msg = "output open error\n";
        write(STDOUT_FILENO, err_msg, w4_strlen(err_msg));
        close(input_fd);
        return 1;
    }

    while (true) {
        ssize_t bytes_read = read(input_fd, buffer, BATCH_SIZE);
        if (bytes_read < 0) {
            const char* err_msg = "copy error\n";
            write(STDOUT_FILENO, err_msg, w4_strlen(err_msg));
            close(input_fd);
            close(output_fd);
            return 1;
        }
        if (bytes_read == 0) break;

        for (ssize_t i = 0; i < bytes_read; i++) {
            if (at_line_start) {
                if (write_line_prefix(output_fd, line_number) < 0) {
                    const char* err_msg = "copy error\n";
                    write(STDOUT_FILENO, err_msg, w4_strlen(err_msg));
                    close(input_fd);
                    close(output_fd);
                    return 1;
                }
                at_line_start = false;
            }

            if (write_all(output_fd, &buffer[i], 1) < 0) {
                const char* err_msg = "copy error\n";
                write(STDOUT_FILENO, err_msg, w4_strlen(err_msg));
                close(input_fd);
                close(output_fd);
                return 1;
            }

            if (buffer[i] == '\n') {
                line_number++;
                at_line_start = true;
            }
        }
    }

    close(input_fd);
    close(output_fd);
    return 0;
}

// helper functions
ssize_t write_all(int fd, const char* buffer, size_t length) {
    size_t written = 0;

    while (written < length) {
        ssize_t result = write(fd, buffer + written, length - written);
        if (result <= 0) return -1;
        written += (size_t)result;
    }

    return (ssize_t)written;
}

int write_line_prefix(int fd, uint64_t line_number) {
    char* number_text = to_string(line_number);

    if (write_all(fd, number_text, w4_strlen(number_text)) < 0) return -1;
    if (write_all(fd, " | ", 3) < 0) return -1;
    return 0;
}

char* to_string(uint64_t integer) {
    do_not_touch_this_array[20] = '\0';
    for (int i = 19; i >= 0; i--) {
        do_not_touch_this_array[i] = (integer % 10) + '0';
        integer /= 10;
        if (integer == 0) return (char*)&do_not_touch_this_array[i];
    }
    return (char*)do_not_touch_this_array;
}

size_t w4_strlen(const char* str) {
    if (str == NULL) return 0;
    size_t len;
    for (len = 0; str[len] != '\0'; len++);
    return len;
}

void get_output_filename(const char* input_filename, char* output_filename) {
    size_t input_i=0;
    size_t input_len = w4_strlen(input_filename);

    for (input_i=0; input_i < input_len; input_i++) {
        output_filename[input_i] = input_filename[input_i];
    }

    size_t output_i = input_i;
    output_filename[output_i++] = '.';
    output_filename[output_i++] = 'n';
    output_filename[output_i++] = 'o';
    output_filename[output_i++] = '\0';
}
