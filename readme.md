Genetic Algorithm Rescue Robot Path Optimizer
Project Overview
This project implements a genetic algorithm-based multi-robot path optimization system for rescue operations in collapsed buildings. The system uses Linux multi-processing and IPC (Inter-Process Communication) techniques to efficiently evolve optimal paths for rescue robots.

Project Structure
project/
├── main.c            # Main program and evolution loop
├── config.c          # Configuration file parsing
├── genetic.c         # Genetic algorithm operations
├── pool.c            # Process pool and IPC management
├── types.h           # Data structures and type definitions
├── config.h          # Configuration interface
├── genetic.h         # Genetic algorithm interface
├── pool.h            # Process pool interface
├── config.txt        # Configuration parameters
├── Makefile          # Build automation
└── README.md         # This documentation
Modular Architecture
main.c - Main Program
Entry point and evolution orchestration
Population initialization
Generation loop control
Results display
genetic.c - Genetic Algorithm Module
Path generation (A*-inspired)
Fitness evaluation
Tournament selection
Crossover operator
Mutation operator
Grid utilities
pool.c - Process Pool Module
Shared memory initialization
Semaphore operations
Worker process creation
Process synchronization
Resource cleanup
config.c - Configuration Module
Configuration file parsing
Default parameter values
Configuration display
Header Files
types.h: Data structures (Config, Path, Coord, SharedData)
config.h: Configuration function declarations
genetic.h: Genetic algorithm function declarations
pool.h: Process pool function declarations
Features
3D Grid Environment: Models collapsed buildings with obstacles and survivor locations
Genetic Algorithm: Evolves optimal paths through selection, crossover, and mutation
Multi-Processing: Uses process pools with shared memory and semaphores for parallel computation
Configurable: All parameters adjustable via configuration file
A-Inspired Path Generation*: Initial population biased towards survivor locations
Collision Detection: Accounts for proximity to obstacles in fitness calculation
System Requirements
Linux operating system
GCC compiler
Standard C libraries
IPC support (System V shared memory and semaphores)
Compilation
Using Make (Recommended)
bash
make
Manual Compilation
bash
gcc -c config.c -o config.o -Wall -g
gcc -c genetic.c -o genetic.o -Wall -g
gcc -c pool.c -o pool.o -Wall -g
gcc -c main.c -o main.o -Wall -g
gcc main.o config.o genetic.o pool.o -o rescue_robot -lm -pthread -Wall -g
For Debugging
bash
make debug
Clean Build Files
bash
make clean
Usage
Run with Configuration File
bash
./rescue_robot config.txt
Run with Default Parameters
bash
./rescue_robot
Using Makefile Shortcuts
bash
make run          # Run with config.txt
make run-default  # Run with defaults
Configuration Parameters
Edit config.txt to customize the simulation:

Grid Dimensions
grid_x, grid_y, grid_z: 3D grid size (default: 20x20x10)
Population Parameters
population_size: Number of paths in each generation (default: 50)
num_generations: Maximum evolution cycles (default: 100)
max_path_length: Maximum coordinates in a path (default: 50)
stagnation_limit: Generations without improvement before stopping (default: 20)
System Resources
num_processes: Worker processes for parallel computation (default: 4)
Environment
num_survivors: Trapped people to rescue (default: 10)
num_obstacles: Debris cells blocking paths (default: 100)
Fitness Weights
w1: Survivors reached weight (default: 10.0)
w2: Coverage area weight (default: 2.0)
w3: Path length penalty (default: 0.5)
w4: Risk/collision penalty (default: 5.0)
Genetic Algorithm
elitism_percent: Fraction of best solutions preserved (default: 0.1)
mutation_rate: Probability of path mutation (default: 0.2)
crossover_rate: Probability of parent crossover (default: 0.8)
tournament_size: Candidates in tournament selection (default: 3)
Algorithm Details
Chromosome Representation
Each path is encoded as a sequence of 3D coordinates:

c
Path = [(x1,y1,z1), (x2,y2,z2), ..., (xn,yn,zn)]
Fitness Function
fitness = w1 × survivors + w2 × coverage - w3 × length - w4 × risk
Where:

survivors: Number of unique survivors reached
coverage: Unique cells visited
length: Total path steps
risk: Proximity to obstacles
Genetic Operators
Selection: Tournament selection picks best paths from random subsets
Crossover: Combines parent paths at random split point
Mutation: Randomly modifies path coordinates
Elitism: Top 10% preserved unchanged
IPC Mechanisms
Shared Memory: Population, grid, and survivor data shared across processes
Semaphores: Synchronize access to shared resources
Process Pool: Worker processes created once and reused
Output
The program displays:

Configuration summary
Progress every 10 generations
Final best solution with:
Fitness score
Survivors reached
Coverage area
Path coordinates (first 20)
Top 5 solutions
Example output:

=== Genetic Algorithm Rescue Robot Path Optimizer ===

Configuration:
  Grid: 20x20x10
  Population: 50
  Generations: 100
  Processes: 4
  Survivors: 10
  Obstacles: 100

Running genetic algorithm...

Gen   0: Best Fitness = 45.23, Survivors = 5, Coverage = 42
Gen  10: Best Fitness = 67.89, Survivors = 7, Coverage = 58
Gen  20: Best Fitness = 78.34, Survivors = 8, Coverage = 65
...

=== Final Results ===
Best Fitness: 89.45
Survivors Reached: 8 / 10
Coverage: 67 cells
Path Length: 48

Optimized Path (first 20 coordinates):
  [ 5, 3, 2]  [ 6, 3, 2]  [ 7, 3, 2]  [ 8, 3, 3]  [ 9, 4, 3]
  ...

Top 5 Solutions:
  #1: Fitness=89.45, Survivors=8, Coverage=67, Length=48
  #2: Fitness=85.12, Survivors=8, Coverage=63, Length=50
  ...
Troubleshooting
Compilation Errors
Ensure you have gcc and make installed
Check that you have math library support (-lm)
Runtime Errors
"shmget failed": Insufficient shared memory. Try reducing population_size
"semget failed": Too many semaphores. Check system limits with ipcs
Slow performance: Reduce num_processes or population_size
Cleaning Shared Resources
If the program crashes, clean up IPC resources:

bash
ipcs -m | grep $(whoami) | awk '{print $2}' | xargs -I{} ipcrm -m {}
ipcs -s | grep $(whoami) | awk '{print $2}' | xargs -I{} ipcrm -s {}
Testing Scenarios
Quick Test (Fast)
grid_x = 10
grid_y = 10
grid_z = 5
population_size = 20
num_generations = 50
Standard Test (Medium)
Use default values in config.txt

Large Scale Test (Slow)
grid_x = 30
grid_y = 30
grid_z = 15
population_size = 100
num_generations = 200
Debugging
Compile with debug symbols:

bash
make debug
gdb ./rescue_robot
Common GDB commands:

(gdb) run config.txt
(gdb) break calculate_fitness
(gdb) print path->fitness
(gdb) backtrace
(gdb) continue
Performance Optimization Tips
Match processes to CPU cores: Set num_processes to your CPU core count
Tune weights: Adjust w1-w4 for problem-specific priorities
Reduce grid size: Smaller grids converge faster
Enable stagnation: Stop early if no improvement
Adjust population: Balance between diversity and speed
Code Organization Benefits
Modularity
Separation of Concerns: Each module handles specific functionality
Maintainability: Easy to modify individual components
Testability: Can test modules independently
Reusability
Generic Genetic Algorithm: genetic.c can be adapted for other problems
Flexible Process Pool: pool.c can handle various parallel tasks
Configurable System: config.c makes parameter tuning easy
Scalability
Add new genetic operators: Extend genetic.c
Change IPC mechanism: Modify pool.c
Add features: Main logic isolated in main.c
Future Enhancements
 Dynamic obstacle updates
 Multi-robot collision avoidance
 GPU acceleration
 Visualization output (3D rendering)
 Real-time path updates
 Machine learning-based initialization
 Alternative IPC mechanisms (message queues, pipes)
 Distributed computing support
Credits
Developed for ENCS4330 - Real-Time Applications & Embedded Systems
Birzeit University - Instructor: Dr. Hanna Bullata

License
Academic project - For educational purposes only

