<div align="center">

# LightDB

### A lightweight relational database engine built from scratch in C++17

**Explore what happens behind a SQL query — from tokenization and parsing to execution, indexing, and disk-backed storage.**

<p>
  <img src="https://img.shields.io/badge/Language-C%2B%2B17-00599C?style=for-the-badge&logo=cplusplus&logoColor=white" alt="C++17">
  <img src="https://img.shields.io/badge/Database-Custom%20SQL%20Engine-4479A1?style=for-the-badge" alt="Custom SQL engine">
  <img src="https://img.shields.io/badge/Index-B%2B%20Tree-6C5CE7?style=for-the-badge" alt="B+ Tree">
  <img src="https://img.shields.io/badge/Build-GNU%20Make-2D3436?style=for-the-badge&logo=gnubash&logoColor=white" alt="GNU Make">
</p>

<p>
  <a href="#-overview">Overview</a> •
  <a href="#-features">Features</a> •
  <a href="#-architecture">Architecture</a> •
  <a href="#-getting-started">Getting Started</a> •
  <a href="#-sql-support">SQL Support</a>
</p>

</div>

---

## 🎯 Overview

**LightDB** is a small relational database engine implemented in C++17. It processes a supported subset of SQL and stores table data in local files, bringing together several core database concepts in one project.

Instead of relying on an existing database engine, LightDB implements its own SQL lexer, recursive-descent parser, query execution layer, page-based storage, buffer pool, and in-memory B+ Tree primary-key index.

> **Project scope:** LightDB is an educational systems-programming project. It is not intended to be a production database or a replacement for SQLite or PostgreSQL.

## ✨ Features

<table>
  <tr>
    <td width="50%">
      <h3>🔤 Custom SQL Processing</h3>
      A hand-written lexer and recursive-descent parser turn SQL statements into structures that the execution layer can process.
    </td>
    <td width="50%">
      <h3>🧾 CRUD Operations</h3>
      Create tables, insert rows, query records, update values, delete rows, and filter results using supported SQL statements.
    </td>
  </tr>
  <tr>
    <td>
      <h3>🌳 B+ Tree Index</h3>
      An in-memory primary-key index supports efficient equality lookups for supported query patterns.
    </td>
    <td>
      <h3>💾 Page-Based Storage</h3>
      Table data is stored in fixed-size 4096-byte pages using a slotted-page layout.
    </td>
  </tr>
  <tr>
    <td>
      <h3>⚡ LRU Buffer Pool</h3>
      Caches pages in memory, tracks pin counts, and writes dirty pages back to disk when evicted.
    </td>
    <td>
      <h3>🔁 Persistence</h3>
      Table schemas and rows are stored locally; in-memory primary-key indexes are rebuilt when the database is reopened.
    </td>
  </tr>
</table>

## 🏗️ Architecture

A query passes through the SQL-processing pipeline and reaches the execution and storage layers.

```text
                 ┌───────────────────────┐
                 │       SQL Input       │
                 └───────────┬───────────┘
                             ▼
                 ┌───────────────────────┐
                 │         Lexer         │
                 │  SQL text → tokens    │
                 └───────────┬───────────┘
                             ▼
                 ┌───────────────────────┐
                 │        Parser         │
                 │  Tokens → AST         │
                 └───────────┬───────────┘
                             ▼
                 ┌───────────────────────┐
                 │       Executor        │
                 │  Execute SQL queries  │
                 └───────────┬───────────┘
                             ▼
                 ┌───────────────────────┐
                 │        Table          │
                 │ Rows + primary index  │
                 └────────┬──────┬───────┘
                          │      │
                          ▼      ▼
                ┌────────────┐ ┌────────────┐
                │Buffer Pool │ │   B+ Tree  │
                │ LRU cache  │ │ PK lookup  │
                └─────┬──────┘ └────────────┘
                      ▼
                ┌────────────┐
                │Disk Manager│
                │ Page I/O   │
                └─────┬──────┘
                      ▼
                ┌────────────┐
                │Table files │
                └────────────┘
```

### Core components

| Component | Responsibility |
|---|---|
| **Lexer** | Converts SQL text into tokens. |
| **Parser** | Parses supported statements using recursive descent and builds an AST. |
| **Executor** | Executes statements and evaluates supported filters. |
| **Table layer** | Coordinates row operations, storage, and primary-key indexing. |
| **B+ Tree** | Maps primary-key values to row identifiers in memory. |
| **Buffer pool** | Caches pages, tracks pins, and writes dirty pages during eviction. |
| **Disk manager** | Reads and writes pages in local binary table files. |

## 🧰 Tech Stack

| Layer | Technology |
|---|---|
| Language | C++17 |
| Build system | GNU Make |
| Compiler | `g++` or a compatible C++17 compiler |
| Query processing | Custom lexer, recursive-descent parser, AST, query executor |
| Indexing | In-memory B+ Tree |
| Storage | 4096-byte pages, slotted-page layout, buffer pool, disk-backed files |

## 🚀 Getting Started

### Prerequisites

Install a C++17-compatible compiler and GNU Make.

- **macOS:** install Xcode Command Line Tools if needed.
- **Ubuntu/Debian:** install `g++` and `make`.
- **Windows:** use a compatible environment such as WSL with a C++17 compiler and GNU Make.

### 1. Clone the repository

```bash
git clone https://github.com/pawariarushil/Database-Engine.git
cd Database-Engine
```

### 2. Build the project

```bash
make
```

The Makefile should compile the project and produce the database executable configured by the repository.

### 3. Launch LightDB

```bash
./dbengine
```

This starts the interactive SQL shell if the executable is named `dbengine` by the current Makefile.

### 4. Run the SQL example script

```bash
./dbengine tests/test_queries.sql
```

Use the executable name and script path that exist in your checked-out version of the repository. If the Makefile defines a different target or output name, follow that configuration.

## 💻 Example

The following illustrates the style of SQL statements supported by the engine:

```sql
CREATE TABLE students (
    id INT PRIMARY KEY,
    name TEXT,
    gpa FLOAT
);

INSERT INTO students VALUES (1, 'Alice', 3.9);
INSERT INTO students VALUES (2, 'Bob', 3.2);
INSERT INTO students VALUES (3, 'Charlie', 3.7);

SELECT * FROM students;

SELECT name, gpa
FROM students
WHERE gpa >= 3.5;

UPDATE students
SET gpa = 4.0
WHERE id = 1;

DELETE FROM students
WHERE id = 2;
```

**Note:** This is a custom SQL dialect. Exact accepted syntax and supported operators depend on the current parser and executor implementation.

## 📋 SQL Support

| Statement / capability | Description |
|---|---|
| `CREATE TABLE` | Create a table with a declared schema. |
| `INSERT` | Insert rows into a table. |
| `SELECT` | Read rows and project selected columns. |
| `UPDATE` | Modify matching rows. |
| `DELETE` | Delete matching rows. |
| `WHERE` | Filter records using supported comparison conditions. |
| Primary-key lookup | Use the in-memory B+ Tree for supported primary-key equality lookups. |

The engine implements a subset of SQL rather than the complete SQL standard. Features such as joins, transactions, `NULL` semantics, and other advanced SQL behavior should not be assumed unless implemented and tested in the source.

## 🔬 Storage and Indexing

<details>
  <summary><strong>Slotted-page storage</strong></summary>

Table records are stored in fixed-size **4096-byte pages**. A slotted-page layout keeps record references separate from record contents, allowing the engine to track variable-length rows within a page.

</details>

<details>
  <summary><strong>Buffer pool and LRU eviction</strong></summary>

The buffer pool caches pages in memory and uses LRU-style replacement to choose an unpinned page for eviction. Pin counts help track whether a page is currently in use, and dirty pages are written back when evicted.

</details>

<details>
  <summary><strong>B+ Tree primary-key index</strong></summary>

The in-memory B+ Tree maps primary-key values to row identifiers to speed up supported equality lookups. The index is rebuilt from persisted table data when the database is reopened; it is not a separately persisted index structure.

**Current limitation:** deletion does not rebalance the tree through redistribution or merging.

</details>

<details>
  <summary><strong>Persistence</strong></summary>

Table data is stored in local binary files, and table schemas are persisted in a catalog. At startup, table definitions are loaded and the in-memory primary-key indexes are reconstructed from stored rows.

</details>

## 🧪 Testing

If the repository includes a SQL test script, run it after building:

```bash
make
./dbengine tests/test_queries.sql
```

Inspect the output for the expected results and errors. Check the current repository for the exact test files and executable name before relying on these commands.

Useful cases to validate as the project evolves:

- Table creation and schema persistence
- Inserts and duplicate primary-key handling
- Selection and `WHERE` filtering
- Updates and deletes
- Reopening the database and reading previously stored rows
- Primary-key equality lookups
- Page eviction and dirty-page write-back

## 🧭 Current Limitations

LightDB focuses on the fundamentals of database implementation, so it has important limitations:

- The SQL dialect is intentionally limited.
- The B+ Tree index is in memory and reconstructed at startup.
- B+ Tree deletion does not currently rebalance underfilled nodes.
- `AND` and `OR` conditions may be evaluated left-to-right rather than with standard SQL precedence.
- Production features such as transactions, concurrency control, crash recovery, and broad SQL compatibility are outside the current scope.

These limitations are useful starting points for future improvements, not claims of production readiness.

## 🛣️ Possible Improvements

- Implement B+ Tree node redistribution and merging after deletion.
- Persist index pages to reduce index reconstruction work at startup.
- Add a free-space map to reduce the cost of searching for pages with room for new records.
- Implement conventional SQL boolean precedence and parenthesized expressions.
- Expand automated tests for parser, execution, persistence, and storage edge cases.
- Explore transactions and concurrency control.

## 🎓 What This Project Demonstrates

LightDB brings together several foundational systems concepts:

- Translating SQL into tokens, syntax trees, and executable operations
- Designing a custom query-processing pipeline
- Storing variable-length records in fixed-size pages
- Managing cached pages using pin counts and LRU eviction
- Building and using a B+ Tree primary-key index
- Persisting data and reconstructing in-memory structures after restart
- Documenting trade-offs and limitations in a systems project

---

<div align="center">

### LightDB

**A closer look at the internals of a relational database.**

Built in C++17.

</div>
