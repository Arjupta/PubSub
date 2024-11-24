import matplotlib.pyplot as plt
import numpy as np

# Function to read and parse data from a file
def read_data(file_name):
    threadpools = []
    requests = []
    times = []
    with open(file_name, 'r') as f:
        for line in f:
            # Split the line by comma and convert values
            threadpool, request, time = line.strip().split(',')
            threadpools.append(int(threadpool))
            requests.append(int(request))
            times.append(float(time))
    return threadpools, requests, times

# Read data from the publisher and subscriber times files
pub_threadpools, pub_requests, pub_times = read_data("./output/plot/pub_times.txt")
sub_threadpools, sub_requests, sub_times = read_data("./output/plot/sub_times.txt")

# Function to create and plot the data
def plot_response_time(threadpools, requests, times, title, label_prefix):
    # Create a figure and axis for the plot
    fig, ax = plt.subplots(figsize=(10, 6))
    
    # Define unique threadpool sizes
    threadpool_sizes = sorted(set(threadpools))

    # Plot the data for each threadpool size
    for threadpool in threadpool_sizes:
        # Extract data for the current threadpool size
        x = [req for req, tpool in zip(requests, threadpools) if tpool == threadpool]
        y = [time for req, tpool, time in zip(requests, threadpools, times) if tpool == threadpool]
        
        # Plot line for this threadpool size
        ax.plot(x, y, label=f'{label_prefix} - Threadpool {threadpool}', marker='o')

    # Adding labels and title
    ax.set_xlabel('Number of Requests')
    ax.set_ylabel('Average Response Time (ms)')
    ax.set_title(title)

    # Show the legend
    ax.legend()

    # Save the plot
    plt.tight_layout()
    plt.savefig(f"./output/plot/{label_prefix}_plot.png", dpi=300, format="png")
    # plt.show()

# Plot Publisher Data
plot_response_time(pub_threadpools, pub_requests, pub_times, 'Publisher: Average Response Time vs. Number of Requests', 'Publisher')

# Plot Subscriber Data
plot_response_time(sub_threadpools, sub_requests, sub_times, 'Subscriber: Average Response Time vs. Number of Requests', 'Subscriber')
