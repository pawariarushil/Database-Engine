#include "database.h"
#include "executor.h"
#include "lexer.h"
#include "parser.h"
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>

static void printResult(const QueryResult& r) {
    if (r.isSelect) {
        std::vector<size_t> widths(r.columnNames.size());
        for (size_t i = 0; i < r.columnNames.size(); ++i) widths[i] = r.columnNames[i].size();
        std::vector<std::vector<std::string>> cells;
        for (auto& row : r.rows) {
            std::vector<std::string> cellRow;
            for (size_t i = 0; i < row.size(); ++i) {
                std::string s = valueToString(row[i]);
                widths[i] = std::max(widths[i], s.size());
                cellRow.push_back(s);
            }
            cells.push_back(std::move(cellRow));
        }

        auto printLine = [&]() {
            std::cout << "+";
            for (auto w : widths) std::cout << std::string(w + 2, '-') << "+";
            std::cout << "\n";
        };

        printLine();
        std::cout << "|";
        for (size_t i = 0; i < r.columnNames.size(); ++i)
            std::cout << " " << std::left << std::setw(static_cast<int>(widths[i])) << r.columnNames[i] << " |";
        std::cout << "\n";
        printLine();
        for (auto& cellRow : cells) {
            std::cout << "|";
            for (size_t i = 0; i < cellRow.size(); ++i)
                std::cout << " " << std::left << std::setw(static_cast<int>(widths[i])) << cellRow[i] << " |";
            std::cout << "\n";
        }
        printLine();
        std::cout << r.message;
        if (r.usedIndex) std::cout << "  [primary-key index used]";
        std::cout << "\n";
    } else {
        std::cout << r.message << "\n";
    }
}

static void runStatement(Executor& executor, const std::string& sql, bool timeIt) {
    try {
        Lexer lexer(sql);
        auto tokens = lexer.tokenize();
        Parser parser(tokens);
        auto stmt = parser.parseStatement();

        auto start = std::chrono::high_resolution_clock::now();
        QueryResult result = executor.execute(stmt.get());
        auto end = std::chrono::high_resolution_clock::now();

        printResult(result);
        if (timeIt) {
            double ms = std::chrono::duration<double, std::milli>(end - start).count();
            std::cout << "(" << std::fixed << std::setprecision(3) << ms << " ms)\n";
        }
    } catch (const DbException& e) {
        std::cout << "Error: " << e.what() << "\n";
    } catch (const std::exception& e) {
        std::cout << "Error: " << e.what() << "\n";
    }
}

static void runScript(Executor& executor, const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "Could not open script file: " << path << "\n";
        return;
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();

    // Split on ';' — simple statement separator (string literals in this
    // dialect never contain semicolons, so this is safe).
    std::string stmtText;
    for (char c : content) {
        stmtText += c;
        if (c == ';') {
            std::string trimmed = stmtText;
            size_t start = trimmed.find_first_not_of(" \t\r\n");
            if (start != std::string::npos) {
                std::cout << ">> " << trimmed.substr(start) << "\n";
                runStatement(executor, trimmed, false);
                std::cout << "\n";
            }
            stmtText.clear();
        }
    }
}

int main(int argc, char** argv) {
    std::string dataDir = "./data";
    std::string scriptPath;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--data-dir" && i + 1 < argc) {
            dataDir = argv[++i];
        } else {
            scriptPath = arg;
        }
    }

    Database db(dataDir);
    Executor executor(db);

    std::cout << "Lightweight Relational Database Engine (C++)\n";
    std::cout << "Data directory: " << dataDir << "\n";

    if (!scriptPath.empty()) {
        runScript(executor, scriptPath);
        return 0;
    }

    std::cout << "Type SQL statements terminated by ';'. Type 'exit;' to quit.\n\n";
    std::string line, buffer;
    while (true) {
        std::cout << (buffer.empty() ? "sql> " : "  -> ");
        if (!std::getline(std::cin, line)) break;
        buffer += line + " ";
        if (buffer.find(';') == std::string::npos) continue;

        std::string trimmed = buffer;
        size_t start = trimmed.find_first_not_of(" \t\r\n");
        std::string check = (start == std::string::npos) ? "" : trimmed.substr(start);
        for (auto& c : check) c = std::tolower(static_cast<unsigned char>(c));
        if (check.rfind("exit", 0) == 0 || check.rfind("quit", 0) == 0) break;

        runStatement(executor, buffer, true);
        buffer.clear();
    }
    std::cout << "Goodbye.\n";
    return 0;
}
