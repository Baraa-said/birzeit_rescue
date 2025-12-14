#!/usr/bin/env python3
import re, glob
import numpy as np
import matplotlib.pyplot as plt
from mpl_toolkits.mplot3d import Axes3D  # noqa: F401

def extract_num(fname, prefix):
    m = re.search(rf'{re.escape(prefix)}(\d+)\.txt$', fname)
    return int(m.group(1)) if m else -1

def numeric_sort(files, prefix):
    files = list(files)
    files.sort(key=lambda f: extract_num(f, prefix))
    return files

class RescueVisualizer:
    def __init__(self):
        self.grid_x=self.grid_y=self.grid_z=0
        self.gen=-1
        self.survivors=[]
        self.survivor_p=[]
        self.obstacles=[]
        self.path=[]
        self.fitness=0.0

    def parse_data_file(self, filename):
        try:
            with open(filename,'r') as f:
                lines=f.readlines()

            self.survivors=[]; self.survivor_p=[]
            self.obstacles=[]; self.path=[]
            self.gen=-1

            i=0
            while i<len(lines):
                line=lines[i].strip()
                if line.startswith("GRID:"):
                    _,x,y,z=line.split()
                    self.grid_x=int(x); self.grid_y=int(y); self.grid_z=int(z)
                elif line.startswith("GEN:"):
                    self.gen=int(line.split()[1])
                elif line.startswith("FITNESS:"):
                    self.fitness=float(line.split()[1])
                elif line.startswith("SURVIVORS:"):
                    n=int(line.split()[1])
                    for _ in range(n):
                        i+=1
                        parts=list(map(int,lines[i].strip().split()))
                        self.survivors.append(parts[:3])
                        self.survivor_p.append(parts[3] if len(parts)>=4 else 1)
                elif line.startswith("OBSTACLES:"):
                    n=int(line.split()[1])
                    for _ in range(n):
                        i+=1
                        self.obstacles.append(list(map(int,lines[i].strip().split())))
                elif line.startswith("PATH:"):
                    n=int(line.split()[1])
                    for _ in range(n):
                        i+=1
                        self.path.append(list(map(int,lines[i].strip().split())))
                i+=1
            return True
        except Exception as e:
            print(f"Error parsing {filename}: {e}")
            return False

    def visualize_single(self, ax, title):
        if self.survivors:
            S=np.array(self.survivors)
            ax.scatter(S[:,0],S[:,1],S[:,2], marker='*', s=200, label='Survivors')
            for (x,y,z),p in zip(self.survivors,self.survivor_p):
                ax.text(x,y,z+0.1,f"P{p}", fontsize=8)

        if self.obstacles:
            O=np.array(self.obstacles)
            ax.scatter(O[:,0],O[:,1],O[:,2], marker='s', s=70, alpha=0.35, label='Obstacles')

        if self.path:
            P=np.array(self.path)
            ax.plot(P[:,0],P[:,1],P[:,2], linewidth=2, alpha=0.8, label='Path')
            ax.scatter(P[0,0],P[0,1],P[0,2], s=140, label='Start')
            ax.scatter(P[-1,0],P[-1,1],P[-1,2], s=140, marker='D', label='End')

        ax.set_xlim([0,self.grid_x]); ax.set_ylim([0,self.grid_y]); ax.set_zlim([0,self.grid_z])
        ax.set_xlabel("X"); ax.set_ylabel("Y"); ax.set_zlabel("Z")

        gen_part = f" | GEN: {self.gen}" if self.gen >= 0 else ""
        ax.set_title(f"{title}{gen_part}\nFitness: {self.fitness:.2f} | Steps: {len(self.path)}", fontsize=10)
        ax.legend(loc='upper left', fontsize=7)
        ax.view_init(elev=20, azim=45)
        ax.grid(True, alpha=0.3)

def main():
    global_files = glob.glob("robot_data_[0-9]*.txt")
    global_files = [f for f in global_files if "worker" not in f]
    global_files = numeric_sort(global_files, "robot_data_")

    worker_files = numeric_sort(glob.glob("robot_data_worker_*.txt"), "robot_data_worker_")

    astar_file = "robot_data_astar.txt"
    has_astar = (astar_file in glob.glob("robot_data_astar.txt"))

    print(f"Found snapshots: {len(global_files)} | workers: {len(worker_files)} | astar: {has_astar}")

    vis=RescueVisualizer()

    print("\n1) Evolution grid (snapshots)\n2) Fitness chart\n3) Show A* baseline only\n4) Show single file")
    ch=input("Choice: ").strip()

    if ch=="1":
        if not global_files:
            print("No robot_data_*.txt found"); return
        n=len(global_files)
        rows=2 if n>=4 else 1
        cols=(n+1)//2 if n>=4 else n
        fig=plt.figure(figsize=(18,10))
        for idx,f in enumerate(global_files):
            if not vis.parse_data_file(f): continue
            snap=extract_num(f,"robot_data_")
            ax=fig.add_subplot(rows,cols,idx+1,projection="3d")
            vis.visualize_single(ax, f"Snapshot {snap}/{n}")
        plt.suptitle("GA Evolution (Snapshots)", fontsize=14, fontweight="bold")
        plt.tight_layout(rect=[0,0,1,0.96])
        plt.show()

    elif ch=="2":
        if not global_files:
            print("No robot_data_*.txt found"); return
        xs=[]; ys=[]
        for f in global_files:
            if vis.parse_data_file(f):
                xs.append(extract_num(f,"robot_data_"))
                ys.append(vis.fitness)
        pairs=sorted(zip(xs,ys), key=lambda t:t[0])
        xs,ys=zip(*pairs)
        plt.figure(figsize=(10,6))
        plt.plot(xs,ys,'o-', linewidth=2)
        plt.xlabel("Snapshot #"); plt.ylabel("Fitness"); plt.title("Fitness Evolution")
        plt.grid(True, alpha=0.3)
        plt.show()

    elif ch=="3":
        if not has_astar:
            print("robot_data_astar.txt not found"); return
        if vis.parse_data_file(astar_file):
            fig=plt.figure(figsize=(12,9))
            ax=fig.add_subplot(111,projection="3d")
            vis.visualize_single(ax, "A* Baseline")
            plt.show()

    elif ch=="4":
        files = list(global_files) + worker_files + ([astar_file] if has_astar else [])
        for i,f in enumerate(files): print(f"{i+1}. {f}")
        k=int(input("File #: "))-1
        if 0<=k<len(files) and vis.parse_data_file(files[k]):
            fig=plt.figure(figsize=(12,9))
            ax=fig.add_subplot(111,projection="3d")
            vis.visualize_single(ax, files[k])
            plt.show()

if __name__=="__main__":
    main()
