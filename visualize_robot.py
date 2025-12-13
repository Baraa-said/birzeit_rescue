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

def main():
    print("=== 3D Rescue Robot Evolution Visualizer ===\n")
    
    # Find all robot_data_*.txt files
    data_files = sorted(glob.glob('robot_data_*.txt'))
    
    if not data_files:
        print("❌ No robot_data_*.txt files found in current directory!")
        print("Please run the C program first to generate data files.")
        return
    
    print(f"✅ Found {len(data_files)} data files:")
    for f in data_files:
        print(f"   • {f}")
    
    visualizer = RescueVisualizer()
    
    print("\nChoose visualization:")
    print("1. Evolution grid (all snapshots side-by-side)")
    print("2. Fitness progression chart")
    print("3. Both")
    print("4. Individual snapshot")
    
    choice = input("\nEnter choice (1-4): ").strip()
    
    if choice == '1':
        visualizer.visualize_evolution(data_files)
    elif choice == '2':
        visualizer.create_comparison_chart(data_files)
    elif choice == '3':
        visualizer.visualize_evolution(data_files)
        visualizer.create_comparison_chart(data_files)
    elif choice == '4':
        print("\nAvailable files:")
        for i, f in enumerate(data_files):
            print(f"{i+1}. {f}")
        file_choice = int(input("Choose file number: ")) - 1
        if 0 <= file_choice < len(data_files):
            if visualizer.parse_data_file(data_files[file_choice]):
                fig = plt.figure(figsize=(12, 9))
                ax = fig.add_subplot(111, projection='3d')
                visualizer.visualize_single(ax, file_choice + 1, len(data_files))
                plt.show()
    else:
        print("Invalid choice")

if __name__ == "__main__":
    main()