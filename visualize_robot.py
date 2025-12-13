#!/usr/bin/env python3
"""
3D Rescue Robot Path Visualizer - Multi-Snapshot Version
Visualizes multiple genetic algorithm snapshots to show evolution progress
"""

import matplotlib.pyplot as plt
from mpl_toolkits.mplot3d import Axes3D
import numpy as np
import os
import glob

class RescueVisualizer:
    def __init__(self):
        self.grid_x = 0
        self.grid_y = 0
        self.grid_z = 0
        self.survivors = []
        self.obstacles = []
        self.path = []
        self.fitness = 0.0
        
    def parse_data_file(self, filename):
        """Parse robot data file"""
        try:
            with open(filename, 'r') as f:
                lines = f.readlines()
            
            self.survivors = []
            self.obstacles = []
            self.path = []
            
            i = 0
            while i < len(lines):
                line = lines[i].strip()
                
                if line.startswith('GRID:'):
                    parts = line.split()
                    self.grid_x = int(parts[1])
                    self.grid_y = int(parts[2])
                    self.grid_z = int(parts[3])
                    
                elif line.startswith('FITNESS:'):
                    self.fitness = float(line.split()[1])
                    
                elif line.startswith('SURVIVORS:'):
                    num = int(line.split()[1])
                    for j in range(num):
                        i += 1
                        coords = list(map(int, lines[i].strip().split()))
                        self.survivors.append(coords)
                    
                elif line.startswith('OBSTACLES:'):
                    num = int(line.split()[1])
                    for j in range(num):
                        i += 1
                        coords = list(map(int, lines[i].strip().split()))
                        self.obstacles.append(coords)
                    
                elif line.startswith('PATH:'):
                    num = int(line.split()[1])
                    for j in range(num):
                        i += 1
                        coords = list(map(int, lines[i].strip().split()))
                        self.path.append(coords)
                
                i += 1
                
            return True
            
        except Exception as e:
            print(f"Error parsing {filename}: {e}")
            return False
    
    def visualize_single(self, ax, snapshot_num, total_snapshots):
        """Create 3D visualization on given axis"""
        
        # Convert lists to numpy arrays
        if self.survivors:
            survivors_arr = np.array(self.survivors)
            ax.scatter(survivors_arr[:, 0], survivors_arr[:, 1], survivors_arr[:, 2],
                      c='green', marker='*', s=200, label='Survivors', 
                      edgecolors='darkgreen', linewidths=2, alpha=0.9)
        
        if self.obstacles:
            obstacles_arr = np.array(self.obstacles)
            ax.scatter(obstacles_arr[:, 0], obstacles_arr[:, 1], obstacles_arr[:, 2],
                      c='red', marker='s', s=80, label='Obstacles', 
                      alpha=0.4, edgecolors='darkred')
        
        if self.path:
            path_arr = np.array(self.path)
            
            # Draw path line
            ax.plot(path_arr[:, 0], path_arr[:, 1], path_arr[:, 2],
                   c='blue', linewidth=2, alpha=0.7, label='Robot Path')
            
            # Mark start point
            ax.scatter(path_arr[0, 0], path_arr[0, 1], path_arr[0, 2],
                      c='cyan', marker='o', s=150, label='Start', 
                      edgecolors='blue', linewidths=2)
            
            # Mark end point
            ax.scatter(path_arr[-1, 0], path_arr[-1, 1], path_arr[-1, 2],
                      c='purple', marker='D', s=150, label='End',
                      edgecolors='darkviolet', linewidths=2)
        
        # Grid boundaries
        ax.set_xlim([0, self.grid_x])
        ax.set_ylim([0, self.grid_y])
        ax.set_zlim([0, self.grid_z])
        
        # Labels
        ax.set_xlabel('X', fontsize=9)
        ax.set_ylabel('Y', fontsize=9)
        ax.set_zlabel('Z', fontsize=9)
        
        # Title
        title = f'Snapshot {snapshot_num}/{total_snapshots}\n'
        title += f'Fitness: {self.fitness:.2f} | Path: {len(self.path)} steps'
        ax.set_title(title, fontsize=10, fontweight='bold')
        
        # Legend
        ax.legend(loc='upper left', fontsize=7)
        
        # Grid
        ax.grid(True, alpha=0.3)
        
        # Viewing angle
        ax.view_init(elev=20, azim=45)
    
    def visualize_evolution(self, data_files):
        """Create grid of plots showing evolution"""
        num_files = len(data_files)
        
        # Create figure with subplots
        fig = plt.figure(figsize=(18, 10))
        
        # Arrange in 2 rows if we have 4+ files, otherwise 1 row
        if num_files >= 4:
            rows = 2
            cols = (num_files + 1) // 2
        else:
            rows = 1
            cols = num_files
        
        for idx, filename in enumerate(data_files):
            if not self.parse_data_file(filename):
                continue
            
            ax = fig.add_subplot(rows, cols, idx + 1, projection='3d')
            self.visualize_single(ax, idx + 1, num_files)
        
        plt.suptitle('Genetic Algorithm Evolution - Path Optimization Progress', 
                    fontsize=14, fontweight='bold', y=0.98)
        plt.tight_layout(rect=[0, 0, 1, 0.96])
        plt.show()
    
    def create_comparison_chart(self, data_files):
        """Create fitness progression chart"""
        fitness_values = []
        snapshot_nums = []
        
        for idx, filename in enumerate(data_files):
            if self.parse_data_file(filename):
                fitness_values.append(self.fitness)
                snapshot_nums.append(idx + 1)
        
        if not fitness_values:
            print("No data to plot")
            return
        
        fig, ax = plt.subplots(figsize=(10, 6))
        
        ax.plot(snapshot_nums, fitness_values, 'bo-', linewidth=2, markersize=10)
        ax.set_xlabel('Snapshot Number', fontsize=12, fontweight='bold')
        ax.set_ylabel('Best Fitness', fontsize=12, fontweight='bold')
        ax.set_title('Genetic Algorithm Fitness Evolution', fontsize=14, fontweight='bold')
        ax.grid(True, alpha=0.3)
        
        # Add value labels
        for i, (x, y) in enumerate(zip(snapshot_nums, fitness_values)):
            ax.annotate(f'{y:.2f}', (x, y), textcoords="offset points", 
                       xytext=(0,10), ha='center', fontsize=9)
        
        plt.tight_layout()
        plt.show()
    
    def visualize_workers(self, worker_files):
        """Compare all worker solutions side-by-side"""
        num_workers = len(worker_files)
        
        if num_workers == 0:
            print("No worker files to visualize")
            return
        
        # Create figure with subplots
        fig = plt.figure(figsize=(18, 10))
        
        cols = min(4, num_workers)
        rows = (num_workers + cols - 1) // cols
        
        for idx, filename in enumerate(worker_files):
            if not self.parse_data_file(filename):
                continue
            
            # Extract worker ID from filename
            worker_id = filename.split('_')[-1].split('.')[0]
            
            ax = fig.add_subplot(rows, cols, idx + 1, projection='3d')
            
            # Visualize this worker's path
            if self.survivors:
                survivors_arr = np.array(self.survivors)
                ax.scatter(survivors_arr[:, 0], survivors_arr[:, 1], survivors_arr[:, 2],
                          c='green', marker='*', s=200, label='Survivors', 
                          edgecolors='darkgreen', linewidths=2, alpha=0.9)
            
            if self.obstacles:
                obstacles_arr = np.array(self.obstacles)
                ax.scatter(obstacles_arr[:, 0], obstacles_arr[:, 1], obstacles_arr[:, 2],
                          c='red', marker='s', s=80, label='Obstacles', 
                          alpha=0.4, edgecolors='darkred')
            
            if self.path:
                path_arr = np.array(self.path)
                
                # Use different colors for each worker
                colors = ['blue', 'purple', 'orange', 'cyan', 'magenta', 'yellow']
                worker_color = colors[idx % len(colors)]
                
                ax.plot(path_arr[:, 0], path_arr[:, 1], path_arr[:, 2],
                       c=worker_color, linewidth=2, alpha=0.7, label=f'Worker {worker_id} Path')
                
                ax.scatter(path_arr[0, 0], path_arr[0, 1], path_arr[0, 2],
                          c='cyan', marker='o', s=150, label='Start', 
                          edgecolors='blue', linewidths=2)
                
                ax.scatter(path_arr[-1, 0], path_arr[-1, 1], path_arr[-1, 2],
                          c='red', marker='D', s=150, label='End',
                          edgecolors='darkred', linewidths=2)
            
            ax.set_xlim([0, self.grid_x])
            ax.set_ylim([0, self.grid_y])
            ax.set_zlim([0, self.grid_z])
            
            ax.set_xlabel('X', fontsize=9)
            ax.set_ylabel('Y', fontsize=9)
            ax.set_zlabel('Z', fontsize=9)
            
            title = f'Worker {worker_id}\n'
            title += f'Fitness: {self.fitness:.2f}\n'
            title += f'Survivors: {len([c for c in self.path if any((c[0]==s[0] and c[1]==s[1] and c[2]==s[2]) for s in self.survivors)])}'
            ax.set_title(title, fontsize=10, fontweight='bold')
            
            ax.legend(loc='upper left', fontsize=7)
            ax.grid(True, alpha=0.3)
            ax.view_init(elev=20, azim=45)
        
        plt.suptitle('Worker Solutions Comparison - Independent Paths', 
                    fontsize=14, fontweight='bold', y=0.98)
        plt.tight_layout(rect=[0, 0, 1, 0.96])
        plt.show()
    
    def create_worker_comparison(self, worker_files):
        """Create bar chart comparing worker performance"""
        worker_ids = []
        fitness_values = []
        survivor_counts = []
        
        for filename in worker_files:
            if self.parse_data_file(filename):
                worker_id = filename.split('_')[-1].split('.')[0]
                worker_ids.append(f'Worker {worker_id}')
                fitness_values.append(self.fitness)
                
                # Count survivors reached
                survivors_reached = 0
                for coord in self.path:
                    for survivor in self.survivors:
                        if coord[0] == survivor[0] and coord[1] == survivor[1] and coord[2] == survivor[2]:
                            survivors_reached += 1
                            break
                survivor_counts.append(survivors_reached)
        
        if not fitness_values:
            print("No data to plot")
            return
        
        fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(14, 6))
        
        # Fitness comparison
        colors = ['#1f77b4', '#ff7f0e', '#2ca02c', '#d62728', '#9467bd', '#8c564b']
        bars1 = ax1.bar(worker_ids, fitness_values, color=colors[:len(worker_ids)])
        ax1.set_ylabel('Fitness Score', fontsize=12, fontweight='bold')
        ax1.set_title('Worker Fitness Comparison', fontsize=14, fontweight='bold')
        ax1.grid(True, alpha=0.3, axis='y')
        
        for i, (bar, val) in enumerate(zip(bars1, fitness_values)):
            ax1.text(bar.get_x() + bar.get_width()/2, val, f'{val:.1f}',
                    ha='center', va='bottom', fontsize=10, fontweight='bold')
        
        # Survivors comparison
        bars2 = ax2.bar(worker_ids, survivor_counts, color=colors[:len(worker_ids)])
        ax2.set_ylabel('Survivors Reached', fontsize=12, fontweight='bold')
        ax2.set_title('Worker Survivor Coverage', fontsize=14, fontweight='bold')
        ax2.grid(True, alpha=0.3, axis='y')
        ax2.set_ylim([0, max(survivor_counts) + 1])
        
        for i, (bar, val) in enumerate(zip(bars2, survivor_counts)):
            ax2.text(bar.get_x() + bar.get_width()/2, val, str(val),
                    ha='center', va='bottom', fontsize=10, fontweight='bold')
        
        plt.suptitle('Independent Worker Performance Analysis', 
                    fontsize=14, fontweight='bold')
        plt.tight_layout()
        plt.show()

def main():
    print("=== 3D Rescue Robot Evolution Visualizer ===\n")
    
    # Find all robot_data_*.txt files
    global_files = sorted(glob.glob('robot_data_[0-9]*.txt'))
    worker_files = sorted(glob.glob('robot_data_worker_*.txt'))
    
    if not global_files and not worker_files:
        print("❌ No robot_data*.txt files found in current directory!")
        print("Please run the C program first to generate data files.")
        return
    
    print(f"✅ Found {len(global_files)} global snapshot files")
    print(f"✅ Found {len(worker_files)} worker path files")
    
    if global_files:
        print("\n📊 Global snapshots:")
        for f in global_files:
            print(f"   • {f}")
    
    if worker_files:
        print("\n🤖 Worker solutions:")
        for f in worker_files:
            print(f"   • {f}")
    
    visualizer = RescueVisualizer()
    
    print("\nChoose visualization:")
    print("1. Evolution grid (all snapshots side-by-side)")
    print("2. Fitness progression chart")
    print("3. Compare all workers (side-by-side paths)")
    print("4. Individual snapshot")
    print("5. Worker comparison chart")
    
    choice = input("\nEnter choice (1-5): ").strip()
    
    if choice == '1':
        if global_files:
            visualizer.visualize_evolution(global_files)
        else:
            print("No global snapshot files found!")
    elif choice == '2':
        if global_files:
            visualizer.create_comparison_chart(global_files)
        else:
            print("No global snapshot files found!")
    elif choice == '3':
        if worker_files:
            visualizer.visualize_workers(worker_files)
        else:
            print("No worker files found!")
    elif choice == '4':
        all_files = global_files + worker_files
        if all_files:
            print("\nAvailable files:")
            for i, f in enumerate(all_files):
                print(f"{i+1}. {f}")
            file_choice = int(input("Choose file number: ")) - 1
            if 0 <= file_choice < len(all_files):
                if visualizer.parse_data_file(all_files[file_choice]):
                    fig = plt.figure(figsize=(12, 9))
                    ax = fig.add_subplot(111, projection='3d')
                    visualizer.visualize_single(ax, file_choice + 1, len(all_files))
                    plt.show()
        else:
            print("No files found!")
    elif choice == '5':
        if worker_files:
            visualizer.create_worker_comparison(worker_files)
        else:
            print("No worker files found!")
    else:
        print("Invalid choice")

if __name__ == "__main__":
    main()