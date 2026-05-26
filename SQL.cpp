#include <iostream>
#include <vector>
#include <unordered_map>
#include <string>
#include <fstream>
#include <sstream>
#include <thread>
#include <future>
#include <queue>
#include <functional>
#include <condition_variable>
#include <mutex>

static std::string clean(std::string s)
{
    if (!s.empty() && s.back() == ';')
        s.pop_back();

    if (!s.empty() && s.front() == '"')
        s.erase(0, 1);
    if (!s.empty() && s.back() == '"')
        s.pop_back();

    return s;
}

static std::string trim(const std::string &s)
{
    size_t start = s.find_first_not_of(" \t\r\n");
    size_t end = s.find_last_not_of(" \t\r\n");
    return (start == std::string::npos) ? std::string() : s.substr(start, end - start + 1);
}

#ifndef SQL_SERVER
#define SQL_SERVER 0
#endif

#if SQL_SERVER
#include "network_compat.h"
#endif

class ThreadPool
{
public:
    explicit ThreadPool(size_t threads) : stop(false)
    {
        for (size_t i = 0; i < threads; ++i)
        {
            workers.emplace_back([this]
                                 {
                for (;;) {
                    std::function<void()> task;
                    {
                        std::unique_lock<std::mutex> lock(this->queue_mutex);
                        this->condition.wait(lock, [this]{ return this->stop || !this->tasks.empty(); });
                        if (this->stop && this->tasks.empty())
                            return;
                        task = std::move(this->tasks.front());
                        this->tasks.pop();
                    }
                    task();
                } });
        }
    }

    ~ThreadPool()
    {
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            stop = true;
        }
        condition.notify_all();
        for (std::thread &worker : workers)
            if (worker.joinable())
                worker.join();
    }

    template <class F, class... Args>
    auto enqueue(F &&f, Args &&...args)
        -> std::future<typename std::result_of<F(Args...)>::type>
    {
        using return_type = typename std::result_of<F(Args...)>::type;

        auto task = std::make_shared<std::packaged_task<return_type()>>(std::bind(std::forward<F>(f), std::forward<Args>(args)...));

        std::future<return_type> res = task->get_future();
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            if (stop)
                throw std::runtime_error("enqueue on stopped ThreadPool");
            tasks.emplace([task]()
                          { (*task)(); });
        }
        condition.notify_one();
        return res;
    }

private:
    std::vector<std::thread> workers;
    std::queue<std::function<void()>> tasks;
    std::mutex queue_mutex;
    std::condition_variable condition;
    bool stop;
};

class Table
{
private:
    std::vector<std::vector<std::string>> rows;

public:
    Table() = default;
    Table(std::vector<std::string> arr)
    {
        rows.push_back(arr);
    }

    void insert(const std::vector<std::string> &arr)
    {
        if (rows.empty() || arr.size() == rows[0].size())
        {
            rows.push_back(arr);
            std::cout << "row inserted" << std::endl;
        }
        else
        {
            std::cout << "row not inserted: wrong number of columns" << std::endl;
        }
    }

    Table select() const
    {
        if (rows.empty())
        {
            std::cout << "Empty table" << std::endl;
            return *this;
        }

        int columns = static_cast<int>(rows[0].size());
        int total_width = 18 * columns + 1;

        for (int i = 0; i < total_width; ++i)
            std::cout << '_';
        std::cout << std::endl;

        for (int i = 0; i < rows.size(); ++i)
        {
            std::cout << "| ";
            for (int j = 0; j < rows[i].size(); ++j)
            {
                std::cout << rows[i][j];
                for (int k = 0; k < 15 - static_cast<int>(rows[i][j].length()); ++k)
                    std::cout << ' ';
                std::cout << " | ";
            }
            std::cout << std::endl;
            if (i == 0)
            {
                std::cout << "|";
                for (int k = 0; k < 18 * columns - 1; ++k)
                    std::cout << '-';
                std::cout << "|" << std::endl;
            }
        }

        return *this;
    }

    Table select(const std::string &key, const std::string &value) const
    {
        Table result;
        if (rows.empty())
        {
            std::cout << "Table is empty" << std::endl;
            return result;
        }

        int index = -1;
        for (int i = 0; i < rows[0].size(); ++i)
        {
            if (rows[0][i] == key)
            {
                index = i;
                break;
            }
        }

        if (index == -1)
        {
            std::cout << "Key not found" << std::endl;
            return result;
        }

        result.insert(rows[0]);
        bool found = false;

        for (int i = 1; i < rows.size(); ++i)
        {
            if (index < rows[i].size() && rows[i][index] == value)
            {
                result.insert(rows[i]);
                found = true;
            }
        }

        if (found)
        {
            result.select();
        }
        else
        {
            std::cout << "No rows found for " << key << " = " << value << std::endl;
        }

        return result;
    }

    void delete_from()
    {
        rows.clear();
        std::cout << "All rows deleted" << std::endl;
    }

    void delete_from(const std::string &key, const std::string &value)
    {
        if (rows.empty())
        {
            std::cout << "Table is empty" << std::endl;
            return;
        }

        int index = -1;
        for (int i = 0; i < rows[0].size(); ++i)
        {
            if (rows[0][i] == key)
            {
                index = i;
                break;
            }
        }

        if (index == -1)
        {
            std::cout << "Key not found" << std::endl;
            return;
        }

        for (int i = 1; i < rows.size();)
        {
            if (index < rows[i].size() && rows[i][index] == value)
            {
                rows.erase(rows.begin() + i);
            }
            else
            {
                ++i;
            }
        }
    }

    void update_set(const std::string &key1, const std::string &current_value, const std::string &new_value)
    {
        if (rows.empty())
        {
            std::cout << "Table is empty" << std::endl;
            return;
        }

        int index = -1;
        for (int i = 0; i < rows[0].size(); ++i)
        {
            if (rows[0][i] == key1)
            {
                index = i;
                break;
            }
        }

        if (index == -1)
        {
            std::cout << "Key " << key1 << " not found" << std::endl;
            return;
        }

        for (int i = 1; i < rows.size(); ++i)
        {
            if (index < rows[i].size() && rows[i][index] == current_value)
            {
                rows[i][index] = new_value;
            }
        }
    }

    void update_set(const std::string &key1,
                    const std::string &current_value,
                    const std::string &key2,
                    const std::string &new_value)
    {
        if (rows.empty())
        {
            std::cout << "Table is empty" << std::endl;
            return;
        }

        std::string clean_current = clean(current_value);
        std::string clean_new = clean(new_value);

        int index1 = -1;
        int index2 = -1;
        for (int i = 0; i < rows[0].size(); ++i)
        {
            if (rows[0][i] == key1)
                index1 = i;
            if (rows[0][i] == key2)
                index2 = i;
        }

        if (index1 == -1 || index2 == -1)
        {
            std::cout << "Key not found" << std::endl;
            return;
        }

        for (int i = 1; i < rows.size(); ++i)
        {
            if (index1 < rows[i].size() && rows[i][index1] == clean_current)
            {
                if (index2 < rows[i].size())
                    rows[i][index2] = clean_new;
            }
        }
    }

    void save(const std::string &name) const
    {
        std::ofstream file(name + ".txt");
        for (const auto &row : rows)
        {
            for (int i = 0; i < row.size(); ++i)
            {
                file << row[i];
                if (i != row.size() - 1)
                    file << ",";
            }
            file << '\n';
        }
    }

    void load(const std::string &name)
    {
        rows.clear();
        std::ifstream file(name + ".txt");
        if (!file.is_open())
            return;

        std::string line;
        while (std::getline(file, line))
        {
            if (line.empty())
                continue;
            std::stringstream ss(line);
            std::vector<std::string> row;
            std::string value;
            while (std::getline(ss, value, ','))
            {
                row.push_back(value);
            }
            if (!row.empty())
                rows.push_back(row);
        }
    }
};

class SQL
{
private:
    std::unordered_map<std::string, Table> tables;

public:
    SQL() = default;

    int create_table(const std::vector<std::string> &arr, const std::string &name)
    {
        if (tables.find(name) != tables.end())
        {
            std::cout << "table with this name exist" << std::endl;
            return 1;
        }

        tables.insert({name, Table(arr)});
        std::cout << "table " << name << " created" << std::endl;

        std::ifstream check("tables.txt");
        bool exists = false;
        std::string temp;
        while (std::getline(check, temp))
        {
            if (trim(temp) == name)
            {
                exists = true;
                break;
            }
        }

        if (!exists)
        {
            std::ofstream meta("tables.txt", std::ios::app);
            meta << name << '\n';
        }

        tables[name].save(name);
        return 0;
    }

    bool drop_table(const std::string &name)
    {
        auto it = tables.find(name);
        if (it != tables.end())
        {
            std::cout << name << " table deleted" << std::endl;
            tables.erase(it);
            return true;
        }
        std::cout << name << " table not found" << std::endl;
        return false;
    }

    Table *operator()(const std::string &name)
    {
        auto it = tables.find(name);
        if (it == tables.end())
            return nullptr;
        return &it->second;
    }

    void load_tables()
    {
        std::ifstream file("tables.txt");
        if (!file.is_open())
            return;

        std::string table_name;
        while (std::getline(file, table_name))
        {
            table_name = trim(table_name);
            if (table_name.empty())
                continue;
            Table t;
            t.load(table_name);
            tables[table_name] = t;
        }
    }
};

static void SQL_compiler(SQL &db, std::string str)
{
    if (!str.empty() && str.back() != ';')
    {
        str.push_back(';');
    }

    while (str.find(';') != std::string::npos)
    {
        size_t pos = str.find(';');
        std::string line = trim(str.substr(0, pos));
        if (line.empty())
        {
            str.erase(0, pos + 1);
            continue;
        }

        size_t space_index = line.find(' ');
        std::string opp = (space_index == std::string::npos) ? line : line.substr(0, space_index);

        if (opp == "CREATE")
        {
            line = trim(line.substr(space_index + 1));
            space_index = line.find(' ');
            opp = (space_index == std::string::npos) ? line : line.substr(0, space_index);

            if (opp == "TABLE")
            {
                line = trim(line.substr(space_index + 1));
                size_t left = line.find('(');
                std::string db_name = (left == std::string::npos) ? trim(line) : trim(line.substr(0, left));
                std::string columns = (left == std::string::npos) ? std::string() : line.substr(left);

                size_t right = columns.find(')');
                if (left == std::string::npos || right == std::string::npos || right <= 0)
                {
                    std::cout << "Unknown command" << std::endl;
                }
                else
                {
                    std::string keys = columns.substr(1, right - 1);
                    std::vector<std::string> key_vec;
                    std::stringstream ss(keys);
                    std::string key_value;
                    while (std::getline(ss, key_value, ','))
                    {
                        key_vec.push_back(trim(key_value));
                    }
                    db.create_table(key_vec, db_name);
                }
            }
        }
        else if (opp == "INSERT")
        {
            line = trim(line.substr(space_index + 1));
            space_index = line.find(' ');
            opp = (space_index == std::string::npos) ? line : line.substr(0, space_index);

            if (opp == "INTO")
            {
                line = trim(line.substr(space_index + 1));
                size_t values_pos = line.find("VALUES");
                std::string db_name;
                std::string values_section;

                if (values_pos != std::string::npos)
                {
                    db_name = trim(line.substr(0, values_pos));
                    values_section = trim(line.substr(values_pos + 6));
                }

                if (!db_name.empty())
                {
                    size_t left = db_name.find('(');
                    if (left != std::string::npos)
                    {
                        db_name = trim(db_name.substr(0, left));
                    }
                }

                if (!values_section.empty() && values_section.front() == '(')
                {
                    size_t right = values_section.find(')');
                    if (right != std::string::npos && right > 0)
                    {
                        std::string values = values_section.substr(1, right - 1);
                        std::vector<std::string> key_vec;
                        std::stringstream ss(values);
                        std::string key_value;
                        while (std::getline(ss, key_value, ','))
                        {
                            key_vec.push_back(clean(trim(key_value)));
                        }
                        Table *table = db(db_name);
                        if (table)
                        {
                            table->insert(key_vec);
                            table->save(db_name);
                        }
                        else
                        {
                            std::cout << "Table " << db_name << " not found" << std::endl;
                        }
                    }
                }
            }
        }
        else if (opp == "SELECT")
        {
            line = trim(line.substr(space_index + 1));
            space_index = line.find(' ');
            opp = (space_index == std::string::npos) ? line : line.substr(0, space_index);

            if (opp == "*")
            {
                line = trim(line.substr(space_index + 1));
                space_index = line.find(' ');
                opp = (space_index == std::string::npos) ? line : line.substr(0, space_index);
                if (opp == "FROM")
                {
                    line = trim(line.substr(space_index + 1));
                    space_index = line.find(' ');
                    std::string db_name = (space_index == std::string::npos) ? line : line.substr(0, space_index);
                    Table *table = db(db_name);
                    if (!table)
                    {
                        std::cout << "Table " << db_name << " not found" << std::endl;
                    }
                    else if (space_index == std::string::npos)
                    {
                        table->select();
                    }
                    else
                    {
                        line = trim(line.substr(space_index + 1));
                        space_index = line.find(' ');
                        opp = (space_index == std::string::npos) ? line : line.substr(0, space_index);
                        if (opp == "WHERE")
                        {
                            line = trim(line.substr(space_index + 1));
                            space_index = line.find(' ');
                            std::string key_value = (space_index == std::string::npos) ? line : line.substr(0, space_index);
                            line = (space_index == std::string::npos) ? std::string() : trim(line.substr(space_index + 1));
                            space_index = line.find(' ');
                            opp = (space_index == std::string::npos) ? line : line.substr(0, space_index);
                            if (opp == "=")
                            {
                                std::string name_value = trim((space_index == std::string::npos) ? std::string() : line.substr(space_index + 1));
                                table->select(key_value, clean(name_value));
                            }
                        }
                    }
                }
            }
        }
        else if (opp == "DELETE")
        {
            line = trim(line.substr(space_index + 1));
            space_index = line.find(' ');
            opp = (space_index == std::string::npos) ? line : line.substr(0, space_index);

            if (opp == "*")
            {
                line = trim(line.substr(space_index + 1));
                space_index = line.find(' ');
                opp = (space_index == std::string::npos) ? line : line.substr(0, space_index);
                if (opp == "FROM")
                {
                    line = trim(line.substr(space_index + 1));
                    space_index = line.find(' ');
                    std::string db_name = (space_index == std::string::npos) ? line : line.substr(0, space_index);
                    Table *table = db(db_name);
                    if (!table)
                    {
                        std::cout << "Table " << db_name << " not found" << std::endl;
                    }
                    else if (space_index == std::string::npos)
                    {
                        table->delete_from();
                        table->save(db_name);
                    }
                    else
                    {
                        line = trim(line.substr(space_index + 1));
                        space_index = line.find(' ');
                        opp = (space_index == std::string::npos) ? line : line.substr(0, space_index);
                        if (opp == "WHERE")
                        {
                            line = trim(line.substr(space_index + 1));
                            space_index = line.find(' ');
                            std::string key_value = (space_index == std::string::npos) ? line : line.substr(0, space_index);
                            line = (space_index == std::string::npos) ? std::string() : trim(line.substr(space_index + 1));
                            space_index = line.find(' ');
                            opp = (space_index == std::string::npos) ? line : line.substr(0, space_index);
                            if (opp == "=")
                            {
                                std::string name_value = trim((space_index == std::string::npos) ? std::string() : line.substr(space_index + 1));
                                table->delete_from(key_value, clean(name_value));
                                table->save(db_name);
                            }
                        }
                    }
                }
            }
        }
        else if (opp == "UPDATE")
        {
            line = trim(line.substr(space_index + 1));
            space_index = line.find(' ');
            std::string db_name = (space_index == std::string::npos) ? line : line.substr(0, space_index);
            std::string rest = (space_index == std::string::npos) ? std::string() : trim(line.substr(space_index + 1));
            Table *table = db(db_name);
            if (!table)
            {
                std::cout << "Table " << db_name << " not found" << std::endl;
            }
            else
            {
                space_index = rest.find(' ');
                opp = (space_index == std::string::npos) ? rest : rest.substr(0, space_index);
                if (opp == "SET")
                {
                    std::string segment = trim(rest.substr(space_index + 1));
                    size_t eq_pos = segment.find('=');
                    if (eq_pos != std::string::npos)
                    {
                        std::string key_value = trim(segment.substr(0, eq_pos));
                        std::string name_value = trim(segment.substr(eq_pos + 1));
                        size_t where_pos = name_value.find("WHERE");
                        if (where_pos != std::string::npos)
                        {
                            std::string target_value = trim(name_value.substr(0, where_pos));
                            std::string condition = trim(name_value.substr(where_pos + 5));
                            size_t eq2 = condition.find('=');
                            if (eq2 != std::string::npos)
                            {
                                std::string new_key_value = trim(condition.substr(0, eq2));
                                std::string new_name_value = trim(condition.substr(eq2 + 1));
                                table->update_set(new_key_value, clean(new_name_value), key_value, clean(target_value));
                                table->save(db_name);
                            }
                        }
                    }
                }
            }
        }

        str.erase(0, pos + 1);
    }
}

#if !SQL_SERVER
int main()
{
    SQL db;
    db.load_tables();

    unsigned int threads = std::thread::hardware_concurrency();
    if (threads == 0)
        threads = 4;
    ThreadPool pool(threads);
    std::mutex db_mutex;

    std::cout << "this is SQL" << std::endl;
    std::cout << "enter 'help' for help" << std::endl;

    while (true)
    {
        std::cout << "SQL|> ";
        std::string code;
        std::getline(std::cin, code);
        code = trim(code);
        if (code == "EXIT;" || code == "exit;")
        {
            return 0;
        }
        if (code == "help")
        {
            std::cout << "Commands:" << std::endl;
            std::cout << "CREATE TABLE ...;" << std::endl;
            std::cout << "INSERT INTO ... VALUES;" << std::endl;
            std::cout << "SELECT ...;" << std::endl;
            std::cout << "DELETE ...;" << std::endl;
            std::cout << "UPDATE ...;" << std::endl;
            std::cout << "EXIT;" << std::endl;
            continue;
        }
        // Run SQL_compiler on the thread pool. Access to `db` is serialized by `db_mutex`.
        pool.enqueue([&db, &db_mutex, code]()
                     {
            std::lock_guard<std::mutex> lock(db_mutex);
            SQL_compiler(db, code); });
    }

    return 0;
}
#endif
