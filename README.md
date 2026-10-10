# Lightweight Relational Database Engine (C++)

A relational database engine built from scratch in C++17 — SQL front end,
page-based storage layer with a buffer pool, and a B+Tree index — with no
external database libraries. Built as a systems-programming portfolio
project.

```
sql> CREATE TABLE students (id INT PRIMARY KEY, name TEXT, gpa FLOAT);
Table 'students' created (3 columns).

sql> INSERT INTO students VALUES (1, 'Alice', 3.9);
1 row inserted into 'students' (page 0, slot 0).

sql> SELECT * FROM students WHERE id = 1;
+----+-------+-----+
| id | name  | gpa |
+----+-------+-----+
| 1  | Alice | 3.9 |
+----+-------+-----+
1 row(s) returned.  [primary-key index used]
(0.012 ms)
```

## Features

- **SQL front end**: hand-written lexer + recursive-descent parser for
  `CREATE TABLE`, `INSERT`, `SELECT` (with projection and `WHERE`),
  `UPDATE`, and `DELETE`, including `AND`/`OR`-chained filters and the
  standard comparison operators.
- **Page-based storage engine**: fixed 4KB slotted pages, a `DiskManager`
  that persists tables as flat binary heap files, and a `BufferPoolManager`
  implementing LRU-based caching with pin counts and dirty-page write-back —
  the same core design used by production engines like SQLite/Postgres.
- **B+Tree indexing**: an in-memory B+Tree indexes every table's primary
  key, giving `O(log n)` point lookups instead of a full table scan for
  equality queries on the key (`SELECT ... WHERE id = X`, `UPDATE`, and
  `DELETE` all take this fast path automatically — the CLI reports
  `[primary-key index used]` when it does).
- **Persistence**: a text catalog records table schemas; table data lives
  in per-table page files. Restarting the engine reopens all tables with
  their data intact and rebuilds each index from the on-disk data.
- **CLI**: an interactive REPL (with per-query timing) and a batch mode for
  running `.sql` scripts (`./dbengine tests/test_queries.sql`).

## Architecture

```
SQL text
   │
   ▼
 Lexer            (src/lexer.cpp)     — tokenizes SQL into IDENT/NUMBER/STRING/SYMBOL
   │
   ▼
 Parser           (src/parser.cpp)    — recursive descent → AST (src/ast.h)
   │
   ▼
 Executor         (src/executor.cpp)  — type-checks values, evaluates WHERE,
   │                                     picks index scan vs. full scan
   ▼
 Table            (src/table.cpp)     — row (de)serialization, ties storage + index together
   │                          │
   ▼                          ▼
 BufferPoolManager        BPlusTree   (src/bptree.cpp) — PK index, in-memory
   │  (LRU cache)
   ▼
 DiskManager       (src/storage.cpp)  — reads/writes fixed 4KB pages to disk
   │
   ▼
 table_name.tbl file on disk
```

### Storage layer: slotted pages

Each page is a fixed 4096-byte block:

```
[ header: num_slots | free_space_end ]
[ slot 0: offset,len ][ slot 1: offset,len ] ...   (slot array grows down)
......................... free space ..........................
[ record bytes ][ record bytes ] ...                (records grow up from the
                                                      end of the page)
```

Deletes tombstone a slot (`len = -1`) instead of physically compacting the
page, so slot ids stay stable. Rows are packed into pages via a first-fit
scan over existing pages before a new page is allocated.

### Buffer pool

`BufferPoolManager` keeps a fixed number of pages resident (configurable,
default 16 frames per table) and evicts the least-recently-used *unpinned*
page when the pool is full, flushing it to disk first if dirty. This keeps
memory bounded even for tables much larger than RAM while avoiding a disk
round-trip on every access.

### B+Tree index

`BPlusTree` is a from-scratch, in-memory B+Tree (`src/bptree.cpp`) mapping
primary-key values to `RowId {page, slot}`. It implements textbook
insert-with-split (leaf split → propagate median key to parent → recursive
internal split up to a new root) and point/range search. Every table with a
declared `PRIMARY KEY` gets one automatically; the executor uses it whenever
a query's `WHERE` clause is a single primary-key equality check.

*Known simplification*: `remove()` deletes the key from its leaf but does
not perform merge/redistribution rebalancing after deletion. Search
correctness is unaffected; node occupancy just isn't kept minimally bounded
under heavy delete-heavy workloads. This is called out explicitly because
it's the one place a "real" B+Tree implementation goes further.

### Row encoding

Rows are serialized per the table's schema: `INT` → 8-byte `int64_t`,
`FLOAT` → 8-byte `double`, `TEXT` → 4-byte length + UTF-8 bytes. `NULL`
values are out of scope.

## Building

Requires a C++17 compiler (g++ or clang++) and `make`.

```bash
make            # builds ./dbengine
make clean      # removes build artifacts and the ./data directory
```

## Running

```bash
./dbengine                        # interactive REPL, data stored in ./data
./dbengine tests/test_queries.sql # run a script of ';'-terminated statements
./dbengine --data-dir /tmp/mydb   # use a different data directory
```

Example session:

```sql
CREATE TABLE students (id INT PRIMARY KEY, name TEXT, gpa FLOAT);
INSERT INTO students VALUES (1, 'Alice', 3.9);
INSERT INTO students VALUES (2, 'Bob', 3.2);
SELECT * FROM students WHERE gpa >= 3.5;
UPDATE students SET gpa = 4.0 WHERE id = 1;
DELETE FROM students WHERE id = 2;
```

## Design notes / trade-offs (for discussion in an interview)

- **Why slotted pages?** Fixed-size pages with an indirection layer (the
  slot array) let records be variable length and deleted/tombstoned without
  shifting every other record's offset — the same trick real engines use.
- **Why a first-fit page scan on insert, not a free-space map?** Keeps the
  storage layer simple and easy to reason about; a production engine would
  track free space per page in the catalog to avoid the linear scan. Noted
  as the clearest next optimization.
- **Why is the B+Tree in-memory rather than disk-backed?** It keeps the
  on-disk format to one simple heap file per table and avoids a second,
  more complex page format for index nodes, at the cost of an O(n) index
  rebuild on startup (via a full table scan). For the workloads this engine
  targets (interactive/demo scale), the rebuild is effectively instant; the
  README calls this out as the natural next step (a disk-backed B+Tree
  using the same Page/BufferPoolManager infrastructure).
- **WHERE clause evaluation**: conditions are combined strictly left-to-right
  with `AND`/`OR` (no parentheses/operator precedence) — a deliberate scope
  cut for a small SQL dialect, documented in `ast.h`.

## Project layout

```
db_engine/
├── include/         # headers (common.h, lexer.h, ast.h, parser.h,
│                       storage.h, bptree.h, table.h, database.h, executor.h)
├── src/              # implementations + main.cpp (CLI)
├── tests/
│   └── test_queries.sql
├── Makefile
└── README.md
```
