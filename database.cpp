#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

struct Table { //(id, name, surname)
    std::vector<std::string> columns;
    std::vector<std::vector<std::string>> rows;
};

static const std::string dataDirectory = "data"; //fileri pahelu tex

static void ensureDataDirectoryExists() { //stuguma tena ka data txtapanak ete chka stextsuma
    std::filesystem::path dir(dataDirectory);
    std::error_code ec;
    if (!std::filesystem::exists(dir, ec)) {
        std::filesystem::create_directories(dir, ec);
    }
}

static std::filesystem::path tableFilePath(const std::string& tableName) { //table file i tsanaparh
    return std::filesystem::path(dataDirectory) / (tableName + ".csv");
}

static std::string escapeCsvField(const std::string& field) { //csv formati hamar escpape e anum
    bool needsQuotes = field.find_first_of(",\"\n\r") != std::string::npos;
    std::string result;
    for (char ch : field) {
        if (ch == '"') {
            result += "\"\"";
        } else {
            result.push_back(ch);
        }
    }
    if (needsQuotes) {
        result.insert(result.begin(), '"');
        result.push_back('"');
    }
    return result;
}

static std::vector<std::string> parseCsvLine(const std::string& line) { //kardum e liney ey bajanum maseri
    std::vector<std::string> fields;
    std::string field;
    bool inQuotes = false;
    for (size_t i = 0; i < line.size(); ++i) {
        char ch = line[i];
        if (inQuotes) {
            if (ch == '"') {
                if (i + 1 < line.size() && line[i + 1] == '"') {
                    field.push_back('"');
                    ++i;
                } else {
                    inQuotes = false;
                }
            } else {
                field.push_back(ch);
            }
        } else {
            if (ch == '"') {
                inQuotes = true;
            } else if (ch == ',') {
                fields.push_back(field);
                field.clear();
            } else {
                field.push_back(ch);
            }
        }
    }
    fields.push_back(field);
    return fields;
}

static bool saveTable(const std::string& tableName, const Table& table) { //save e anum table filei mej
    ensureDataDirectoryExists();
    std::ofstream out(tableFilePath(tableName));
    if (!out) {
        std::cerr << "Failed to write table file for " << tableName << '\n';
        return false;
    }

    for (size_t i = 0; i < table.columns.size(); ++i) { //gruma columnery
        out << escapeCsvField(table.columns[i]);
        if (i + 1 < table.columns.size()) {
            out << ',';
        }
    }
    out << '\n';

    for (const auto& row : table.rows) { //isk sa gruma rownery
        for (size_t i = 0; i < row.size(); ++i) {
            out << escapeCsvField(row[i]);
            if (i + 1 < row.size()) {
                out << ',';
            }
        }
        out << '\n';
    }
    return true;
}

static bool loadTable(const std::filesystem::path& filePath, Table& table) { //karduma tabley filei mijic
    std::ifstream in(filePath);
    if (!in) {
        return false;
    }

    std::string line;
    if (!std::getline(in, line)) { //arajin toxy columnsa
        return false;
    }

    table.columns = parseCsvLine(line);
    while (std::getline(in, line)) { //gruma rownery
        if (line.empty()) {
            continue;
        }
        std::vector<std::string> row = parseCsvLine(line);
        if (!row.empty()) {
            table.rows.push_back(row);
        }
    }
    return true;
}

static void loadAllTables(std::unordered_map<std::string, Table>& db) { // qashuma bolor tablenery data folderic
    ensureDataDirectoryExists();
    std::filesystem::path dir(dataDirectory);
    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
        if (!entry.is_regular_file()) {
            continue;
        }
        std::filesystem::path path = entry.path();
        if (path.extension() != ".csv") {
            continue;
        }
        std::string tableName = path.stem().string();
        Table table;
        if (loadTable(path, table)) {
            db[tableName] = std::move(table);
        }
    }
}

struct Condition { //sa where paymanna
    bool valid = false; //stuguma ka te che paymany
    std::string column;
    std::string value;
};

static void trim(std::string& text) { //jnjuma spacery skzbic ev verjic
    auto isSpace = [](char ch) { return std::isspace(static_cast<unsigned char>(ch)) != 0; };
    while (!text.empty() && isSpace(text.front())) {
        text.erase(text.begin());
    }
    while (!text.empty() && isSpace(text.back())) {
        text.pop_back();
    }
}

static std::string toUpperCopy(const std::string& text) { //dardznuma metsatar
    std::string result = text;
    std::transform(result.begin(), result.end(), result.begin(), [](unsigned char ch) {
        return static_cast<char>(std::toupper(ch));
    });
    return result;
}

static bool startsWithIgnoreCase(const std::string& text, const std::string& prefix) {
    if (prefix.size() > text.size()) {
        return false;
    }
    for (size_t i = 0; i < prefix.size(); ++i) {
        if (std::toupper(static_cast<unsigned char>(text[i])) != std::toupper(static_cast<unsigned char>(prefix[i]))) {
            return false;
        }
    }
    return true;
}

static void skipSpaces(const std::string& input, size_t& pos) { //bac e toxum spacery
    while (pos < input.size() && std::isspace(static_cast<unsigned char>(input[pos]))) {
        ++pos;
    }
}

static std::string parseIdentifier(const std::string& input, size_t& pos) {  //karduma identifiery (table name, column name)
    skipSpaces(input, pos);
    std::string result;
    while (pos < input.size()) {
        char ch = input[pos];
        if (std::isalnum(static_cast<unsigned char>(ch)) || ch == '_') {
            result.push_back(ch);
            ++pos;
            continue;
        }
        break;
    }
    return result;
}

static std::string parseValue(const std::string& input, size_t& pos) { //karduma valuen (string kam tiv)
    skipSpaces(input, pos);
    std::string value;
    if (pos >= input.size()) {
        return value;
    }

    if (input[pos] == '"' || input[pos] == '\'') { //ete stringa
        char quote = input[pos++];
        while (pos < input.size() && input[pos] != quote) {
            if (input[pos] == '\\' && pos + 1 < input.size()) {
                ++pos;
                value.push_back(input[pos]);
            } else {
                value.push_back(input[pos]);
            }
            ++pos;
        }
        if (pos < input.size() && input[pos] == quote) {
            ++pos;
        }
    } else {
        while (pos < input.size()) {
            char ch = input[pos];
            if (ch == ',' || ch == ')' || ch == ';' || std::isspace(static_cast<unsigned char>(ch))) {
                break;
            }
            value.push_back(ch);
            ++pos;
        }
    }

    trim(value);
    return value;
}

static std::vector<std::string> parseList(const std::string& input, size_t& pos) {
    std::vector<std::string> result;
    skipSpaces(input, pos);
    if (pos >= input.size() || input[pos] != '(') {
        return result;
    }
    ++pos;

    while (true) {
        skipSpaces(input, pos);
        if (pos >= input.size()) {
            break;
        }
        if (input[pos] == ')') {
            ++pos;
            break;
        }

        std::string item = parseValue(input, pos);
        if (!item.empty()) {
            result.push_back(item);
        }

        skipSpaces(input, pos);
        if (pos < input.size() && input[pos] == ',') {
            ++pos;
            continue;
        }
    }

    return result;
}

static Condition parseCondition(const std::string& input, size_t& pos) {
    skipSpaces(input, pos);
    Condition condition;
    if (startsWithIgnoreCase(input.substr(pos), "WHERE")) {
        pos += 5;
        condition.column = parseIdentifier(input, pos);
        skipSpaces(input, pos);
        if (pos < input.size() && input[pos] == '=') {
            ++pos;
        }
        condition.value = parseValue(input, pos);
        if (!condition.column.empty() && !condition.value.empty()) {
            condition.valid = true;
        }
    }
    return condition;
}

static void printTable(const Table& table) {
    if (table.columns.empty()) {
        std::cout << "Empty table structure." << std::endl;
        return;
    }

    for (size_t i = 0; i < table.columns.size(); ++i) {
        std::cout << table.columns[i];
        if (i + 1 < table.columns.size()) {
            std::cout << " | ";
        }
    }
    std::cout << std::endl;

    for (const auto& row : table.rows) {
        for (size_t i = 0; i < row.size(); ++i) {
            std::cout << row[i];
            if (i + 1 < row.size()) {
                std::cout << " | ";
            }
        }
        std::cout << std::endl;
    }
}

static bool hasNonSpace(const std::string& input, size_t pos) {
    while (pos < input.size()) {
        if (!std::isspace(static_cast<unsigned char>(input[pos]))) {
            return true;
        }
        ++pos;
    }
    return false;
}

static std::string trimAndRemoveSemicolon(std::string text) {
    trim(text);
    if (!text.empty() && text.back() == ';') {
        text.pop_back();
    }
    trim(text);
    return text;
}

int main() {
    std::unordered_map<std::string, Table> db;
    std::string command;

    loadAllTables(db);
    std::cout << "Simple SQL-like database. Type EXIT or QUIT to stop." << std::endl;
    if (!db.empty()) {
        std::cout << "Loaded " << db.size() << " table(s) from '" << dataDirectory << "'." << std::endl;
    }

    while (true) {
        std::cout << "db> ";
        if (!std::getline(std::cin, command)) {
            break;
        }
        command = trimAndRemoveSemicolon(command);
        if (command.empty()) {
            continue;
        }

        std::string upper = toUpperCopy(command);
        if (upper == "EXIT" || upper == "QUIT") {
            break;
        }

        if (startsWithIgnoreCase(command, "CREATE TABLE")) {
            size_t pos = 12;
            std::string tableName = parseIdentifier(command, pos);
            if (tableName.empty()) {
                std::cout << "Syntax error: missing table name." << std::endl;
                continue;
            }
            if (db.find(tableName) != db.end()) {
                std::cout << "Table '" << tableName << "' already exists." << std::endl;
                continue;
            }
            std::vector<std::string> columns = parseList(command, pos);
            if (columns.empty()) {
                std::cout << "Syntax error: missing column list." << std::endl;
                continue;
            }
            db[tableName] = Table{columns, {}};
            if (saveTable(tableName, db[tableName])) {
                std::cout << "Created table '" << tableName << "'." << std::endl;
            } else {
                std::cout << "Created table '" << tableName << "' in memory, but failed to save file." << std::endl;
            }
            continue;
        }

        if (startsWithIgnoreCase(command, "INSERT INTO")) {
            size_t pos = 11;
            std::string tableName = parseIdentifier(command, pos);
            if (tableName.empty()) {
                std::cout << "Syntax error: missing table name." << std::endl;
                continue;
            }
            if (db.find(tableName) == db.end()) {
                std::cout << "Table '" << tableName << "' does not exist." << std::endl;
                continue;
            }
            std::string upperRest = toUpperCopy(command.substr(pos));
            size_t valuesPos = upperRest.find("VALUES");
            if (valuesPos == std::string::npos) {
                std::cout << "Syntax error: missing VALUES keyword." << std::endl;
                continue;
            }
            pos += valuesPos + 6;
            std::vector<std::string> values = parseList(command, pos);
            Table& table = db[tableName];
            if (values.size() != table.columns.size()) {
                std::cout << "Column count mismatch: expected " << table.columns.size() << " values." << std::endl;
                continue;
            }
            table.rows.push_back(values);
            if (saveTable(tableName, table)) {
                std::cout << "Inserted 1 row into '" << tableName << "'." << std::endl;
            } else {
                std::cout << "Inserted 1 row into '" << tableName << "' but failed to save file." << std::endl;
            }
            continue;
        }

        if (startsWithIgnoreCase(command, "SELECT * FROM")) {
            size_t pos = 13;
            std::string tableName = parseIdentifier(command, pos);
            if (tableName.empty()) {
                std::cout << "Syntax error: missing table name." << std::endl;
                continue;
            }
            if (db.find(tableName) == db.end()) {
                std::cout << "Table '" << tableName << "' does not exist." << std::endl;
                continue;
            }
            skipSpaces(command, pos);
            if (hasNonSpace(command, pos) && !startsWithIgnoreCase(command.substr(pos), "WHERE")) {
                std::cout << "Syntax error: expected WHERE clause after table name." << std::endl;
                continue;
            }
            Condition condition = parseCondition(command, pos);
            Table& table = db[tableName];
            if (!condition.valid) {
                printTable(table);
                continue;
            }
            auto colIt = std::find(table.columns.begin(), table.columns.end(), condition.column);
            if (colIt == table.columns.end()) {
                std::cout << "Column '" << condition.column << "' does not exist." << std::endl;
                continue;
            }
            size_t columnIndex = std::distance(table.columns.begin(), colIt);
            Table filtered{table.columns, {}};
            for (const auto& row : table.rows) {
                if (columnIndex < row.size() && row[columnIndex] == condition.value) {
                    filtered.rows.push_back(row);
                }
            }
            printTable(filtered);
            continue;
        }

        if (startsWithIgnoreCase(command, "DELETE FROM") || startsWithIgnoreCase(command, "DELETE * FROM")) {
            size_t pos = startsWithIgnoreCase(command, "DELETE * FROM") ? 13 : 12;
            std::string tableName = parseIdentifier(command, pos);
            if (tableName.empty()) {
                std::cout << "Syntax error: missing table name." << std::endl;
                continue;
            }
            if (db.find(tableName) == db.end()) {
                std::cout << "Table '" << tableName << "' does not exist." << std::endl;
                continue;
            }
            skipSpaces(command, pos);
            if (hasNonSpace(command, pos) && !startsWithIgnoreCase(command.substr(pos), "WHERE")) {
                std::cout << "Syntax error: expected WHERE clause after table name." << std::endl;
                continue;
            }
            Table& table = db[tableName];
            Condition condition = parseCondition(command, pos);
            if (!condition.valid) {
                size_t count = table.rows.size();
                table.rows.clear();
                if (saveTable(tableName, table)) {
                    std::cout << "Deleted " << count << " row(s) from '" << tableName << "'." << std::endl;
                } else {
                    std::cout << "Deleted " << count << " row(s) from '" << tableName << "' but failed to save file." << std::endl;
                }
                continue;
            }
            auto colIt = std::find(table.columns.begin(), table.columns.end(), condition.column);
            if (colIt == table.columns.end()) {
                std::cout << "Column '" << condition.column << "' does not exist." << std::endl;
                continue;
            }
            size_t columnIndex = std::distance(table.columns.begin(), colIt);
            size_t originalSize = table.rows.size();
            table.rows.erase(std::remove_if(table.rows.begin(), table.rows.end(), [&](const std::vector<std::string>& row) {
                return columnIndex < row.size() && row[columnIndex] == condition.value;
            }), table.rows.end());
            if (saveTable(tableName, table)) {
                std::cout << "Deleted " << (originalSize - table.rows.size()) << " row(s) from '" << tableName << "'." << std::endl;
            } else {
                std::cout << "Deleted " << (originalSize - table.rows.size()) << " row(s) from '" << tableName << "' but failed to save file." << std::endl;
            }
            continue;
        }

        if (startsWithIgnoreCase(command, "UPDATE ")) {
            size_t pos = 6;
            std::string tableName = parseIdentifier(command, pos);
            if (tableName.empty()) {
                std::cout << "Syntax error: missing table name." << std::endl;
                continue;
            }
            if (db.find(tableName) == db.end()) {
                std::cout << "Table '" << tableName << "' does not exist." << std::endl;
                continue;
            }
            std::string upperRest = toUpperCopy(command.substr(pos));
            size_t setPos = upperRest.find("SET");
            if (setPos == std::string::npos) {
                std::cout << "Syntax error: missing SET clause." << std::endl;
                continue;
            }
            pos += setPos + 3;
            std::string setColumn = parseIdentifier(command, pos);
            skipSpaces(command, pos);
            if (pos >= command.size() || command[pos] != '=') {
                std::cout << "Syntax error: expected '=' in SET clause." << std::endl;
                continue;
            }
            ++pos;
            std::string setValue = parseValue(command, pos);
            Condition condition = parseCondition(command, pos);
            Table& table = db[tableName];
            auto setIt = std::find(table.columns.begin(), table.columns.end(), setColumn);
            if (setIt == table.columns.end()) {
                std::cout << "Column '" << setColumn << "' does not exist." << std::endl;
                continue;
            }
            size_t setIndex = std::distance(table.columns.begin(), setIt);

            if (!condition.valid) {
                for (auto& row : table.rows) {
                    if (setIndex < row.size()) {
                        row[setIndex] = setValue;
                    }
                }
                if (saveTable(tableName, table)) {
                    std::cout << "Updated " << table.rows.size() << " row(s) in '" << tableName << "'." << std::endl;
                } else {
                    std::cout << "Updated " << table.rows.size() << " row(s) in '" << tableName << "' but failed to save file." << std::endl;
                }
                continue;
            }

            auto condIt = std::find(table.columns.begin(), table.columns.end(), condition.column);
            if (condIt == table.columns.end()) {
                std::cout << "Column '" << condition.column << "' does not exist." << std::endl;
                continue;
            }
            size_t condIndex = std::distance(table.columns.begin(), condIt);
            size_t updated = 0;
            for (auto& row : table.rows) {
                if (condIndex < row.size() && row[condIndex] == condition.value) {
                    if (setIndex < row.size()) {
                        row[setIndex] = setValue;
                        ++updated;
                    }
                }
            }
            if (saveTable(tableName, table)) {
                std::cout << "Updated " << updated << " row(s) in '" << tableName << "'." << std::endl;
            } else {
                std::cout << "Updated " << updated << " row(s) in '" << tableName << "' but failed to save file." << std::endl;
            }
            continue;
        }

        std::cout << "Unrecognized command." << std::endl;
    }

    std::cout << "Goodbye." << std::endl;
    return 0;
}


//CREATE TABLE USERS (ID, NAME, surname);
//INSERT INTO USERS VALUES (7, cristiano,ronaldo);
//INSERT INTO USERS VALUES (9, cristiano,ronaldo);
//INSERT INTO USERS VALUES (7, lionel,messi);
//SELECT * FROM USERS;
//SELECT * FROM USERS WHERE NAME = cristiano;
//select * from where id = 7;
//update users set name = messi where id = 7;