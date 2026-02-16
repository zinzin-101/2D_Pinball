import os

# Get current directory
current_dir = os.getcwd()

# Get all files (exclude directories)
files = [f for f in os.listdir(current_dir) if os.path.isfile(f)]

# Sort files (optional but recommended)
files.sort()

# Rename files
for index, filename in enumerate(files, start=1):
    name, ext = os.path.splitext(filename)
    if ext == ".py": continue
    new_name = f"number{index-1}{ext}"
    
    os.rename(filename, new_name)
    print(f"Renamed: {filename} -> {new_name}")

print("Done renaming files.")