#pragma GCC optimize("O3,unroll-loops")
#pragma GCC target("avx2,bmi,bmi2,lzcnt,popcnt")

#include <iostream>
#include <vector>
#include <numeric>
#include <algorithm>
#include <chrono>
#include <random>
#include <cmath>
#include <queue>
#include <fstream>
#include <string>

using namespace std;

// We use 295.0 seconds (4m 55s) to guarantee the program finishes gracefully 
// before the strict 5-minute system termination.
const double TOTAL_TIME_LIMIT = 295.0; 

auto global_start_time = chrono::steady_clock::now();

inline double get_elapsed_time() {
    return chrono::duration<double>(chrono::steady_clock::now() - global_start_time).count();
}

int main(int argc, char* argv[]) {

    // FAST I/O SETUP

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

    int N;
    long long M;
    if (!(cin >> N >> M)) return 0;

    vector<long long> weight(N);
    for (int i = 0; i < N; ++i) {
        cin >> weight[i];
    }

    // Build initial adjacency list
    vector<vector<int>> adj(N);
    for (long long i = 0; i < M; ++i) {
        int u, v;
        cin >> u >> v;
        u--; v--; // Convert to 0-based indexing internally
        adj[u].push_back(v);
        adj[v].push_back(u);
    }

    if (file_in.is_open()) file_in.close();

    for (int i = 0; i < N; ++i) {
        adj[i].shrink_to_fit();
    }

    vector<uint8_t> active_node(N, 1);
    vector<uint8_t> final_team(N, 0);
    long long final_score = 0;


    // ADVANCED GRAPH REDUCTIONS (ITERATIVE PEELING)
    
    queue<int> Q;
    for (int i = 0; i < N; ++i) {
        if (adj[i].size() <= 2) Q.push(i);
    }

    while (!Q.empty()) {
        int u = Q.front();
        Q.pop();
        if (!active_node[u]) continue;
        
        vector<int> neighbors;
        for (int v : adj[u]) {
            if (active_node[v]) neighbors.push_back(v);
        }
        int d = neighbors.size();
        
        if (d == 0) {
            // Rule 1: Degree-0
            final_team[u] = 1;
            final_score += weight[u];
            active_node[u] = 0;
        } 
        
        else if (d == 1) {
            // Rule 2: Degree-1 Domination
            int v = neighbors[0];
            if (weight[u] >= weight[v]) {
                final_team[u] = 1;
                final_score += weight[u];
                active_node[u] = 0;
                active_node[v] = 0; // v is permanently discarded
                // v's neighbors might have reduced degree, push them to check
                for (int w : adj[v]) if (active_node[w]) Q.push(w);
            }
        } 
        
        else if (d == 2) {
            // Rule 3: Degree-2 Folding (Path Reduction)
            int v = neighbors[0];
            int w_node = neighbors[1];
            if (weight[u] >= weight[v] + weight[w_node]) {
                final_team[u] = 1;
                final_score += weight[u];
                active_node[u] = 0;
                active_node[v] = 0;
                active_node[w_node] = 0;
                // v and w are discarded, check their neighbors
                for (int x : adj[v]) if (active_node[x]) Q.push(x);
                for (int x : adj[w_node]) if (active_node[x]) Q.push(x);
            }
        }
    }

     
    //GRAPH DECOMPOSITION (CONNECTED COMPONENTS)
    
    vector<vector<int>> clean_adj(N);
    for (int i = 0; i < N; ++i) {
        if (active_node[i]) {
            for (int v : adj[i]) {
                if (active_node[v]) {
                    clean_adj[i].push_back(v);
                }
            }
        }
    }

    vector<vector<int>> components;
    vector<uint8_t> visited(N, 0);
    int total_active = 0;

    for (int i = 0; i < N; ++i) {
        if (active_node[i] && !visited[i]) {
            vector<int> comp;
            queue<int> q;
            q.push(i);
            visited[i] = 1;
            while (!q.empty()) {
                int curr = q.front();
                q.pop();
                comp.push_back(curr);
                for (int nxt : clean_adj[curr]) {
                    if (!visited[nxt]) {
                        visited[nxt] = 1;
                        q.push(nxt);
                    }
                }
            }
            components.push_back(comp);
            total_active += comp.size();
        }
    }

    
    // TIME ALLOCATION & COMPONENT-LEVEL SOLVING
    
    mt19937 rng(1337);
    uniform_real_distribution<double> dist01(0.0, 1.0);

    for (const auto& comp : components) {
        if (comp.empty()) continue;

        double remaining_time = max(0.0, TOTAL_TIME_LIMIT - get_elapsed_time());
        double comp_time_limit = remaining_time * ((double)comp.size() / total_active);
        total_active -= comp.size();
        double comp_start_time = get_elapsed_time();

        if (comp.size() == 1) {
            final_team[comp[0]] = 1;
            final_score += weight[comp[0]];
            continue;
        }

        // Step A: Dual-Heuristic Greedy Initialization (Standard / SQRT)
        auto run_greedy = [&](int mode) {
            vector<uint8_t> in_team(N, 0);
            long long score = 0;
            
            vector<int> order = comp;
            sort(order.begin(), order.end(), [&](int a, int b) {
                double scoreA, scoreB;
                if (mode == 1) {
                    scoreA = (double)weight[a] / (clean_adj[a].size() + 1.0);
                    scoreB = (double)weight[b] / (clean_adj[b].size() + 1.0);
                } 
                else {
                    scoreA = (double)weight[a] / sqrt(clean_adj[a].size() + 1.0);
                    scoreB = (double)weight[b] / sqrt(clean_adj[b].size() + 1.0);
                }
                if (scoreA != scoreB) return scoreA > scoreB;
                return weight[a] > weight[b]; 
            });

             for (int u : order) {
                bool can_add = true;
                for (int v : clean_adj[u]) if (in_team[v]) { can_add = false; break; }
                if (can_add) { in_team[u] = 1; score += weight[u]; }
            }
            return make_pair(score, in_team);
        };

        auto [score1, team1] = run_greedy(1);
        auto [score2, team2] = run_greedy(2);

        long long best_comp_score = score1;
        vector<uint8_t> best_comp_team = team1;
        if (score2 > score1) {
            best_comp_score = score2;
            best_comp_team = team2;
        }

        // Step B: Component-Level Simulated Annealing (1-Flip)
        vector<uint8_t> current_team = best_comp_team;
        long long current_score = best_comp_score;
        vector<int> conflicts_count(N, 0);
        
        for (int u : comp) {
            if (current_team[u]) {
                for (int v : clean_adj[u]) {
                    conflicts_count[v]++;
                }
            }
        }

        uniform_int_distribution<int> distNode(0, comp.size() - 1);
        
        double T0 = 0;
        for (int u : comp) T0 += weight[u];
        T0 = (T0 / comp.size()) * 0.5; 
        if (T0 < 1.0) T0 = 1.0;
        double Tf = 1e-3;
        double T = T0;

        double sa_start = get_elapsed_time();
        int iter = 0;

        while (true) {
            if ((iter & 1023) == 0) {
                double elapsed = get_elapsed_time() - sa_start;
                if (elapsed > comp_time_limit) break;
                if (comp_time_limit > 0.001) {
                    T = T0 * pow(Tf / T0, elapsed / comp_time_limit);
                } else {
                    break;
                }
            }
            iter++;

            int u = comp[distNode(rng)];

            long long delta = 0;
            vector<int> to_remove;
            
            if (!current_team[u]) {
                delta = weight[u];
                for (int v : clean_adj[u]) {
                    if (current_team[v]) {
                        delta -= weight[v];
                        to_remove.push_back(v);
                    }
                }
            } else {
                delta = -weight[u];
                to_remove.push_back(u);
            }

            if (delta > 0 || exp((double)delta / T) > dist01(rng)) {
                if (current_team[u]) {
                    current_team[u] = 0;
                    for (int v : clean_adj[u]) conflicts_count[v]--;
                    current_score += delta;
                }

                else {
                    for (int w : to_remove) {
                        current_team[w] = 0;
                        for (int v : clean_adj[w]) conflicts_count[v]--;
                    }
                    current_team[u] = 1;
                    for (int v : clean_adj[u]) conflicts_count[v]++;
                    current_score += delta;
                }

                if (current_score > best_comp_score) {
                    best_comp_score = current_score;
                    best_comp_team = current_team;
                }
            }
        }

        // Commit component results
        for (int u : comp) {
            if (best_comp_team[u]) {
                final_team[u] = 1;
            }
        }
        final_score += best_comp_score;
    }


    // FINAL OUTPUT

    cout << final_score << "\n";
    bool first = true;
    for (int i = 0; i < N; ++i) {
        if (final_team[i]) {
            if (!first) cout << " ";
            cout << (i + 1);
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