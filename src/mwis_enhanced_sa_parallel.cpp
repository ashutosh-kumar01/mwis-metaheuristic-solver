// O3 optimiation level 3  aggresive optimization
/*
remove unnecessary variables
simplify expressions
faster loops
reduce memory access
*/

//unroll-loops Duplicate loop body to reduce loop overhead.
#pragma GCC optimize("O3,unroll-loops") 
#pragma GCC target("avx2,bmi,bmi2,lzcnt,popcnt")

#include <iostream> //printing
#include <vector>
#include <numeric> 
#include <algorithm> //sorting
#include <chrono> //high-precision clocks
#include <random>
#include <cmath> //math functions 
#include <queue>
#include <fstream> //File input/output
#include <string>
#include <omp.h> // Include OpenMP for Multithreading

using namespace std;

const double TOTAL_TIME_LIMIT = 295.0; // 4min 55 sec

//chrono::steady_clock::now() gives curr time in sec
auto global_start_time = chrono::steady_clock::now();  // holds exact start time of the program


// tells how much time elapsed from start of program

/* inline tells the compiler:

"Instead of doing a normal function call,
directly place the function code where it is used."

*/
inline double get_elapsed_time() {
    //sub gives duration obj .count() extract the number 
    return chrono::duration<double>(chrono::steady_clock::now() - global_start_time).count();
}

int main(int argc, char* argv[]) {

    /*
    C and C++ use different buffers
        they flush independently

        C++ was designed to work together with old C input/output

        C and C++ streams tied/synchronized  synchronization makes input/output slower
        so

    */
    ios_base::sync_with_stdio(false); //disables synchronization


    /*
    cin and cout tied  compiler automatically does cout.flush() before cin

    cout << "Enter number: ";
    cin >> x;
     Without flushing:"Enter number:" may stay hidden in buffer user sees nothing
     Automatic flush ensures prompt appears before input.
     flushing every time is expensive.
    
    */
    cin.tie(NULL);

    ifstream file_in;// input file stream
    ofstream file_out; //output file stram
    streambuf* cinbuf = cin.rdbuf(); // cin.rdbuf() returns pointer to cin's current buffer(keyboard)
    streambuf* coutbuf = cout.rdbuf();// cout.rdbuf() returns pointer to cout's current buffer(console screen)

    if (argc > 1) {
        string in_filename = argv[1]; //argv[1] is input file name
        file_in.open(in_filename);
        if (!file_in) {
            cerr << "Error: Could not open input file " << in_filename << "\n";
            return 1;
        }

        //Whenever the program asks for cin later, it will actually read from the file
        cin.rdbuf(file_in.rdbuf()); // file_in.rdbuf() give buffer of the file

        string out_filename = in_filename;
        size_t slash_pos = out_filename.find_last_of("/\\"); // finds last  pos of / or '\'
        if (slash_pos != string::npos) {
            out_filename = out_filename.substr(slash_pos + 1); // extract input file name
        }
        
        size_t pos = out_filename.find("input");
        if (pos != string::npos) {
            out_filename.replace(pos, 5, "output"); // replace 5 char input with output
        } 
        
        else {
            out_filename += ".out";
        }
        
        file_out.open(out_filename);
        if (!file_out) {
            cerr << "Error: Could not open output file " << out_filename << "\n";
            return 1;
        }

        // Whenever the program asks for cout later, it will actually write in the file
        cout.rdbuf(file_out.rdbuf());
    }

    int N; // no. of nodes(coders)
    long long M; //  no of edges(conflicts)
    if (!(cin >> N >> M)) return 0;

    vector<long long> weight(N);
    for (int i = 0; i < N; ++i) cin >> weight[i];

    //Adj list
    vector<vector<int>> adj(N);
    for (long long i = 0; i < M; ++i) {
        int u, v;
        cin >> u >> v;
        u--; v--; 
        adj[u].push_back(v);
        adj[v].push_back(u);
    }


    if (file_in.is_open()) file_in.close(); //closing the file

    for (int i = 0; i < N; ++i) adj[i].shrink_to_fit(); //C++ to release any excess unused RAM from the vectors to save memory


    //tracks if a node is still in the graph (1 = yes, 0 = deleted)
    //uint8_t (an 8-bit integer) instead of bool because it processes much faster in CPU caches
    vector<uint8_t> active_node(N, 1);

    vector<uint8_t> final_team(N, 0);
    long long final_score = 0;

    // 1. DEEP KERNELIZATION

    //push every node that has 2 or fewer edges into the queue, because they are candidates for our mathematical peeling rules
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
            final_team[u] = 1;
            final_score += weight[u];
            active_node[u] = 0;
        } 
        
        else if (d == 1) {
            int v = neighbors[0];
            if (weight[u] >= weight[v]) {
                final_team[u] = 1;
                final_score += weight[u];
                active_node[u] = 0;
                active_node[v] = 0;
                for (int w : adj[v]) if (active_node[w]) Q.push(w);
            }
        } 
        else if (d == 2) {
            int v = neighbors[0];
            int w_node = neighbors[1];
            if (weight[u] >= weight[v] + weight[w_node]) {
                final_team[u] = 1;
                final_score += weight[u];
                active_node[u] = 0;
                active_node[v] = 0;
                active_node[w_node] = 0;
                
                // as node  x degree might become <=2
                for (int x : adj[v]) if (active_node[x]) Q.push(x); 
                for (int x : adj[w_node]) if (active_node[x]) Q.push(x);
            }
        }
    }

    // 2. COMPONENT DECOMPOSITION

    vector<vector<int>> clean_adj(N);
    for (int i = 0; i < N; ++i) {
        if (active_node[i]) {
            for (int v : adj[i]) {
                if (active_node[v]) clean_adj[i].push_back(v);
            }
        }
    }

    vector<vector<int>> components;
    vector<uint8_t> visited(N, 0);
    int total_active = 0;

    for (int i = 0; i < N; ++i) {
        //bfs to find components
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

    
    // PARALLEL EXECUTION SETUP

    // Sort components by size descending so large components start first,
    // balancing the thread workload perfectly.
    sort(components.begin(), components.end(), [](const vector<int>& a, const vector<int>& b) {
        return a.size() > b.size();
    });

    double remaining_time_at_start = max(0.0, TOTAL_TIME_LIMIT - get_elapsed_time());

    // 3. COMPONENT SOLVER (Now with OpenMP Multithreading)
    // We use OpenMP to dynamically distribute components across available CPU cores.
    #pragma omp parallel for schedule(dynamic) reduction(+:final_score)
    for (size_t c_idx = 0; c_idx < components.size(); ++c_idx) {
        const auto& comp = components[c_idx];
        if (comp.empty()) continue;

        if (comp.size() == 1) {
            #pragma omp critical
            {
                final_team[comp[0]] = 1;
            }
            final_score += weight[comp[0]];
            continue;
        }

        // Each thread calculates its own time limit based on component size ratio
        double comp_time_limit = remaining_time_at_start * ((double)comp.size() / total_active);
        double comp_start_time = get_elapsed_time();

        //random number generator
        /*
        1337 is seed it decides the starting state of the number generation same seed will give
        same random no. seq every time you run and changing seed give diff ranmdom no. seq
        Each thread MUST have its own random generator to avoid data races.
        */
        mt19937 rng(1337 + c_idx); // Added c_idx to seed to make each thread unique

        //scales down  the  values  given to it between 0 and 1
        uniform_real_distribution<double> dist01(0.0, 1.0);

        //  Dual Baselines

        //small function directly inside another function (lambda func)
        /*
        Why lambda is used here?
        Because this function: needs access to many local variables
        */

        //[&] capture all surrounding variables(all variables declared above this local or global) by reference(do not copy)
        auto run_forward_greedy = [&]() {
            vector<uint8_t> in_team(N, 0);
            long long score = 0;
            vector<int> order = comp;

            //sort in descending order
            sort(order.begin(), order.end(), [&](int a, int b) {
                double r1 = (double)weight[a] / (clean_adj[a].size() + 1.0);
                double r2 = (double)weight[b] / (clean_adj[b].size() + 1.0);
                if (r1 != r2) return r1 > r2;
                return weight[a] > weight[b];
            }); 

            // checks if any neighbour is in team or not if yes then cannot add if no add
            for (int u : order) {
                bool can_add = true;
                for (int v : clean_adj[u]) if (in_team[v]) { can_add = false; break; }
                if (can_add) { in_team[u] = 1; score += weight[u]; }
            }
            return make_pair(score, in_team);
        };


        
        auto run_reverse_greedy = [&]() {

            vector<uint8_t> in_team(N, 0);
            for (int u : comp) in_team[u] = 1; // every node in comp is in team
            long long score = 0;
            vector<int> order = comp;

            //sort in increasing order
            sort(order.begin(), order.end(), [&](int a, int b) {
                double r1 = (double)weight[a] / (clean_adj[a].size() + 1.0);
                double r2 = (double)weight[b] / (clean_adj[b].size() + 1.0);
                if (r1 != r2) return r1 < r2; 
                return weight[a] < weight[b];
            });

            for(int u : comp) score += weight[u];
            
            //Remove conflicting bad nodes
            for (int u : order) {
                if (!in_team[u]) continue;
                bool conflict = false;
                for (int v : clean_adj[u]) if (in_team[v]) { conflict = true; break; }
                if (conflict) { in_team[u] = 0; score -= weight[u]; }
            }

            //checking if we can add some nodes back doing small repairs
            for (int u : comp) {
                if (!in_team[u]) {
                    bool can_add = true;
                    for (int v : clean_adj[u]) if (in_team[v]) { can_add = false; break; }
                    if (can_add) { in_team[u] = 1; score += weight[u]; }
                }
            }
            return make_pair(score, in_team);
        };

        auto [scoreF, teamF] = run_forward_greedy();
        auto [scoreR, teamR] = run_reverse_greedy();
        
        //taking the best score
        long long best_comp_score = scoreF;
        vector<uint8_t> best_comp_team = teamF;
        if (scoreR > scoreF) {
            best_comp_score = scoreR;
            best_comp_team = teamR;
        }

        
        auto run_random_greedy = [&]() {
            vector<uint8_t> in_team(N, 0);
            long long score = 0;
            vector<int> order = comp;
            //± 20% in value
            //generates random number b/w (0.8, 1.2) with uniform proabibilty
            uniform_real_distribution<double> d(0.8, 1.2);
            vector<double> rand_score(N, 0.0);
            for (int u : comp) {
                rand_score[u] = ((double)weight[u] / (clean_adj[u].size() + 1.0)) * d(rng);
            }

            //descending order
            sort(order.begin(), order.end(), [&](int a, int b) {
                if (rand_score[a] != rand_score[b]) return rand_score[a] > rand_score[b];
                return weight[a] > weight[b];
            });

            for (int u : order) {
                bool can_add = true;
                for (int v : clean_adj[u]) if (in_team[v]) { can_add = false; break; }
                if (can_add) { in_team[u] = 1; score += weight[u]; }
            }
            return make_pair(score, in_team);
        };


        // Multi-Start Enhanced SA 
        // gives random node  of component
        uniform_int_distribution<int> distNode(0, comp.size() - 1);
        
        while ((get_elapsed_time() - comp_start_time) < comp_time_limit) {
            auto [curr_score, curr_team] = run_random_greedy();
            if (curr_score > best_comp_score) { best_comp_score = curr_score; best_comp_team = curr_team; }
            

            vector<int> conf_count(N, 0); // number of neighours that are in team
            for (int u : comp) {
                if (curr_team[u]) {
                    for (int v : clean_adj[u]) conf_count[v]++;
                }
            }
            
            double T0 = 0; 
            for (int u : comp) T0 += weight[u]; 
            T0 = (T0 / comp.size()) * 0.5;  //half of the avg weight of a node
            if (T0 < 1.0) T0 = 1.0;
            double Tf = 1e-3; // final temp
            double T = T0; //curr
            
            double epoch_start = get_elapsed_time();
            double epoch_limit = min(3.0, comp_time_limit - (epoch_start - comp_start_time));
            if (epoch_limit <= 0.01) break;
            
            int iter = 0;
            while (true) {
                if ((iter & 511) == 0) {//(iter % 512 ==0)  (511)10 =(111111111)2   9 1's
                    //pow and get_elapsed_time() are  expensive operations
                    double el = get_elapsed_time() - epoch_start;
                    if (el > epoch_limit) break;

                    //exponential decay
                    /*
                    at strt el=0 so T=T0 at end el=epoch_limit  T=Tf and in between decays exponentionally

                    If temperature stays huge too long
                    Then algorithm: wanders aimlessly keeps destroying good structures
                    exploration becomes chaotic .Fast early cooling reduces chaos

                    Slow late cooling allows refinement

                    At low temperatures:
                    only small bad moves accepted occasionally
                    solution changes become careful
                    This helps: rearrange local structure escape tiny local traps polish solution
                    */
                    T = T0 * pow(Tf / T0, el / epoch_limit); 
                }
                iter++;
                
                int u = comp[distNode(rng)];
                long long prev_score = curr_score;
                vector<int> rollback_removed;
                vector<int> rollback_added;

                if (curr_team[u]) {
                    // Try Drop & Repair
                    curr_team[u] = 0; //remove from team
                    rollback_removed.push_back(u);
                    curr_score -= weight[u];
                    for (int v : clean_adj[u]) conf_count[v]--; // reduce no  of neighbours in team as u is removed


                    for (int v : clean_adj[u]) {
                        if (!curr_team[v] && conf_count[v] == 0) {
                            curr_team[v] = 1;
                            rollback_added.push_back(v);
                            curr_score += weight[v];
                            for (int w : clean_adj[v]) conf_count[w]++;
                        }
                    }
                } 
                
                else {
                    // Try Force Add & Repair
                    for (int v : clean_adj[u]) {
                        if (curr_team[v]) {
                            curr_team[v] = 0;
                            rollback_removed.push_back(v);
                            curr_score -= weight[v];
                            for (int w : clean_adj[v]) conf_count[w]--;
                        }
                    }
                    curr_team[u] = 1;
                    rollback_added.push_back(u);
                    curr_score += weight[u];
                    for (int w : clean_adj[u]) conf_count[w]++;

                    for (int v : rollback_removed) {
                        for (int n : clean_adj[v]) {
                            if (!curr_team[n] && conf_count[n] == 0) {
                                curr_team[n] = 1;
                                rollback_added.push_back(n);
                                curr_score += weight[n];
                                for (int w : clean_adj[n]) conf_count[w]++;
                            }
                        }
                    }
                }

                long long delta = curr_score - prev_score;
                if (delta > 0 || exp((double)delta / T) > dist01(rng)) {
                    // accept Move
                    if (curr_score > best_comp_score) { 
                        best_comp_score = curr_score; 
                        best_comp_team = curr_team; 
                    }
                } 
                
                else {
                    // Rollback Move (Reverse Order)
                    for (int i = (int)rollback_added.size() - 1; i >= 0; i--) {
                        int v = rollback_added[i];
                        curr_team[v] = 0;
                        curr_score -= weight[v];
                        for (int w : clean_adj[v]) conf_count[w]--;
                    }
                    for (int i = (int)rollback_removed.size() - 1; i >= 0; i--) {
                        int v = rollback_removed[i];
                        curr_team[v] = 1;
                        curr_score += weight[v];
                        for (int w : clean_adj[v]) conf_count[w]++;
                    }
                }
            }
        }

        // Commit best found in this component safely using critical section
        #pragma omp critical
        {
            for (int u : comp) {
                if (best_comp_team[u]) final_team[u] = 1;
            }
        }
        final_score += best_comp_score;
    }

    // 4. FINAL OUTPUT
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
