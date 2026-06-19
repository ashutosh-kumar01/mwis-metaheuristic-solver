# MWIS Algorithm Final Analysis & Rankings

Based on the evaluation reports from both the sequential test suite (`evaluation_report3.txt`) and the parallel test suite (`evaluation_report_parallel.txt`), here is the definitive ranking and analysis of the MWIS solvers.

## 🏆 Final Global Rankings

### 1. The Undisputed Champion: `mwis_tabu_parallel.cpp` (Ultimate)
* **Performance:** 1st Place.
* **Why it wins:** The "Ultimate" algorithm combines the Multi-Start Tabu-Annealing engine with OpenMP multi-threading. By distributing the connected components across all available CPU cores, it evaluates vastly more states per second than the sequential version. It achieved the absolute highest global maximums on the largest sparse graphs (Input 16 and Input 18) and hit the maximum score tied-for-first on 22 out of 23 test cases.

### 2. The Sequential King: `mwis_enhanced_sa.cpp` 
* **Performance:** 2nd Place (1st Place among sequential algorithms).
* **Why it wins:** Simulated Annealing with a dynamic Tabu memory lock proved superior to Iterated Local Search (ILS). The exponential cooling curve allows for aggressive exploration early on and incredibly precise fine-tuning at the end. Sequentially, it beat `mwis_ils_grasp` on Inputs 17 and 18. In the parallel tests, its parallel counterpart achieved a global maximum on Input 17.

### 3. The Strong Contender: `mwis_ils_grasp.cpp`
* **Performance:** 3rd Place.
* **Why it falls short:** ILS-GRASP is highly competitive and scored a surprise sequential victory on Input 16. However, its strategy of dropping exactly 5% of the team and doing greedy repairs gets stuck in local optimums slightly more often than the thermal decay of Simulated Annealing. In the parallel arena, it failed to secure any absolute global maximums on the massive graphs.

### 4. The Baseline: `hackathon_peel2.exe` / `hackathon_peel.exe`
* **Performance:** 4th Place.
* **Why it falls short:** These purely structural algorithms are fast but easily trapped. They completely failed the trap graph (`input_legacy_5`), scoring 1,000,000 against a target of 1,099,989. While `peel2`'s Degree-2 folding is vastly superior to `peel`'s Degree-1 domination, structural math alone cannot solve NP-Hard dense graphs without a meta-heuristic engine backing it up.

---

## 🧠 Key Technical Insights

### 1. The Magic of Deep Kernelization
If you look at the time logs for graphs like `input_04_star_n400`, `input_11_matching_n200`, `input_12`, `input_13`, `input_14`, and `input_legacy_2`, the algorithms finish in **0.03 to 0.06 seconds**. 
**Insight:** The Deep Kernelization pre-processing step is a massive success. By mathematically collapsing paths, trees, stars, and low-degree nodes, the graph is completely solved in $O(V+E)$ time before the Simulated Annealing engine even turns on.

### 2. SA vs ILS: The Fine-Tuning Phase
The data shows that on extremely large sparse graphs (N=100,000 to N=200,000), `Enhanced_SA` consistently edges out `ILS_GRASP`. 
**Insight:** ILS forces a violent 5% drop (thousands of nodes) upon hitting a local optimum. Simulated Annealing, as it approaches Temperature = 0, performs highly surgical 1-node or 2-node swaps. This surgical fine-tuning at low temperatures allows SA to squeeze out those final few thousand points that ILS violently skips over.

### 3. The Power of "Component Splitting"
By breaking the graph into disconnected components using BFS and solving them individually, the time complexity is drastically reduced. We can see this in the Parallel report: an algorithm can instantly solve 50 tiny components on one thread, dedicating its remaining 290 seconds entirely to the 1 massive, complex component on another thread.