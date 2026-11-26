#include <stdio.h>
#include <string.h>
#include <windows.h>

#define BUFFER_SIZE 8192
#define ID_SIZE 129

typedef struct mock_error {
    const char *code;
    const char *category;
} MockError;

// Collection Of IPC errors
static const MockError ERRORS[] = {
    {"INVALID_REQUEST", "protocol"},
    {"UNSUPPORTED_VERSION", "protocol"},
    {"UNSUPPORTED_OPERATION", "protocol"},
    {"MESSAGE_TOO_LARGE", "protocol"},
    {"DATABASE_NOT_OPEN", "semantic"},
    {"DATABASE_ALREADY_OPEN", "semantic"},
    {"DATABASE_ALREADY_EXISTS", "semantic"},
    {"DATABASE_NOT_FOUND", "semantic"},
    {"DATABASE_INVALID_FORMAT", "storage"},
    {"DATABASE_IN_USE", "semantic"},
    {"SQL_SYNTAX_ERROR", "syntax"},
    {"TABLE_NOT_FOUND", "semantic"},
    {"COLUMN_NOT_FOUND", "semantic"},
    {"CONSTRAINT_VIOLATION", "constraint"},
    {"STORAGE_ERROR", "storage"},
    {"RESULT_TOO_LARGE", "internal"},
    {"INTERNAL_ERROR", "internal"}
};

// Helper that fetches the MockError based on the error code
static const MockError *find_error(const char *code) {
    size_t count = sizeof(ERRORS) / sizeof(ERRORS[0]);

    for (size_t i = 0; i < count; i++) {
        if (!strcmp(code, ERRORS[i].code)) {
            return &ERRORS[i];
        }
    }

    return NULL;
}

// Extract error code from the response in the format: MOCK_ERROR:<CODE> 
static int extract_mock_error_code(const char *input, char *code, size_t capacity) {
    const char *start = strstr(input, "MOCK_ERROR:");
    if (!start) {
        return 0;
    }

    start += strlen("MOCK_ERROR:");

    size_t length = 0;
    
    while ((start[length] >= 'A' && start[length] <= 'Z') ||
           (start[length] >= '0' && start[length] <= '9') ||
           start[length] == '_') {
        if (length + 1 > capacity) return 0;
        ++length;
    }

    if (length == 0) {
        return 0;
    }

    memcpy(code, start, length);
    code[length] = '\0';

    return 1;
}

static void send_error(const char *id, const MockError *error) {
    printf("{\"version\":1,\"id\":\"%s\",\"status\":\"error\","
           "\"error\":{\"code\":\"%s\",\"category\":\"%s\","
           "\"message\":\"Mock error: %s\"}}\n", 
            id, error->code, error->category, error->code);
    fflush(stdout);
}


// Extract a simple, unescaped request ID from the test JSON
// This is intentionally not a general-purpose JSON parser
static int extract_id(const char *input, char *id, size_t capacity) {
    const char *start = strstr(input, "\"id\":\"");
    if (!start) {
        return 0;
    }

    start += strlen("\"id\":\"");

    const char *end = strchr(start, '"');
    if (!end) {
        return 0;
    }

    size_t length = (size_t)(end - start);

    if (length == 0 || length >= capacity) {
        return 0;
    }

    memcpy(id, start, length);
    id[length] = '\0';

    return 1;
}

int main(int argc, char *argv[]) {
    // The Node.js - C DBMS communication protocol uses:
    // - stdout for machine-readable application-specific messages
    // - stderr for human-readable debug logs, diagnostics, warnings and errors     
    fprintf(stderr, "Mock MiniDB process started.\n");

    // Send READY
    printf("{\"version\":1,\"type\":\"ready\",\"capabilities\":[\"query\",\"schema\"]}\n");
    fflush(stdout);

    char buffer[BUFFER_SIZE];
    char id[ID_SIZE];
    char error_code[128];

    while (fgets(buffer, sizeof(buffer), stdin) != NULL) {
        fprintf(stderr, "Received input: %s", buffer);

        // Extract request id in order to reflect in the JSON response 
        if (!extract_id(buffer, id, sizeof(id))) {
            fprintf(stderr, "Unable to extract request ID.\n");
            continue;
        }

        // "MOCK_..." codes simulate behaviors during integration testing
        if (strstr(buffer, "MOCK_NO_RESPONSE")) continue;
        if (strstr(buffer, "MOCK_EXIT")) return 1;

        if (strstr(buffer, "MOCK_BAD_JSON")) {
            printf("{invalid json}\n");
            fflush(stdout);
            continue;
        }

        if (strstr(buffer, "MOCK_WRONG_ID")) {
            printf("{\"version\":1,\"id\":\"wrong-id\",\"status\":\"success\","
                   "\"result\":{\"kind\":\"schema\",\"tables\":[],\"indexes\":[]}}\n");
            fflush(stdout);
            continue;
        }

        if (strstr(buffer, "MOCK_INVALID_RESPONSE") != NULL) {
            printf(
                "{\"version\":1,"
                "\"id\":\"req-001\","
                "\"status\":\"success\","
                "\"result\":{\"kind\":\"rows\",\"columns\":[]}}\n"
            );

            fflush(stdout);
            continue;
        }

        if (strstr(buffer, "MOCK_MAXSIZE_RESPONSE") != NULL) {
            char prefix[256];

            int prefix_len = snprintf(
                prefix,
                sizeof(prefix),
                "{\"version\":1,\"id\":\"%s\",\"status\":\"success\","
                "\"result\":{\"kind\":\"rows\",\"columns\":[\"data\"],"
                "\"rows\":[[\"",
                id
            );

            const char *suffix = "\"]],\"rowCount\":1}}\n";

            if (prefix_len < 0 || (size_t)prefix_len >= sizeof(prefix)) {
                return 0;
            }

            size_t suffix_len = strlen(suffix) - 1; // Exclude LF
            size_t padding = (8 * 1024 * 1024) - (size_t)prefix_len - suffix_len;

            fwrite(prefix, 1, (size_t)prefix_len, stdout);

            for (size_t i = 0; i < padding; i++) {
                fputc('A', stdout);
            }

            fputs(suffix, stdout);
            fflush(stdout);
            continue;
        }

        if (strstr(buffer, "MOCK_OVERSIZED_RESPONSE") != NULL) {
            const size_t oversized = 8 * 1024 * 1024 + 1;

            for (size_t i = 0; i < oversized; i++) {
                putchar('A');
            }

            putchar('\n');
            fflush(stdout);
            continue;
        }

        if (strstr(buffer, "MOCK_OVERSIZED_INCOMPLETE_FRAME") != NULL) {
            char buffer[4906];
            memset(buffer, 'A', sizeof(buffer));

            size_t remaining = 8 * 1024 * 1024 + 1;

            while (remaining > 0) {
                size_t amount = remaining < sizeof(buffer) ? remaining : sizeof(buffer);

                if (fwrite(buffer, 1, amount, stdout) != amount) {
                    return 0;
                }

                remaining -= amount;
            }

            fflush(stdout);
            return 1;
        }

        if (strstr(buffer, "MOCK_UNSOLICITED_RESPONSE") != NULL) {
            const char *response =
                "{\"version\":1,"
                "\"id\":\"req-001\","
                "\"status\":\"success\","
                "\"result\":{\"kind\":\"schema\",\"tables\":[],\"indexes\":[]}}\n";

            // First response: legitimate response to the pending request.
            fputs(response, stdout);

            // Second response: unsolicited, because the first one
            // has already satisfied the pending request.
            fputs(response, stdout);

            fflush(stdout);
            continue;
        }

        if (strstr(buffer, "MOCK_INCOMPLETE_FRAME") != NULL) {
            fprintf(stdout,
                "{\"version\":1,\"id\":\"req-001\",\"status\":\"success\""
            );

            fflush(stdout);

            return 1;
        }

        if (strstr(buffer, "MOCK_SPLIT_RESPONSE") != NULL) {
            printf("{\"version\":1,\"id\":\"%s\",\"status\":\"success\",", id);
            fflush(stdout);

            Sleep(500); // 500ms wait between chunks

            printf("\"result\":{\"kind\":\"schema\",\"tables\":[],\"indexes\":[]}}\n");
            fflush(stdout);
            return 1;
        }

        if (strstr(buffer, "MOCK_SPLIT_UTF8") != NULL) {
            printf(
                "{\"version\":1,\"id\":\"%s\",\"status\":\"success\","
                "\"result\":{\"kind\":\"command\",\"command\":\"ΕΝΤ",
                id
            );
            fflush(stdout);

            Sleep(500); // 500ms wait between chunks

            printf("ΟΛΗ\",\"rowsAffected\":0}}\n");
            fflush(stdout);
            return 1;
        }

        if (strstr(buffer, "MOCK_INVALID_UTF8") != NULL) {
            printf(
                "{\"version\":1,\"id\":\"%s\",\"status\":\"success\","
                "\"result\":{\"kind\":\"command\",\"command\":\"",
                id
            );

            unsigned char invalid_utf8[] = {0xC3, 0x28};

            fwrite(invalid_utf8, 1, sizeof(invalid_utf8), stdout);
            
            printf("\",\"rowsAffected\":0}}\n");
            fflush(stdout);
            return 1;
        }

        if (strstr(buffer, "MOCK_CLRF_RESPONSE") != NULL) {
            printf("{\"version\":1,\"id\":\"%s\",\"status\":\"success\","
                   "\"result\":{\"kind\":\"schema\",\"tables\":[],\"indexes\":[]}}\r\n", id);
            fflush(stdout);
            return 1;
        }

        if (extract_mock_error_code(buffer, error_code, sizeof(error_code))) {
            const MockError *error = find_error(error_code);

            if (!error) {
                error = find_error("INVALID_REQUEST");
            }

            send_error(id, error);
        }

        if (strstr(buffer, "MOCK_ERROR")) {
            send_error(id, find_error("TABLE_NOT_FOUND"));
            continue;
        }

        // Successful schema response
        if (strstr(buffer, "\"type\":\"schema\"")) {
            printf("{\"version\":1,\"id\":\"%s\",\"status\":\"success\","
                   "\"result\":{\"kind\":\"schema\",\"tables\":[],\"indexes\":[]}}\n", id);
            fflush(stdout);
            continue;
        }

        // Successful query response
        if (strstr(buffer, "\"type\":\"query\"")) {
            printf("{\"version\":1,\"id\":\"%s\",\"status\":\"success\","
                   "\"result\":{\"kind\":\"rows\","
                   "\"columns\":[{\"name\":\"id\",\"type\":\"INTEGER\"}],"
                   "\"rows\":[[1],[2]],\"rowCount\":2}}\n", id);
            fflush(stdout);
            continue;
        }

        send_error(id, find_error("UNSUPPORTED_OPERATION"));
    }

    fprintf(stderr, "Mock MiniDB process exiting.\n");
    return 0;
}