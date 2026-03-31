#include "pa1.h"

// Write a short error message to stderr.
static int write_stderr(const char *message) {
    return write_full(2, message, cstr_len(message));
}

// Build the index, read queries, and print one result line per query.
int main(int argc, char **argv) {
    int input_fd;
    Index *index;
    LineReader stdin_reader;
    ByteVec query;
    QueryScratch scratch;
    OutBuf stdout_buf;

    if (argc != 2) {
        write_stderr("usage: ./pa1 <input-file>\n");
        return 1;
    }

    input_fd = open(argv[1], O_RDONLY);
    if (input_fd < 0) {
        write_stderr("failed to open input file\n");
        return 1;
    }

    index = (Index *) malloc(sizeof(Index));
    if (index == NULL) {
        write_stderr("failed to allocate index\n");
        close(input_fd);
        return 1;
    }

    index->input_fd = -1;
    index->line_fd = -1;
    if (index_init(index, input_fd) < 0) {
        write_stderr("failed to initialize index\n");
        close(input_fd);
        free(index);
        return 1;
    }
    if (index_build(index) < 0) {
        write_stderr("failed to build index\n");
        index_free(index);
        free(index);
        return 1;
    }

    query.data = NULL;
    query.len = 0;
    query.cap = 0;
    query_scratch_init(&scratch);
    line_reader_init(&stdin_reader, 0);
    outbuf_init(&stdout_buf, 1);

    while (1) {
        int had_newline;
        int read_status = line_reader_read_line(&stdin_reader, &query, &had_newline);

        if (read_status < 0) {
            write_stderr("failed to read query\n");
            outbuf_flush(&stdout_buf);
            bytevec_free(&query);
            query_scratch_free(&scratch);
            index_free(index);
            free(index);
            return 1;
        }
        if (read_status == 0) {
            break;
        }
        if (query.len > PA1_QUERY_LIMIT) {
            write_stderr("query is too long\n");
            outbuf_flush(&stdout_buf);
            bytevec_free(&query);
            query_scratch_free(&scratch);
            index_free(index);
            free(index);
            return 1;
        }
        if (slice_equals_cstr(query.data, query.len, "PA1EXIT")) {
            break;
        }
        if (handle_query(index, query.data, query.len, &stdout_buf, &scratch) < 0) {
            write_stderr("failed to execute query\n");
            outbuf_flush(&stdout_buf);
            bytevec_free(&query);
            query_scratch_free(&scratch);
            index_free(index);
            free(index);
            return 1;
        }
        if (outbuf_flush(&stdout_buf) < 0) {
            write_stderr("failed to flush stdout\n");
            bytevec_free(&query);
            query_scratch_free(&scratch);
            index_free(index);
            free(index);
            return 1;
        }
        query_scratch_free(&scratch);
        query_scratch_init(&scratch);
    }

    outbuf_flush(&stdout_buf);
    bytevec_free(&query);
    query_scratch_free(&scratch);
    index_free(index);
    free(index);
    return 0;
}
