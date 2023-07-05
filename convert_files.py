import datetime
import os
from pathlib import Path


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

def convert_timestamp(epoch_microseconds):
    timestamp = datetime.datetime.fromtimestamp(epoch_microseconds / 1000000.0)
    return timestamp.strftime("%d-%m-%Y,%H:%M:%S,%f")

def do_something(file_paths):
    for file_path in file_paths:
        new_full_file_path = os.path.join("excel",file_path)
        new_path,name = os.path.split(new_full_file_path)
        Path(new_path).mkdir(mode=777, parents=True, exist_ok=True)

        new_file = open(new_full_file_path, 'w')
        gain = 0.256 if file_path.find("termo")>0 else 1.024
        with open(file_path, 'r') as file:
            for line in file:
                line = line.strip()
                if line:
                    id_value, value, timestamp = line.split(',')
                    value = int(value)*1000*gain/32768
                    timestamp = convert_timestamp(int(timestamp))
                    new_line = f"{id_value},{value},{timestamp}\n"
                    new_file.write(new_line)


all_files = []
for directory in dac_directories:
    files = get_files_in_directory(directory)
    print(files)
    do_something(files)
    all_files.extend(files)
