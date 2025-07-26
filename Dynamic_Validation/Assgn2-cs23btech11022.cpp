#include <iostream>
#include <vector>
#include <fstream>
#include <thread>
#include <sstream>
#include <cmath>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <atomic>

using namespace std;

using namespace std::chrono;

int K, N, taskinc;

atomic<bool> sud_validity{true};

int counter = 0;

vector<stringstream> print_buffers;

vector<vector<int>> sudoku;

vector<vector<int>> rows;

vector<vector<int>> columns;

vector<vector<int>> grids;

vector<vector<int>> fragments;

ofstream out("output.txt");

struct ThreadTiming
{

    vector<int> cs_entry_times;

    vector<int> cs_exit_times;
};

vector<ThreadTiming> thread_timings;

void segregator()
{

    for (int i = 0; i < N; i++)
    {

        rows.push_back(sudoku[i]);
    }

    for (int j = 0; j < N; j++)
    {

        vector<int> column;

        for (int i = 0; i < N; i++)
        {

            column.push_back(sudoku[i][j]);
        }

        columns.push_back(column);
    }

    int n = sqrt(N);

    for (int i = 0; i < n; i++)
    {

        for (int j = 0; j < n; j++)
        {

            vector<int> grid;

            for (int x = 0; x < n; x++)
            {

                for (int y = 0; y < n; y++)
                {

                    int a = i * n + x;

                    int b = j * n + y;

                    grid.push_back(sudoku[a][b]);
                }
            }

            grids.push_back(grid);
        }
    }
}

class TASlock
{

private:
    atomic<bool> flag{false};

public:
    void lock()
    {

        while (flag.exchange(true))
        {
        }
    }

    void unlock()
    {

        flag.store(false);
    }
};

class CASlock
{

private:
    atomic<bool> lock_flag;

public:
    CASlock() : lock_flag(false) {}

    void lock()
    {

        bool expected = false;

        while (!lock_flag.compare_exchange_weak(expected, true))
        {

            expected = false;
        }
    }

    void unlock()
    {

        lock_flag.store(false);
    }
};

class BoundedCASlock
{

private:
    atomic<bool> locked = false;

    int backoff_limit;

public:
    BoundedCASlock(int limit = 100) : backoff_limit(limit) {}

    void lock(int t_id)
    {

        vector<int> backoff_counter(K, 0);

        while (true)
        {

            bool expected = false;

            if (!locked.compare_exchange_strong(expected, true))
            {

                backoff_counter[t_id]++;

                if (backoff_counter[t_id] > backoff_limit)
                {

                    this_thread::yield();

                    backoff_counter[t_id] = 0;
                }
            }
            else
            {

                return;
            }
        }
    }

    void unlock()
    {

        locked.store(false);
    }
};

bool validator(vector<int> ele, int n)
{

    vector<bool> visited(n + 1, false);
    for (int i = 0; i < n; i++)
    {
        if (ele[i] <= 0 || ele[i] > n || visited[ele[i]])
            return false;

        visited[ele[i]] = true;
    }
    return true;
}

std::string entity(int i)
{

    if (i < N)
    {

        return "row " + std::to_string(i + 1);
    }
    else if (i < 2 * N)
    {

        return "column " + std::to_string(i - N + 1);
    }

    return "grid " + std::to_string(i - 2 * N + 1);
}

string get_current_time()
{

    auto now = system_clock::now();

    auto now_c = system_clock::to_time_t(now);

    stringstream ss;

    ss << put_time(localtime(&now_c), "%H:%M:%S");

    return ss.str();
}

TASlock tas_lock;

CASlock cas_lock;

BoundedCASlock bounded_cas_lock(100);

void task_TAS(int c)
{

    stringstream buffer;

    while (sud_validity && counter < 3 * N)
    {

        auto start_cs_entry = high_resolution_clock::now();

        buffer << "Thread " << c + 1 << " requests to enter CS at time " << get_current_time() << endl;

        tas_lock.lock();

        auto end_cs_entry = high_resolution_clock::now();

        buffer << "Thread " << c + 1 << " enters CS at time " << get_current_time() << endl;

        auto duration_cs_entry = duration_cast<nanoseconds>(end_cs_entry - start_cs_entry);

        thread_timings[c].cs_entry_times.push_back(duration_cs_entry.count());

        int temp;

        int tasklength = 0;

        if (counter + taskinc < 3 * N)
        {

            temp = counter + taskinc;

            tasklength = taskinc;
        }
        else
        {

            temp = 3 * N;

            tasklength = 3 * N - counter;
        }

        int start_task = counter;

        counter = temp;

        auto start_cs_exit = high_resolution_clock::now();

        tas_lock.unlock();

        auto end_cs_exit = high_resolution_clock::now();

        auto duration_cs_exit = duration_cast<nanoseconds>(end_cs_exit - start_cs_exit);

        thread_timings[c].cs_exit_times.push_back(duration_cs_exit.count());

        buffer << "Thread " << c + 1 << " exits CS at time " << get_current_time() << endl;

        buffer << "Thread " << c + 1 << " grabs " << entity(start_task) << " to " << entity(temp - 1)
               << " at time " << get_current_time() << endl;

        for (int i = start_task; i < temp; i++)
        {

            bool v = validator(fragments[i], N);

            buffer << "Thread " << c + 1 << " checks " << entity(i) << " at time " << get_current_time()
                   << " and finds it " << (v ? "valid" : "invalid") << endl;

            if (!v)
            {

                sud_validity = false;

                // buffer << "Thread " << c + 1 << " sets sudoku_valid to false at time " << get_current_time()
                //        << endl;

                break;
            }
        }

        print_buffers.push_back(move(buffer));
    }
}

void task_CAS(int c)
{

    stringstream buffer;

    while (sud_validity && counter < 3 * N)
    {

        auto start_cs_entry = high_resolution_clock::now();

        buffer << "Thread " << c + 1 << " requests to enter CS at time " << get_current_time() << endl;

        cas_lock.lock();

        auto end_cs_entry = high_resolution_clock::now();

        buffer << "Thread " << c + 1 << " enters CS at time " << get_current_time() << endl;

        auto duration_cs_entry = duration_cast<nanoseconds>(end_cs_entry - start_cs_entry);

        thread_timings[c].cs_entry_times.push_back(duration_cs_entry.count());

        int temp;

        int tasklength = 0;

        if (counter + taskinc < 3 * N)
        {

            temp = counter + taskinc;

            tasklength = taskinc;
        }
        else
        {

            temp = 3 * N;

            tasklength = 3 * N - counter;
        }
        int start_task = counter;
        counter = temp;

        auto start_cs_exit = high_resolution_clock::now();

        cas_lock.unlock();

        auto end_cs_exit = high_resolution_clock::now();

        auto duration_cs_exit = duration_cast<nanoseconds>(end_cs_exit - start_cs_exit);

        thread_timings[c].cs_exit_times.push_back(duration_cs_exit.count());

        buffer << "Thread " << c + 1 << " exits CS at time " << get_current_time() << endl;

        buffer << "Thread " << c + 1 << " grabs " << entity(start_task) << " to " << entity(temp - 1)
               << " at time " << get_current_time() << endl;

        for (int i = start_task; i < temp; i++)
        {

            bool v = validator(fragments[i], N);

            buffer << "Thread " << c + 1 << " checks " << entity(i) << " at time " << get_current_time()
                   << " and finds it " << (v ? "valid" : "invalid") << endl;

            if (!v)
            {

                sud_validity = false;

                // buffer << "Thread " << c + 1 << " sets sudoku_valid to false at time " << get_current_time()
                //        << endl;

                break;
            }
        }

        print_buffers.push_back(move(buffer));
    }
}

void task_CASb(int c)
{

    stringstream buffer;

    while (sud_validity && counter < 3 * N)
    {

        auto start_cs_entry = high_resolution_clock::now();

        buffer << "Thread " << c + 1 << " requests to enter CS at time " << get_current_time() << endl;

        bounded_cas_lock.lock(c);

        auto end_cs_entry = high_resolution_clock::now();

        buffer << "Thread " << c + 1 << " enters CS at time " << get_current_time() << endl;

        auto duration_cs_entry = duration_cast<nanoseconds>(end_cs_entry - start_cs_entry);

        thread_timings[c].cs_entry_times.push_back(duration_cs_entry.count());

        int temp;

        int tasklength = 0;

        if (counter + taskinc < 3 * N)
        {

            temp = counter + taskinc;

            tasklength = taskinc;
        }
        else
        {

            temp = 3 * N;

            tasklength = 3 * N - counter;
        }

        int start_task = counter;

        counter = temp;

        auto start_cs_exit = high_resolution_clock::now();

        bounded_cas_lock.unlock();

        auto end_cs_exit = high_resolution_clock::now();

        auto duration_cs_exit = duration_cast<nanoseconds>(end_cs_exit - start_cs_exit);

        thread_timings[c].cs_exit_times.push_back(duration_cs_exit.count());

        buffer << "Thread " << c + 1 << " exits CS at time " << get_current_time() << endl;

        buffer << "Thread " << c + 1 << " grabs " << entity(start_task) << " to " << entity(temp - 1)
               << " at time " << get_current_time() << endl;

        for (int i = start_task; i < temp; i++)
        {

            bool v = validator(fragments[i], N);

            buffer << "Thread " << c + 1 << " checks " << entity(i) << " at time " << get_current_time()
                   << " and finds it " << (v ? "valid" : "invalid") << endl;

            if (!v)
            {

                sud_validity = false;

                // buffer << "Thread " << c + 1 << " sets sudoku_valid to false at time " << get_current_time()
                //        << endl;

                break;
            }
        }

        print_buffers.push_back(move(buffer));
    }
}

int main()
{

    ifstream input("inp.txt");

    string line;

    if (input.is_open())
    {

        getline(input, line);

        stringstream ss(line);

        ss >> K >> N >> taskinc;

        sudoku.resize(N, vector<int>(N));

        for (int i = 0; i < N; ++i)
        {

            if (getline(input, line))
            {

                stringstream row_stream(line);

                for (int j = 0; j < N; ++j)
                {

                    row_stream >> sudoku[i][j];
                }
            }
        }

        input.close();
    }
    else
    {

        cerr << "Error: Unable to open input file" << endl;

        return 1;
    }

    segregator();

    fragments.reserve(3 * N);

    for (int i = 0; i < N; i++)
    {

        fragments.push_back(rows[i]);
    }

    for (int i = 0; i < N; i++)
    {

        fragments.push_back(columns[i]);
    }

    for (int i = 0; i < N; i++)
    {

        fragments.push_back(grids[i]);
    }

    thread_timings.resize(K);

    vector<string> lock_types = {"TAS", "CAS", "Bounded CAS"};

    int x;

    cout << "Choose locking type" << endl;

    cout << "1.TAS" << endl;

    cout << "2.CAS" << endl;

    cout << "3.Bounded CAS" << endl;

    cin >> x;

    cout << "Running with " << lock_types[--x] << " lock:" << endl;

    print_buffers.clear();

    for (auto &timing : thread_timings)
    {

        timing.cs_entry_times.clear();

        timing.cs_exit_times.clear();
    }

    auto start_time = high_resolution_clock::now();

    vector<thread> threads;

    for (int i = 0; i < K; i++)
    {

        if (x == 0)
        {

            threads.emplace_back(task_TAS, i);
        }
        else if (x == 1)
        {

            threads.emplace_back(task_CAS, i);
        }
        else
        {

            threads.emplace_back(task_CASb, i);
        }
    }

    for (auto &t : threads)
    {

        t.join();
    }

    auto end_time = high_resolution_clock::now();

    auto total_duration = duration_cast<nanoseconds>(end_time - start_time);


    for (const auto &buffer : print_buffers)
    {

        out << buffer.str();
    }

    cout << "Sudoku is " << (sud_validity ? "valid" : "invalid") << endl;

    cout << "Total execution time: " << total_duration.count()/1000 << " microseconds" << endl;

    int total_entry_time = 0, total_exit_time = 0;

    int max_entry_time = 0, max_exit_time = 0;

    int total_cs_operations = 0;

    for (const auto &timing : thread_timings)
    {

        for (int time : timing.cs_entry_times)
        {

            total_entry_time += time;

            max_entry_time = max(max_entry_time, time);
        }

        for (int time : timing.cs_exit_times)
        {

            total_exit_time += time;

            max_exit_time = max(max_exit_time, time);
        }

        total_cs_operations += timing.cs_entry_times.size();
    }

    double avg_entry_time = static_cast<double>(total_entry_time) / total_cs_operations;

    double avg_exit_time = static_cast<double>(total_exit_time) / total_cs_operations;

    cout << "Average time taken by a thread to enter the CS: " << avg_entry_time/1000 << " microseconds" << endl;

    cout << "Average time taken by a thread to exit the CS: " << avg_exit_time/1000 << " microseconds" << endl;

    cout << "Worst-case time taken by a thread to enter the CS: " << max_entry_time/1000 << " microseconds" << endl;

    cout << "Worst-case time taken by a thread to exit the CS: " << max_exit_time/1000 << " microseconds" << endl;

    cout << endl;

    return 0;
}
