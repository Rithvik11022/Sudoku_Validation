#include <iostream>
#include <vector>
#include <fstream>
#include <pthread.h>
#include <sstream>
#include <math.h>
#include <chrono>
#include <atomic>

using namespace std;

int N, K;

std::atomic<bool> validity = true;

vector<vector<int>> sudoku;
vector<vector<int>> rows;
vector<vector<int>> columns;
vector<vector<int>> grids;

ofstream out("output.txt");
// ofstream Data("data.txt");

vector<stringstream> thread_buffers_chunk;
vector<stringstream> thread_buffers_mixed;

/* for the sake of passing a single argument in a void* function */
class t_data
{
public:
    int t_id;
    int index;
    int step;
    bool is_valid;
    int chunk_size;
    t_data() : t_id(0), index(0), step(0), is_valid(true), chunk_size(0) {}

    t_data(int start, int size_chunk, int t_id)
    {
        index = start;
        chunk_size = size_chunk;
        step = 1;
        is_valid = true;
        this->t_id = t_id;
    }

    t_data(int start, int step, int size_chunk, int t_id)
    {
        index = start;
        this->step = step;
        chunk_size = 0;
        is_valid = true;
        this->t_id = t_id;
    }
};

/* initialized rows,columns and grids */
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

/* the functions which are to be used by threads */
void *row_validator_chunk(void *arg)
{
    t_data *numbers = (t_data *)arg;
    stringstream &buffer = thread_buffers_chunk[numbers->t_id];

    for (int k = numbers->index; k < numbers->index + numbers->chunk_size; k++)
    {
        bool v = true;
        vector<bool> visited(N+1, false);
        for (int i = 0; i < N; i++)
        {
            if (rows[k][i] <= 0 || rows[k][i] > N || visited[rows[k][i]] == true)
            {
                
                numbers->is_valid = false;
                v = false;
                validity.store(false);
            }
            visited[rows[k][i]] = true;
        }
        buffer << "Thread " << numbers->t_id + 1 << " checks row " << k + 1 << " and is " << (v ? "valid" : "invalid") << endl;
    }
    return NULL;
}

void *column_validator_chunk(void *arg)
{
    t_data *numbers = (t_data *)arg;
    stringstream &buffer = thread_buffers_chunk[numbers->t_id]; // Access thread-specific buffer

    for (int k = numbers->index; k < numbers->index + numbers->chunk_size; k++)
    {
        bool v = true;
        vector<bool> visited(N+1, false);
        for (int i = 0; i < N; i++)
        {
            if (columns[k][i] <= 0 || columns[k][i] > N || visited[columns[k][i]] == true)
            {
                numbers->is_valid = false;
                v = false;
                validity.store(false);
            }
            visited[columns[k][i]] = true;
        }
        buffer << "Thread " << numbers->t_id + 1 << " checks column " << k + 1 << " and is " << (v ? "valid" : "invalid") << endl;
    }
    return NULL;
}

void *grid_validator_chunk(void *arg)
{
    t_data *numbers = (t_data *)arg;
    stringstream &buffer = thread_buffers_chunk[numbers->t_id]; // Access thread-specific buffer

    for (int k = numbers->index; k < numbers->index + numbers->chunk_size; k++)
    {
        bool v = true;
        vector<bool> visited(N+1, false);
        for (int i = 0; i < N; i++)
        {
            if (grids[k][i] <= 0 || grids[k][i] > N || visited[grids[k][i]] == true)
            {
                numbers->is_valid = false;
                v = false;
                validity.store(false);
            }
            visited[grids[k][i]] = true;
        }
        buffer << "Thread " << numbers->t_id + 1 << " checks grid " << k + 1 << " and is " << (v ? "valid" : "invalid") << endl;
    }
    return NULL;
}

void exec_chunk(std::vector<pthread_t> threads)
{
    // auto start = std::chrono::high_resolution_clock::now();

    vector<t_data> thread_data;
    thread_data.reserve(K);
    thread_buffers_chunk.resize(K); // Resize buffers for each thread

    int threads_R = K / 3;
    int threads_C = K / 3; 
    int threads_G = K - 2 * (K / 3);

    std::vector<int> gp_R(threads_R);
    std::vector<int> gp_C(threads_C);
    std::vector<int> gp_G(threads_G);

    auto seg = [&](int n, int k, std::vector<int> &gp) -> void
    {
        int temp = n / k;
        for (auto &ele : gp)
            ele = temp;

        for (int i = 0; i < n % k; i++)
        {
            gp[i]++;
        }
    };

    seg(N, threads_R, gp_R);
    int a = 0;
    auto start1 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < threads_R; i++)
    {
        thread_data.push_back(t_data(a, gp_R[i], i));
        pthread_create(&threads[i], NULL, row_validator_chunk, (void *)&(thread_data[thread_data.size() - 1]));
        a += gp_R[i];
    }
    // auto end1 = std::chrono::high_resolution_clock::now();
    seg(N, threads_C, gp_C);
    a = 0;
    // auto start2 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < threads_C; i++)
    {
        thread_data.push_back(t_data(a, gp_C[i], i + threads_R));
        pthread_create(&(threads[i + threads_R]), NULL, column_validator_chunk, (void *)&(thread_data[thread_data.size() - 1]));
        a += gp_C[i];
    }
    // auto end2 = std::chrono::high_resolution_clock::now();

    seg(N, threads_G,gp_G);
    a = 0;
    // auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < threads_G; i++)
    {
        thread_data.push_back(t_data(a, gp_G[i], i + threads_R + threads_C));
        pthread_create(&(threads[i + threads_R + threads_C]), NULL, grid_validator_chunk, (void *)&(thread_data[thread_data.size() - 1]));
        a += gp_G[i];
    }
    for (int i = 0; i < K; i++)
    {
        pthread_join(threads[i], NULL);
    }
    auto end= std::chrono::high_resolution_clock::now();

    // Combine all buffers into the output stream
    for (const auto &buffer : thread_buffers_chunk)
    {
        out << buffer.str();
    }

    out << "Sudoku is " << (validity ? "valid" : "invalid") << endl;
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end-start1);
    cout << "Time taken for chunk validation is " << duration.count() << " microseconds" << endl;
    out << "Total execution time is "<<duration.count()<<" microseconds"<<endl;
}

void *row_validator_mixed(void *arg)
{
    t_data *numbers = (t_data *)arg;
    stringstream &buffer = thread_buffers_mixed[numbers->t_id]; // Access thread-specific buffer

    for (int k = numbers->index; k < N; k += numbers->step)
    {
        bool v = true;
        vector<bool> visited(N+1, false);
        for (int i = 0; i < N; i++)
        {
            if (rows[k][i] <= 0 || rows[k][i] > N || visited[rows[k][i]] == true)
            {
                numbers->is_valid = false;
                v = false;
                validity.store(false);
            }
            visited[rows[k][i]] = true;
        }
        buffer << "Thread " << numbers->t_id + 1 << " checks row " << k + 1 << " and is " << (v ? "valid" : "invalid") << endl;
    }
    return NULL;
}

void *column_validator_mixed(void *arg)
{
    t_data *numbers = (t_data *)arg;
    stringstream &buffer = thread_buffers_mixed[numbers->t_id];

    for (int k = numbers->index; k < N; k += numbers->step)
    {
        bool v = true;
        vector<bool> visited(N+1, false);
        for (int i = 0; i < N; i++)
        {
            if (columns[k][i] <= 0 || columns[k][i] > N || visited[columns[k][i]] == true)
            {
                numbers->is_valid = false;
                v = false;
                validity.store(false);
            }
            visited[columns[k][i]] = true;
        }
        buffer << "Thread " << numbers->t_id + 1 << " checks column " << k + 1 << " and is " << (v ? "valid" : "invalid") << endl;
    }
    return NULL;
}

void *grid_validator_mixed(void *arg)
{
    t_data *numbers = (t_data *)arg;
    stringstream &buffer = thread_buffers_mixed[numbers->t_id];

    for (int k = numbers->index; k < N; k += numbers->step)
    {
        bool v = true;
        vector<bool> visited(N+1, false);
        for (int i = 0; i < N; i++)
        {
            if (grids[k][i] <= 0 || grids[k][i] > N || visited[grids[k][i]] == true)
            {
                numbers->is_valid = false;
                v = false;
                validity.store(false);
            }
            visited[grids[k][i]] = true;
        }
        buffer << "Thread " << numbers->t_id + 1 << " checks grid " << k + 1 << " and is " << (v ? "valid" : "invalid") << endl;
    }
    return NULL;
}
/* */

void exec_mixed(vector<pthread_t> threads)
{
    // auto start = std::chrono::high_resolution_clock::now();
    vector<t_data> thread_data(K);
    thread_buffers_mixed.resize(K); // Resize thread-specific buffers for all threads

    int threads_R = K / 3;
    int threads_C = K / 3;
    int threads_G = K - 2 * (K / 3);
    auto start = std::chrono::high_resolution_clock::now();
    // Row validator threads
    for (int i = 0; i < threads_R; i++)
    {
        thread_data[i] = t_data(i, threads_R, 0, i);
        pthread_create(&threads[i], NULL, row_validator_mixed, (void *)&(thread_data[i]));
    }

    // Column validator threads
    for (int i = 0; i < threads_C; i++)
    {
        thread_data[i + threads_R] = t_data(i, threads_C, 0, i + threads_R);
        pthread_create(&threads[i + threads_R], NULL, column_validator_mixed, (void *)&(thread_data[i + threads_R]));
    }

    // Grid validator threads
    for (int i = 0; i < threads_G; i++)
    {
        thread_data[i + threads_C + threads_R] = t_data(i, threads_G, 0, i + threads_C + threads_R);
        pthread_create(&threads[i + threads_R + threads_C], NULL, grid_validator_mixed, (void *)&(thread_data[i + threads_R + threads_C]));
    }

    // Wait for threads to finish
    for (int i = 0; i < K; i++)
    {
        pthread_join(threads[i], NULL);
    }
    auto end= std::chrono::high_resolution_clock::now();

    // Combine all buffers into the output stream
    for (const auto &buffer : thread_buffers_mixed)
    {
        out << buffer.str();
    }

    out << "Sudoku is " << (validity ? "valid" : "invalid") << endl;
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end-start);
    out << "Total execution time is "<<duration.count()<<" microseconds"<<endl;
    cout << "Time taken for mixed validation is "<<duration.count()<<" microseconds"<<endl;
}

/* */
bool validation(vector<int> num)
{
    vector<bool> visited(N+1, false);
    for (int i = 0; i < N; i++)
    {
        if (num[i] <= 0 or num[i] > N or visited[num[i]] == true)
            return false;
        visited[num[i]] = true;
    }
    return true;
}

/* */
void exec_seq()
{
    auto start = std::chrono::high_resolution_clock::now();
    bool valid = true;

    for (int i = 0; i < N; i++)
    {
        if (!validation(rows[i]) || !validation(columns[i]) || !validation(grids[i]))
            valid = false;
    }

    out << "Sudoku is " << (valid ? "valid" : "invalid") << endl;
    auto end= std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end-start);
    out << "Total execution time is "<<duration.count()<<" microseconds"<<endl;
    cout << "Time taken for sequential validation is "<<duration.count()<<" microseconds"<<endl;
}

int main()
{
    /*file input*/
    ifstream input("inp.txt");
    string line;
    if (input.is_open())
    {
        getline(input, line);
        stringstream ss(line);
        ss >> K >> N;
        while (getline(input, line))
        {
            vector<int> row;
            stringstream ss(line);
            int value;
            while (ss >> value)
            {
                row.push_back(value);
            }
            sudoku.push_back(row);
        }
        input.close();
    }
    segregator();

    /*thread initialization*/
    std::vector<pthread_t> threads(K);

    /* different algorithms*/
    exec_chunk(threads);
    exec_mixed(threads);
    exec_seq();
}