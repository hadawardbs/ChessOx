import matplotlib.pyplot as plt
import csv
import sys
import os

def plot_cls(csv_file):
    if not os.path.exists(csv_file):
        print(f"Error: {csv_file} not found.")
        return

    sims = []
    entropy = []
    voc = []
    pressure = []
    evals = []
    time_pts = []

    with open(csv_file, 'r') as f:
        reader = csv.DictReader(f)
        for row in reader:
            try:
                sims.append(int(row['Sims']))
                entropy.append(float(row['Entropy']))
                voc.append(float(row['VOC']))
                pressure.append(float(row['Pressure']))
                # BestVal might be string "0.53"
                evals.append(float(row['BestVal']))
                time_pts.append(float(row['Time']))
            except: pass

    if not sims:
        print("No valid data points found.")
        return

    fig, ax1 = plt.subplots(figsize=(10, 6))

    color = 'tab:red'
    ax1.set_xlabel('Time (s)')
    ax1.set_ylabel('Evaluation (WinProb)', color=color)
    ax1.plot(time_pts, evals, color=color, label='Evaluation')
    ax1.tick_params(axis='y', labelcolor=color)

    ax2 = ax1.twinx()  # instantiate a second axes that shares the same x-axis

    color = 'tab:blue'
    ax2.set_ylabel('Cognitive Metrics', color=color)  # we already handled the x-label with ax1
    ax2.plot(time_pts, entropy, color=color, linestyle='--', label='Entropy')
    ax2.plot(time_pts, pressure, color='green', linestyle=':', label='Pressure')
    ax2.tick_params(axis='y', labelcolor=color)

    plt.title('Oxta Cognitive Dynamics: Evaluation vs Entropy/Pressure')
    fig.tight_layout()  # otherwise the right y-label is slightly clipped
    plt.legend(loc='upper left')

    output_file = "cognitive_plot.png"
    plt.savefig(output_file)
    print(f"Plot saved to {output_file}")

if __name__ == "__main__":
    target = "search_metrics.csv"
    if len(sys.argv) > 1: target = sys.argv[1]
    plot_cls(target)
