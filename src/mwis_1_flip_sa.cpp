#pragma GCC optimize("O3,unroll-loops")
#pragma GCC target("avx2,bmi,bmi2,lzcnt,popcnt")

#include <iostream>
#include <vector>
#include <numeric>
#include <algorithm>
#include <chrono>
#include <random>
#include <cmath>
#include <fstream>
#include <string>

using namespace std;

// Configuration Constants
// We use 295.0 seconds (4m 55s) to guarantee the program finishes gracefully 
// and prints output before the strict 5-minute system termination.
const double TIME_LIMIT = 295.0; 

int main(int argc, char* argv[]) {
    // --------------------------------------------------------
    // 0. FAST I/O SETUP
    // --------------------------------------------------------
    // Essential for reading potentially millions of integers quickly.
    ios_base::sync_with_stdio(false);
    cin.tie(NULL);


    ifstream file_in;
    ofstream file_out;
    streambuf* cinbuf = cin.rdbuf();
    streambuf* coutbuf = cout.rdbuf();

    if (argc > 1) {
        string in_filename = argv[1];
        file_in.open(in_filename);
        if (!file_in) {
            cerr << "Error: Could not open input file " << in_filename << "\n";
            return 1;
        }
        cin.rdbuf(file_in.rdbuf());

        string base_filename = in_filename;
        size_t slash_pos = base_filename.find_last_of("/\\");
        if (slash_pos != string::npos) {
            base_filename = base_filename.substr(slash_pos + 1);
        }

        size_t pos = base_filename.find("input");
        if (pos != string::npos) {
            base_filename.replace(pos, 5, "output");
        } else {
            base_filename += ".out";
        }
        
        file_out.open(base_filename);
        if (!file_out) {
            cerr << "Error: Could not open output file " << base_filename << "\n";
            return 1;
        }
        cout.rdbuf(file_out.rdbuf());
    }


    // We start the timer immediately. If reading massive inputs takes 30 seconds,
    // the algorithm will perfectly adapt and just run Simulated Annealing for less time.
    auto start_time = chrono::steady_clock::now();

    int N;
    long long M;
    if (!(cin >> N >> M)) return 0;

    vector<long long> weight(N);
    double total_weight = 0.0;
    for (int i = 0; i < N; ++i) {
        cin >> weight[i];
        total_weight += weight[i];
    }

    // --------------------------------------------------------
    // 1. INPUT + GRAPH BUILDING
    // --------------------------------------------------------
    // Using an Adjacency List to avoid Memory Limit Exceeded (MLE).
    vector<vector<int>> adj(N);
    for (long long i = 0; i < M; ++i) {
        int u, v;
        cin >> u >> v;
        u--; v--; // Convert to 0-based indexing for arrays
        adj[u].push_back(v);
        adj[v].push_back(u);
    }

    if (file_in.is_open()) file_in.close();

    // Memory optimization: shrink excess capacity in vectors
    for (int i = 0; i < N; ++i) {
        adj[i].shrink_to_fit();
    }

    // --------------------------------------------------------
    // 2. GRAPH REDUCTION / PREPROCESSING
    // --------------------------------------------------------
    // vector<uint8_t> is used instead of vector<bool> because it avoids 
    // the slow bit-shifting overhead of vector<bool> in C++, making loops faster.
    vector<uint8_t> locked(N, 0);
    vector<uint8_t> in_team(N, 0);
    vector<int> conflicts_count(N, 0); 
    long long current_score = 0;

    // Isolate Degree-0 nodes: 
    // If a node has no rivals, they are an automatic perfect addition.
    for (int i = 0; i < N; ++i) {
        if (adj[i].empty()) {
            locked[i] = 1;
            in_team[i] = 1;
            current_score += weight[i];
        }
    }

    // --------------------------------------------------------
    // 3. INITIAL GREEDY SOLUTION
    // --------------------------------------------------------
    // We sort the remaining candidates by a heuristic: (Weight / Degree).
    // This prioritizes high-wieght nodes who don't block many other nodes.
    vector<int> order(N);
    for(int i=0; i<N; i++){order[i]=i;}
    sort(order.begin(), order.end(), [&](int a, int b) {
        double ratioA = (double)weight[a] / (adj[a].size() + 1);
        double ratioB = (double)weight[b] / (adj[b].size() + 1);
        if (ratioA != ratioB) return ratioA > ratioB;
        return weight[a] > weight[b]; // Break ties by raw skill
    });

    for (int u : order) {
        if (locked[u]) continue; 
        
        // If adding this node causes 0 conflicts with our current set, add them.
        if (conflicts_count[u] == 0) {
            in_team[u] = 1;
            current_score += weight[u];
            // Update their neighbors: they now have 1 more conflicting neighbor on the team
            for (int v : adj[u]) {
                conflicts_count[v]++;
            }
        }
    }

    // Save our greedy baseline
    long long best_score = current_score;
    vector<uint8_t> best_team = in_team;

    // --------------------------------------------------------
    // 4. LOCAL SEARCH + SIMULATED ANNEALING
    // --------------------------------------------------------
    mt19937 rng(1337); // Fixed seed for reproducible  testing
    uniform_real_distribution<double> dist01(0.0, 1.0);
    uniform_int_distribution<int> distNode(0, N - 1);

    // Initial Temperature based on average weight scales properly to the problem size.
    double T0 = (total_weight / N) * 0.5; 
    if (T0 < 1.0) T0 = 1.0; 
    double Tf = 1e-3; // Final resting temperature
    double T = T0;

    int iter = 0;
    while (true) {
        // Bitwise trick `(iter & 255) == 0` ensures we only check the clock 
        // every 256 iterations to drastically reduce `chrono` system call overhead.
        if ((iter & 255) == 0) {
            auto now = chrono::steady_clock::now();
            double elapsed = chrono::duration<double>(now - start_time).count();
            
            // Time-based stopping condition
            if (elapsed > TIME_LIMIT) break; 
            
            // Exponential cooling schedule
            T = T0 * pow(Tf / T0, elapsed / TIME_LIMIT);
        }
        iter++;

        // Randomly perturb the state
        int u = distNode(rng);
        if (locked[u]) continue;

        long long delta = 0;
        vector<int> to_remove;
        
        if (!in_team[u]) {
            // State mutation: Try ADDING  'u'
            delta = weight[u];
            // If 'u' is added, we MUST remove all their neighbors currently on the team
            // to maintain absolute conflict-free feasibility.
            for (int v : adj[u]) {
                if (in_team[v]) {
                    delta -= weight[v];
                    to_remove.push_back(v);
                }
            }
        } 
        
        else {
            // State mutation: Try REMOVING  'u'
            delta = -weight[u];
            to_remove.push_back(u);
        }

        // Acceptance criterion: 
        // Always accept if it improves score (delta > 0). 
        // If it lowers the score, accept it probabilistically based on the Temperature.
        if (delta > 0 || exp((double)delta / T) > dist01(rng)) {
            if (in_team[u]) {
                // Apply removal
                in_team[u] = 0;
                for (int v : adj[u]) conflicts_count[v]--;
                current_score += delta;
            }
             else {
                // Apply addition: Remove conflicting neighbors first
                for (int w : to_remove) {
                    in_team[w] = 0;
                    for (int v : adj[w]) conflicts_count[v]--;
                }
                // Then lock in 'u'
                in_team[u] = 1;
                for (int v : adj[u]) conflicts_count[v]++;
                current_score += delta;
            }

            // Diversification/Intensification: Track global maximum seen
            if (current_score > best_score) {
                best_score = current_score;
                best_team = in_team;
            }
        }
    }

    // --------------------------------------------------------
    // 5. FINAL OUTPUT
    // --------------------------------------------------------
    cout << best_score << "\n";
    bool first = true;
    for (int i = 0; i < N; ++i) {
        if (best_team[i]) {
            if (!first) cout << " ";
            cout << (i + 1); // Output 1-based index
            first = false;
        }
    }
    cout << "\n";

    if (argc > 1) {
        cin.rdbuf(cinbuf);
        cout.rdbuf(coutbuf);
        if (file_out.is_open()) file_out.close();
    }

    return 0;
}