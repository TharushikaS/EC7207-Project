import numpy as np
import matplotlib.pyplot as plt
from mpl_toolkits.mplot3d import Axes3D
import matplotlib.animation as animation
import os

# --- Configuration ---
N = 2000  # Must match the N in your C++ code
STEPS = 2000
OUTPUT_FREQ = 100
DATA_DIR = "../data/ground_truth/" # Path relative to the scripts folder

# Check if data directory exists
if not os.path.exists(DATA_DIR):
    print(f"Error: Could not find directory {DATA_DIR}. Did you run the C++ code?")
    exit()

# Generate the file list based on C++ output frequency
frames = range(0, STEPS, OUTPUT_FREQ)

# Setup the plot
fig = plt.figure(figsize=(10, 8))
ax = fig.add_subplot(111, projection='3d')
X, Y = np.meshgrid(range(N), range(N))

# Initialize an empty surface plot
# We use a stride of 5 to speed up rendering (plotting 500x500 in 3D is heavy for Python)
plot = [ax.plot_surface(X, Y, np.zeros((N, N)), color='b', rstride=5, cstride=5)]

ax.set_zlim(-1.0, 1.0)
ax.set_title("Tsunami Wave Propagation (Serial Baseline)")
ax.set_axis_off() # Hides the grid lines for a cleaner look

def update_plot(frame_number):
    filename = f"{DATA_DIR}output_{frame_number}.bin"
    
    try:
        # Read the raw binary C++ data
        data = np.fromfile(filename, dtype=np.float64)
        grid = data.reshape((N, N))
    except FileNotFoundError:
        print(f"File {filename} not found. Stopping animation.")
        return plot

    # Remove the old surface and plot the new one
    plot[0].remove()
    plot[0] = ax.plot_surface(X, Y, grid, cmap="Blues", rstride=5, cstride=5, vmin=-0.5, vmax=0.5)
    
    # Update title
    ax.set_title(f"Tsunami Wave Propagation - Step {frame_number}")
    return plot

print("Generating animation... this might take a minute.")

# Create the animation

ani = animation.FuncAnimation(fig, update_plot, frames=frames, interval=50, blit=False)

# Show the interactive plot window
plt.show()

# Optional: To save as an mp4, uncomment the line below (requires ffmpeg installed)
# ani.save('tsunami_serial.mp4', writer='ffmpeg', fps=15)