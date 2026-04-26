import os
import re
import sys
from rich.tree import Tree
from rich.console import Console

console = Console()

def clean_filename(path):
    if path == ".":
        path = os.path.basename(os.getcwd())
    clean = re.sub(r'[\\/:]', '_', path)
    return clean.strip('_')

def generate_tree_logic(root_dir, indent=""):
    tree_str = ""
    try:
        items = sorted([i for i in os.listdir(root_dir) if not i.startswith('.') and i not in ["__pycache__", ".venv"]])
    except PermissionError:
        return indent + "[Permission Denied]\n"

    for i, item in enumerate(items):
        path = os.path.join(root_dir, item)
        is_last = (i == len(items) - 1)
        connector = "└── " if is_last else "├── "
        tree_str += f"{indent}{connector}{item}\n"
        if os.path.isdir(path):
            extension = "    " if is_last else "│   "
            tree_str += generate_tree_logic(path, indent + extension)
    return tree_str

def add_to_rich_tree(rich_node, path):
    try:
        items = sorted([i for i in os.listdir(path) if not i.startswith('.') and i not in ["__pycache__", ".venv"]])
    except PermissionError:
        rich_node.add("[red]Permission Denied[/]")
        return

    for item in items:
        item_path = os.path.join(path, item)
        if os.path.isdir(item_path):
            branch = rich_node.add(f"[bold magenta]📁 {item}[/]")
            add_to_rich_tree(branch, item_path)
        else:
            rich_node.add(f"[green]📄 {item}[/]")

def main():
    # --- UPDATED: CLI Argument Logic ---
    if len(sys.argv) > 1:
        raw_path = sys.argv[1].strip()
    else:
        raw_path = input("Enter the path you want to map (or '.' for current): ").strip()
    
    if raw_path == ".":
        target_path = os.getcwd()
    else:
        target_path = os.path.abspath(raw_path)
    
    if not os.path.exists(target_path):
        console.print("[bold red]Path doesn't exist![/]")
        return

    console.print(f"\n[bold yellow]Mapping:[/] [cyan]{target_path}[/]\n")
    rich_tree = Tree(f"[bold blue]ROOT:[/] {target_path}")
    add_to_rich_tree(rich_tree, target_path)
    console.print(rich_tree)

    output_base_dir = r"C:\Users\rayan\Desktop\Random"
    output_folder = os.path.join(output_base_dir, "Folder_Structures")
    
    if not os.path.exists(output_folder):
        os.makedirs(output_folder)
        console.print(f"[dim]Ensured directory exists: {output_folder}[/]")

    name_base = clean_filename(raw_path)
    output_file = os.path.join(output_folder, f"{name_base}.txt")
    
    structure_text = generate_tree_logic(target_path)
    with open(output_file, "w", encoding="utf-8") as f:
        f.write(f"Structure of: {target_path}\n" + "="*30 + "\n" + structure_text)
    
    console.print(f"\n[bold green]✅ Success![/] Saved to [bold underline]{output_file}[/]\n")

if __name__ == "__main__":
    main()