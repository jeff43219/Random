import time
import psutil
import pynvml
from rich.live import Live
from rich.table import Table
from rich.panel import Panel
from rich.layout import Layout
from rich.console import Console
from rich.progress import BarColumn, Progress, TextColumn
from pathlib import Path
import subprocess

# 1. Get the path of the folder this script is in (C:\...\Random\python)
script_dir = Path(__file__).parent

# 2. Go up one level to the "Random" root (C:\...\Random)
root_dir = script_dir.parent

# 3. Define paths to your tools relative to the root
ENCRYPTOR_EXE = root_dir / "bin" / "encryptor.exe"
DISK_MAP_SCRIPT = script_dir / "disk_map.py"

# Example: How to run your C++ tool from the dashboard
def run_encryptor():
    if ENCRYPTOR_EXE.exists():
        subprocess.run([str(ENCRYPTOR_EXE)])
    else:
        print(f"Error: Could not find {ENCRYPTOR_EXE}")
# Initialize NVIDIA Management Library
try:
    pynvml.nvmlInit()
    gpu_handle = pynvml.nvmlDeviceGetHandleByIndex(0)
    gpu_name = pynvml.nvmlDeviceGetName(gpu_handle)
except:
    gpu_handle = None

console = Console()

def get_stats():
    # CPU Stats
    cpu_usage = psutil.cpu_percent(interval=None)
    cpu_freq = psutil.cpu_freq().current / 1000 # Convert to GHz
    
    # RAM Stats
    ram = psutil.virtual_memory()
    
    # GPU Stats
    if gpu_handle:
        gpu_util = pynvml.nvmlDeviceGetUtilizationRates(gpu_handle).gpu
        gpu_temp = pynvml.nvmlDeviceGetTemperature(gpu_handle, 0)
        gpu_mem = pynvml.nvmlDeviceGetMemoryInfo(gpu_handle)
        gpu_mem_used = gpu_mem.used / (1024**2) # MB
        gpu_mem_total = gpu_mem.total / (1024**2) # MB
    else:
        gpu_util, gpu_temp, gpu_mem_used, gpu_mem_total = 0, 0, 0, 1

    return {
        "cpu": cpu_usage, "cpu_f": cpu_freq,
        "ram_p": ram.percent, "ram_u": ram.used / (1024**3), "ram_t": ram.total / (1024**3),
        "gpu_u": gpu_util, "gpu_t": gpu_temp, "gpu_mu": gpu_mem_used, "gpu_mt": gpu_mem_total
    }

def make_layout() -> Layout:
    layout = Layout()
    layout.split_column(
        Layout(name="header", size=3),
        Layout(name="main", size=10),
        Layout(name="footer", size=3)
    )
    return layout

def generate_table(stats) -> Table:
    table = Table(show_header=True, header_style="bold magenta", expand=True)
    table.add_column("Component", style="dim", width=12)
    table.add_column("Load / Usage", justify="right")
    table.add_column("Temp / Freq", justify="right")
    table.add_column("Memory", justify="right")

    # CPU Row
    table.add_row(
        "Ryzen 7", 
        f"[bold cyan]{stats['cpu']}%[/]", 
        f"{stats['cpu_f']:.2f} GHz", 
        "N/A"
    )
    # GPU Row
    table.add_row(
        "RTX 4060", 
        f"[bold green]{stats['gpu_u']}%[/]", 
        f"{stats['gpu_t']}°C", 
        f"{stats['gpu_mu']:.0f}/{stats['gpu_mt']:.0f} MB"
    )
    # RAM Row
    table.add_row(
        "DDR5 RAM", 
        f"[bold yellow]{stats['ram_p']}%[/]", 
        "Stable", 
        f"{stats['ram_u']:.1f}/{stats['ram_t']:.1f} GB"
    )
    return table

# --- Main Display Loop ---
layout = make_layout()
layout["header"].update(Panel("[bold white]🚀 POWER-USER SYSTEM DASHBOARD[/]", style="blue"))
layout["footer"].update(Panel("[italic white]Press Ctrl+C to Exit[/]", style="dim"))

with Live(layout, refresh_per_second=2):
    try:
        while True:
            stats = get_stats()
            layout["main"].update(generate_table(stats))
            time.sleep(0.5)
    except KeyboardInterrupt:
        pynvml.nvmlShutdown()