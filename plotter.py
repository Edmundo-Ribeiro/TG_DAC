import os
import matplotlib.pyplot as plt

from datetime import datetime


def get_files_in_directory(directory):
    file_list = []
    for root, dirs, files in os.walk(directory):
        # print(root, dirs, files)
        for file in files:
            file_list.append(os.path.join(root, file))
    return file_list

# Get the current directory
current_directory = os.getcwd()

# Find directories starting with "DAC_"
dac_directories = [directory for directory in os.listdir(current_directory) if os.path.isdir(directory) and directory.startswith("DAC_0001")]
# for directory in dac_directories:


# Retrieve all files inside the directories (including subdirectories)
all_files = []
for directory in dac_directories:
    files = get_files_in_directory(directory)
    all_files.extend(files)

# Print the file paths
# for file_path in all_files:
#     print(file_path)


def plot_data_from_file(file_paths):
    for file_path in file_paths:
        ids = []
        values = []
        timestamps = []

        with open(file_path, 'r') as file:
            first_timestamp = None
            for line in file:
                line = line.strip()
                if line:
                    id_value, value, timestamp = line.split(',')
                    ids.append(id_value)
                    values.append(float(value)*1000/32768*0.256)

                    # Calculate time difference from the first timestamp in microseconds
                    if first_timestamp is None:
                        first_timestamp = int(timestamp)
                        time_diff = 0
                    else:
                        current_timestamp = int(timestamp)
                        time_diff = (current_timestamp - first_timestamp) / 1000000  # Convert microseconds to seconds

                    timestamps.append(time_diff)

        # Plot the data for the current file
        plt.plot(timestamps, values, 'o-', label=file_path)

    # Set the labels, title, legend, and grid for the plot
    plt.xlabel('Time Difference (seconds)')
    plt.ylabel('Value')
    plt.title('Data Plot')
    plt.xticks(rotation=45)
    # plt.legend()
    plt.grid(True)
    plt.show()

plot_files= []
sensor_name = "termo2"
for f in all_files:
    if(f.find(sensor_name) > 0):
        plot_files.append(f)

plot_data_from_file(plot_files)