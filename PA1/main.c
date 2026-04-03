#include "pa1.h"

// Write a short error message to stderr.
static int write_stderr(const char *message) {
    return write_full(2, message, cstr_len(message));
}

// Build the index, read queries, and print one result line per query.
int main(int argc, char **argv) {
    int input_fd;
    Index *index;
    LineReader query_reader;
    ByteVec query_buf;
    QueryScratch query_state;
    OutBuf out;

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

    query_buf.data = NULL;
    query_buf.len = 0;
    query_buf.cap = 0;
    query_scratch_init(&query_state);
    line_reader_init(&query_reader, 0);
    outbuf_init(&out, 1);

    while (1) {
        int query_had_newline;
        int query_status = line_reader_read_line(&query_reader, &query_buf, &query_had_newline);

        if (query_status < 0) {
            write_stderr("failed to read query\n");
            outbuf_flush(&out);
            bytevec_free(&query_buf);
            query_scratch_free(&query_state);
            index_free(index);
            free(index);
            return 1;
        }
        if (query_status == 0) {
            break;
        }
        if (query_buf.len > QUERY_LIMIT) {
            write_stderr("query is too long\n");
            outbuf_flush(&out);
            bytevec_free(&query_buf);
            query_scratch_free(&query_state);
            index_free(index);
            free(index);
            return 1;
        }
        if (slice_equals_cstr(query_buf.data, query_buf.len, "PA1EXIT")) {
            break;
        }
        if (handle_query(index, query_buf.data, query_buf.len, &out, &query_state) < 0) {
            write_stderr("failed to execute query\n");
            outbuf_flush(&out);
            bytevec_free(&query_buf);
            query_scratch_free(&query_state);
            index_free(index);
            free(index);
            return 1;
        }
        if (outbuf_flush(&out) < 0) {
            write_stderr("failed to flush stdout\n");
            bytevec_free(&query_buf);
            query_scratch_free(&query_state);
            index_free(index);
            free(index);
            return 1;
        }
        (void) query_had_newline;
    }

    outbuf_flush(&out);
    bytevec_free(&query_buf);
    query_scratch_free(&query_state);
    index_free(index);
    free(index);
    return 0;
}
