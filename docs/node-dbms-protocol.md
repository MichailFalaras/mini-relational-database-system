# BaseQL - Node.js ↔ MiniDB Communication Protocol

## 1. Introduction

### 1.1 Purpose

This document defines version 1 of the inter-process communication (IPC) protocol between the BaseQL Node.js backend and the MiniDB relational database management system (RDBMS) implemented in C.

The protocol defines machine-readable requests, responses, errors, metadata, and process lifecycle messages. It is independent of the public HTTP API and does not rely on parsing human-readable CLI output.

### 1.2 Architecture

```text
React frontend
      | HTTP/JSON
      v
Node.js / Express backend
      | JSON Lines over child-process stdin/stdout
      v
MiniDB C process
      | internal engine APIs
      v
Local MiniDB database files
```

Node.js owns the child process and translates application operations into protocol requests. MiniDB owns SQL processing and database storage. Diagnostic output is sent to `stderr`, never `stdout`.

### 1.3 Process Model

Version 1 uses one persistent MiniDB child process per Node.js backend instance. The backend sends at most one request at a time and waits for its response before sending another. A request produces exactly one terminal response. Startup readiness is the only unsolicited protocol message.

The contract can be implemented initially by a small protocol-compatible C test executable while the full MiniDB engine remains under development.

## 2. Transport and Message Framing

- **Transport:** Node.js writes to MiniDB's standard input (`stdin`). MiniDB writes protocol messages to standard output (`stdout`).
- **Diagnostics:** Logs, warnings, and debugging information go to standard error (`stderr`). No prompts, banners, or debug prints are permitted on stdout.
- **Encoding:** UTF-8, without a byte-order mark (BOM).
- **Framing:** Each message is exactly one compact JSON object followed by a newline byte (`\n`, `0x0A`). Newlines inside JSON string values must be encoded using the JSON escape sequence `\n` (a backslash followed by the letter `n`), not a literal newline byte.
- **Reading:** Both sides buffer incoming bytes until a complete newline-terminated frame is available. A single stream read may contain a partial message, one message, or several messages.
- **Line endings:** Send `\n`. Receivers may remove one trailing `\r` immediately before `\n` for compatibility.
- **Maximum frame size:** 8 MiB (8,388,608 bytes), excluding the terminating newline. Oversized requests receive `MESSAGE_TOO_LARGE` when recoverable. Oversized responses are treated by Node.js as a protocol failure.
- **Connection termination:** EOF is not a message delimiter. An incomplete frame at EOF is a protocol failure.

All messages must be valid JSON objects. JSON arrays, scalars, and blank lines are invalid top-level frames. The transport is intended for trusted local child-process communication. Protocol input must still be validated.

## 3. Protocol Conventions

### 3.1 Version and Request IDs

Every message contains `version: 1`. Requests contain a non-empty string `id` (maximum 128 UTF-8 bytes), which the response must echo unchanged. IDs must be unique among outstanding requests. The backend should generate a fresh ID for each request.

A request envelope has the fields `version`, `id`, `type`, and `payload`. A terminal response has `version`, `id`, `status`, and either `result` or `error`.

Unknown top-level fields may be ignored by compatible implementations, but required fields and their types must be validated. Unknown `type` values produce `UNSUPPORTED_OPERATION`. Unsupported protocol versions produce `UNSUPPORTED_VERSION` when a valid ID can be recovered.

### 3.2 Ordering and Correlation

Version 1 allows only one outstanding request. MiniDB must emit exactly one terminal response for each accepted request and echo its ID. Node.js must reject responses with an unexpected ID or invalid envelope. Responses are not streamed or interleaved.

The `ready` startup event is not a response and has no request ID. The `shutdown` operation is the final request in a normal session.

### 3.3 Result and Error Semantics

`status` is either `success` or `error`:

- `success` requires `result` and must not contain `error`.
- `error` requires `error` and must not contain `result`.
- `execution` is optional and describes a completed SQL operation when available.

A syntactically valid protocol response can still describe a failed database operation. Conversely, malformed JSON, a wrong response ID, EOF during a response, or a crashed child process are transport/process failures, not ordinary database errors.

## 4. Request Messages

### 4.1 General Format

```json
{"version":1,"id":"req-001","type":"query","payload":{"sql":"SELECT id, name FROM users;"}}
```

### 4.2 Supported Operation Names

| `type` | Required payload | Purpose |
| --- | --- | --- |
| `query` | `sql` | Execute one SQL statement |
| `schema` | Empty object `{}` | Retrieve database schema metadata |
| `shutdown` | Empty object `{}` | Request graceful process termination |
| `open_database` | `path` | Open an existing database as the active database |
| `close_database` | Empty object `{}` | Close the currently active database |
| `create_database` | `name`, `path` | Create and open a new database |
| `delete_database` | `path` | Permanently delete an existing, closed database |

`sql`, `name`, and `path` must be non-empty strings. SQL must contain one statement under version 1. Multiple-statement batches are not supported. Operation support is advertised in the startup `capabilities` array. If a recognized operation is not implemented, return `UNSUPPORTED_OPERATION`.

## 5. Successful Responses

### 5.1 SELECT / Row-Producing Queries

```json
{
  "version": 1,
  "id": "req-001",
  "status": "success",
  "result": {
    "kind": "rows",
    "columns": [
      {"name": "id", "type": "INTEGER"},
      {"name": "name", "type": "TEXT"}
    ],
    "rows": [[1, "Alice"], [2, null]],
    "rowCount": 2
  },
  "execution": {"durationMs": 1.42}
}
```

- `columns` is an ordered array. Duplicate column names are permitted.
- `rows` is an array of arrays, with each row matching the column count and order.
- `rowCount` is the number of rows included in `rows`, not an estimate or total count of rows elsewhere.
- Empty results use `rows: []`, `rowCount: 0`, and still include column metadata.
- Version 1 does not stream results. A result must fit within the frame-size limit. Queries producing larger responses fail with `RESULT_TOO_LARGE` rather than returning truncated data.

### 5.2 Non-SELECT / Command Responses

```json
{
  "version": 1,
  "id": "req-002",
  "status": "success",
  "result": {
    "kind": "command",
    "command": "UPDATE",
    "rowsAffected": 3
  },
  "execution": {"durationMs": 0.85}
}
```

`command` identifies the executed statement class, such as `INSERT`, `UPDATE`, `DELETE`, `CREATE_TABLE`, or `DROP_INDEX`. `rowsAffected` is a non-negative integer for row-changing commands, or `null` where not applicable (for example, DDL). It is not the number of rows returned by SELECT.

### 5.3 Execution Metadata

`execution.durationMs` is a non-negative finite JSON number measuring elapsed time spent executing the operation inside MiniDB, excluding Node.js HTTP handling and inter-process transfer time. Additional execution fields may be added in later versions.

## 6. Error Responses

```json
{
  "version": 1,
  "id": "req-003",
  "status": "error",
  "error": {
    "code": "TABLE_NOT_FOUND",
    "category": "semantic",
    "message": "Table 'customers' does not exist."
  }
}
```

`code` is a stable machine-readable uppercase identifier. `category` is one of `protocol`, `syntax`, `semantic`, `constraint`, `storage`, or `internal`. `message` is intended for diagnostics or display and must never be parsed to determine error behavior.

Initial error codes:

| Code | Category | Meaning |
| --- | --- | --- |
| `INVALID_REQUEST` | `protocol` | Malformed or missing required request fields |
| `UNSUPPORTED_VERSION` | `protocol` | Unsupported protocol version |
| `UNSUPPORTED_OPERATION` | `protocol` | Unknown or unavailable operation |
| `MESSAGE_TOO_LARGE` | `protocol` | Request exceeds the frame-size limit |
| `DATABASE_NOT_OPEN` | `semantic` | No database is currently active |
| `DATABASE_ALREADY_OPEN` | `semantic` | An operation requires no active database, but one is open |
| `DATABASE_ALREADY_EXISTS` | `semantic` | Database creation would overwrite an existing file |
| `DATABASE_NOT_FOUND` | `semantic` | The requested database file does not exist |
| `DATABASE_INVALID_FORMAT` | `storage` | The file is not a valid or supported MiniDB database |
| `DATABASE_IN_USE` | `semantic` | The requested database cannot be deleted because it is open |
| `SQL_SYNTAX_ERROR` | `syntax` | Invalid SQL syntax |
| `TABLE_NOT_FOUND` | `semantic` | Unknown table |
| `COLUMN_NOT_FOUND` | `semantic` | Unknown column |
| `CONSTRAINT_VIOLATION` | `constraint` | Constraint check failed |
| `STORAGE_ERROR` | `storage` | Database storage operation failed |
| `RESULT_TOO_LARGE` | `internal` | Response cannot fit in a protocol frame |
| `INTERNAL_ERROR` | `internal` | Unexpected engine failure |

The C implementation may map its internal status enums to these external codes. If a malformed request has no recoverable ID, MiniDB may terminate the protocol session rather than inventing an ID. A process crash or invalid response is detected by Node.js independently and does not require a structured error message from MiniDB.

## 7. Database Values and Types

The column `type` describes the logical SQL data type. The corresponding row element contains its serialized value.

| MiniDB logical value | JSON representation |
| --- | --- |
| SQL `NULL` | `null` |
| Boolean | `true` or `false` |
| Text | UTF-8 JSON string |
| Signed integer within JavaScript's safe range | JSON integer |
| Signed 64-bit integer outside the safe range | `{"$type":"int64","value":"9223372036854775807"}` |
| Finite floating-point value | JSON number |
| Binary value | `{"$type":"bytes","encoding":"base64","value":"AQID"}` |

Integer values outside `[-9007199254740991, 9007199254740991]` must use the tagged decimal-string representation to avoid JavaScript precision loss. NaN and positive/negative infinity are not valid JSON numbers. If the engine supports them, it must return a structured error until a dedicated encoding is specified.

Tagged values are recognized only when their object has exactly the documented fields and a recognized `$type`. Regular SQL values are never serialized as arbitrary JSON objects. Node.js must validate tagged values before converting them to application-level values. Text must be valid UTF-8 and correctly JSON-escaped.

## 8. Schema Metadata Response

Request:

```json
{"version":1,"id":"req-004","type":"schema","payload":{}}
```

Response:

```json
{
  "version": 1,
  "id": "req-004",
  "status": "success",
  "result": {
    "kind": "schema",
    "tables": [
      {
        "name": "users",
        "columns": [
          {"name": "id", "type": "INTEGER", "nullable": false, "primaryKey": true},
          {"name": "name", "type": "TEXT", "nullable": true, "primaryKey": false}
        ]
      }
    ],
    "indexes": [
      {"name": "pk_users", "table": "users", "columns": ["id"], "unique": true, "primary": true}
    ]
  }
}
```

`tables` and `indexes` are arrays and may be empty. Column order reflects the table schema. Index column order reflects the index key order. Constraint details beyond `nullable` and `primaryKey` may be added when their representation is finalized against MiniDB's metadata structures.

## 9. Database Operations

### Database Lifecycle Semantics

Version 1 supports exactly one active database per MiniDB process.

- No database is active immediately after process startup.
- `create_database` creates a new database file and opens it on success.
- `open_database` opens an existing database file.
- `close_database` closes the currently active database and releases its resources.
- `delete_database` permanently removes an existing database file that is not currently open.
- `query` and `schema` operate exclusively on the active database.
- Creating or opening a database while another database is active returns `DATABASE_ALREADY_OPEN`.
- Querying, retrieving schema metadata, or closing a database when none is active returns `DATABASE_NOT_OPEN`.
- Creating a database whose target file already exists returns `DATABASE_ALREADY_EXISTS`.
- Opening or deleting a nonexistent database returns `DATABASE_NOT_FOUND`.
- Deleting the currently active database returns `DATABASE_IN_USE`.
- Failed database creation or opening must not leave a partially active database.

Database switching is performed by sending `close_database` followed by `open_database`. If opening the replacement database fails, no database remains active. Node.js may attempt to reopen the previous database, but recovery is not guaranteed.

Node.js maintains the application-level database registry, maps registered database identifiers to validated filesystem paths, and serializes database lifecycle operations with SQL requests. MiniDB manages physical database files and the active database state.

### 9.1 Database Opening

Request:

```json
{"version":1,"id":"req-010","type":"open_database","payload":{"path":"databases/inventory.db"}}
```

Response:

```json
{
  "version": 1,
  "id": "req-010",
  "status": "success",
  "result": {
    "kind": "database",
    "name": "inventorydb",
    "state": "open"
  }
}
```

The path should be resolved and validated by the backend. The C engine must also handle invalid or inaccessible files.

The operation succeeds only when no database is currently active. On success, the opened database becomes the active database. Opening an invalid MiniDB file returns `DATABASE_INVALID_FORMAT`. The operation must not create a missing database file.

### 9.2 Database Closing

Request:

```json
{"version":1,"id":"req-011","type":"close_database","payload":{}}
```

Response:

```json
{
  "version": 1,
  "id": "req-011",
  "status": "success",
  "result": {
    "kind": "database",
    "state": "closed"
  }
}
```

Because there's only 1 active database, `close_database` doesn't need a database identifier.

Closing a database releases its engine resources and leaves the process without an active database. The operation returns `DATABASE_NOT_OPEN` when no database is active.

### 9.3 Database Creation

Request:

```json
{"version":1,"id":"req-012","type":"create_database","payload":{"name":"inventorydb","path":"databases/inventory.db"}}
```

Response:

```json
{
  "version": 1,
  "id": "req-012",
  "status": "success",
  "result": {
    "kind": "database",
    "name": "inventorydb",
    "state": "open"
  }
}
```

Successful creation also opens the new database, making it the active database. The operation must not overwrite an existing database file. Creating a database requires no database to be active.

If creation fails, the process must not report a partially initialized database as active. Any partially created file must be cleaned up or explicitly reported as requiring recovery.

### 9.4 Database Deletion

Request:

```json
{"version":1,"id":"req-013","type":"delete_database","payload":{"path":"databases/inventory.db"}}
```

Response:

```json
{
  "version": 1,
  "id": "req-013",
  "status": "success",
  "result": {
    "kind": "database",
    "state": "deleted"
  }
}
```

Deletion permanently removes the specified database file. The target database must not be currently open. Attempting to delete the active database returns `DATABASE_IN_USE`.

Node.js must authorize the operation and validate that the target path belongs to an allowed, registered database. The application-level database registry is updated only after MiniDB confirms successful deletion.

## 10. Process Lifecycle

### 10.1 Startup and Readiness

1. Node.js spawns the MiniDB executable with stdin, stdout, and stderr connected to separate pipes.
2. MiniDB initializes its protocol handler and required engine resources.
3. MiniDB emits exactly one unsolicited `ready` frame on stdout:

```json
{"version":1,"type":"ready","capabilities":["query", "schema", "open_database", "close_database", "create_database", "delete_database"]}
```

4. Node.js validates the frame and marks the process available only after receiving it.

`capabilities` lists only implemented operations. `shutdown` is mandatory and need not appear in the array. A process that exits, closes stdout, emits an invalid first frame, or exceeds the configured startup timeout before `ready` is considered unavailable. The initial recommended startup timeout is 5 seconds.

The example above represents a MiniDB implementation supporting all listed operations. Implementations that support only a subset must advertise only those implemented capabilities.

### 10.2 Graceful Shutdown

Node.js stops accepting new DBMS requests, waits for any outstanding operation to complete, and then sends:

```json
{"version":1,"id":"req-006","type":"shutdown","payload":{}}
```

MiniDB releases its owned resources and responds:

```json
{"version":1,"id":"req-006","status":"success","result":{"kind":"shutdown"}}
```

MiniDB then exits with status code `0`. Node.js waits for both the acknowledgment and process exit. If shutdown exceeds the configured timeout (initial recommendation: 5 seconds), Node.js terminates the child process and records the shutdown as abnormal.

### 10.3 Unexpected Termination

Unexpected child-process exit, broken pipes, or stdout EOF before a complete response make the process unavailable. Node.js fails any outstanding request with an application-level process-unavailable error. Automatic restart policy belongs to the process-manager implementation. Restarting must create a new session, await a fresh `ready` event, and re-establish any database state.

## 11. Protocol Limits and Failure Rules

- Only one request may be outstanding per MiniDB process in version 1.
- Requests and responses are limited to one newline-terminated JSON frame each.
- Maximum frame size is 8 MiB, excluding the newline.
- All required fields must be validated before an operation is executed.
- Unexpected response IDs, invalid response shapes, and malformed JSON from MiniDB are protocol failures.
- A database error response does not terminate the process unless the engine cannot safely continue.
- Timeouts do not imply a query was rolled back. Node.js must not blindly retry a mutating query after an ambiguous timeout or process failure.
- Standard output is exclusively for protocol frames. Standard error is exclusively for diagnostics.
- The backend is responsible for authorizing database operations and mapping user-facing database identifiers to engine resources.

## 12. Example Exchange

The following is a logical exchange. Each JSON object is transmitted as one compact line followed by `\n`.

**MiniDB → Node.js (stdout, after startup):**

```json
{"version":1,"type":"ready","capabilities":["query", "schema", "open_database", "close_database", "create_database", "delete_database"]}
```

**Node.js → MiniDB (stdin):**

```json
{"version":1,"id":"req-100","type":"open_database","payload":{"path":"databases/inventory.db"}}
```

**MiniDB → Node.js (stdout):**

```json
{"version":1,"id":"req-100","status":"success","result":{"kind":"database","name":"inventorydb","state":"open"}}
```

**Node.js → MiniDB (stdin):**

```json
{"version":1,"id":"req-101","type":"query","payload":{"sql":"SELECT id FROM users;"}}
```

**MiniDB → Node.js (stdout):**

```json
{"version":1,"id":"req-101","status":"success","result":{"kind":"rows","columns":[{"name":"id","type":"INTEGER"}],"rows":[[1],[2]],"rowCount":2},"execution":{"durationMs":0.7}}
```

**Node.js → MiniDB (stdin):**

```json
{"version":1,"id":"req-102","type":"shutdown","payload":{}}
```

**MiniDB → Node.js (stdout):**

```json
{"version":1,"id":"req-102","status":"success","result":{"kind":"shutdown"}}
```

**MiniDB:** exits with code `0`.

---

**Implementation note:** This specification defines the intended version 1 contract, not a statement that all listed operations are already implemented. The initial Node.js process manager may be tested with a protocol-compatible C stub. Database lifecycle operations and other extensions must be specified before they are implemented across the process boundary.
