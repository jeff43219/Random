import os
import pandas as pd
import plotly.express as px
from datetime import datetime

# --- CONFIGURATION ---
path_to_scan = "C:\\Users"  # Scans your User folders (usually where the bulk is)
# If you want to scan the WHOLE drive, use "C:\\" (may take longer)
# ---------------------

print(f"Scanning {path_to_scan}... this might take a minute depending on your SSD speed.")

data = []

for root, dirs, files in os.walk(path_to_scan):
    for f in files:
        try:
            fp = os.path.join(root, f)
            # Get size in Megabytes
            size = os.path.getsize(fp) / (1024 * 1024)
            if size > 1:  # Only track files larger than 1MB to keep the map clean
                data.append({
                    'File': f,
                    'Parent': os.path.basename(root),
                    'Path': root,
                    'Size (MB)': size,
                    'Extension': os.path.splitext(f)[1].lower()
                })
        except (PermissionError, OSError):
            continue

# Turn the data into a "Dataframe" (like a spreadsheet)
df = pd.DataFrame(data)

print("Building your heatmap...")

# Create an interactive Treemap
fig = px.treemap(
    df, 
    path=[px.Constant(path_to_scan), 'Parent', 'File'], 
    values='Size (MB)',
    color='Extension',
    hover_data=['Size (MB)'],
    title=f"Disk Usage Heatmap: {path_to_scan}"
)

# This will open a tab in your browser with the interactive map
fig.show()

print("Done! Check your browser.")