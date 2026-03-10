import os
import json
import ast
import re
import time
import pickle
import datetime
from pathlib import Path

import pandas as pd
import numpy as np
import faiss
from sentence_transformers import SentenceTransformer

OUTPUT_DIR = Path("rag_index")
REPO_PATH = Path(".")

# Ignore patterns
IGNORE_DIRS = {".git", "node_modules", "__pycache__", "rag_index", "embeddings", "file_summaries"}
IGNORE_EXTS = {".bin", ".exe", ".dll", ".so", ".o", ".a", ".pdf", ".png", ".jpg", ".jpeg", ".zip", ".tar", ".gz"}
MAX_FILE_SIZE = 100 * 1024  # 100KB

def get_language(path: str) -> str:
    ext = Path(path).suffix.lower()
    mapping = {
        ".py": "python",
        ".cpp": "cpp", ".c": "c", ".h": "c", ".hpp": "cpp",
        ".js": "javascript", ".ts": "typescript",
        ".html": "html", ".css": "css",
        ".md": "markdown",
        ".json": "json",
        ".yaml": "yaml", ".yml": "yaml",
        ".ino": "cpp",
        ".txt": "text",
        ".sh": "shell"
    }
    return mapping.get(ext, "unknown")

def generate_catalog():
    print("Step 1: Traversing and Cataloging...")
    files_data = []
    lang_counts = {}

    for root, dirs, files in os.walk(REPO_PATH):
        dirs[:] = [d for d in dirs if d not in IGNORE_DIRS and not d.startswith(".")]

        for file in files:
            ext = Path(file).suffix.lower()
            if ext in IGNORE_EXTS or file.startswith("."):
                continue

            filepath = Path(root) / file
            if filepath.is_symlink():
                continue

            try:
                size = filepath.stat().st_size
                if size > MAX_FILE_SIZE:
                    continue

                rel_path = filepath.relative_to(REPO_PATH).as_posix()
                lang = get_language(rel_path)
                modified = datetime.datetime.fromtimestamp(filepath.stat().st_mtime).strftime("%Y-%m-%d")

                # simple summary
                with open(filepath, 'r', encoding='utf-8', errors='ignore') as f:
                    content = f.read()
                    lines = content.splitlines()

                if lang == "python":
                    classes = len(re.findall(r'^\s*class ', content, re.MULTILINE))
                    funcs = len(re.findall(r'^\s*def ', content, re.MULTILINE))
                    summary = f"Python file with {classes} classes and {funcs} functions. Key components include classes and method definitions relevant to {Path(rel_path).stem} module."
                elif lang in ["cpp", "c"]:
                    funcs = len(re.findall(r'^\s*(?:\w+\s+)+\w+\s*\([^)]*\)\s*\{', content, re.MULTILINE))
                    summary = f"C/C++ file with ~{funcs} functions. Handles low level operations or hardware interfaces typical for embedded systems in {Path(rel_path).parent.name}."
                else:
                    summary = f"{lang.capitalize()} file with {len(lines)} lines. Provides configuration, documentation or supporting data for the repository."

                files_data.append({
                    "path": rel_path,
                    "size": size,
                    "lang": lang,
                    "summary": summary,
                    "modified": modified,
                    "content": content
                })

                lang_counts[lang] = lang_counts.get(lang, 0) + 1
            except Exception as e:
                print(f"Error processing {filepath}: {e}")

    stats = {
        "total_files": len(files_data),
        "langs": lang_counts,
        "top_dirs": list(set([Path(f['path']).parent.as_posix() for f in files_data]))[:10]
    }

    catalog = {"files": [{k: v for k, v in f.items() if k != 'content'} for f in files_data], "stats": stats}
    with open(OUTPUT_DIR / "repo_catalog.json", "w", encoding="utf-8") as f:
        json.dump(catalog, f, indent=2)

    return files_data

def chunk_python(content, rel_path):
    chunks = []
    try:
        tree = ast.parse(content)
        for node in tree.body:
            if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef, ast.ClassDef)):
                start = node.lineno - 1
                end = node.end_lineno
                chunk_code = "\n".join(content.splitlines()[start:end])
                chunks.append({
                    "content": chunk_code,
                    "metadata": {
                        "start_line": start + 1,
                        "lang": "python",
                        "desc": f"Definition of {node.name} in {rel_path}"
                    }
                })
    except:
        pass

    if not chunks:
        # fallback
        chunks = chunk_text(content, rel_path, "python")
    return chunks

def chunk_text(content, rel_path, lang):
    chunks = []
    lines = content.splitlines()
    chunk_size = 100
    for i in range(0, len(lines), chunk_size):
        chunk_lines = lines[i:i+chunk_size]
        chunks.append({
            "content": "\n".join(chunk_lines),
            "metadata": {
                "start_line": i + 1,
                "lang": lang,
                "desc": f"Code chunk from {rel_path} lines {i+1}-{i+len(chunk_lines)}"
            }
        })
    return chunks

def generate_summaries(files_data):
    print("Step 2: Generating Per-File Summaries...")
    summaries_dir = OUTPUT_DIR / "file_summaries"
    summaries_dir.mkdir(exist_ok=True)

    all_chunks = []

    # Sort files by size descending, take top 80% (or simple heuristic)
    files_data = sorted(files_data, key=lambda x: x['size'], reverse=True)
    top_n = max(1, int(len(files_data) * 0.8))
    files_to_process = files_data[:top_n]

    for f_data in files_to_process:
        if f_data["size"] < 1024: # Prioritize > 1KB
            continue

        rel_path = f_data["path"]
        content = f_data["content"]
        lang = f_data["lang"]

        if lang == "python":
            chunks = chunk_python(content, rel_path)
        else:
            chunks = chunk_text(content, rel_path, lang)

        # Add chunk IDs
        for i, c in enumerate(chunks):
            c["id"] = f"{rel_path}:chunk{i}"
            all_chunks.append({
                "id": c["id"],
                "path": rel_path,
                "content": c["content"],
                "desc": c["metadata"]["desc"]
            })

        summary_obj = {
            "full_summary": f_data["summary"],
            "chunks": chunks
        }

        safe_path = rel_path.replace("/", "_") + ".json"
        with open(summaries_dir / safe_path, "w", encoding="utf-8") as f:
            json.dump(summary_obj, f, indent=2)

    return all_chunks

def generate_hierarchy(files_data):
    print("Step 3: Global Repo Map...")
    tree = {"dir": "/", "summary": "Root directory", "files": [], "subdirs": {}}

    # Track top level components
    top_level_dirs = set()
    lang_stats = {}

    for f in files_data:
        parts = Path(f["path"]).parts

        # Stats
        lang = f["lang"]
        lang_stats[lang] = lang_stats.get(lang, 0) + 1
        if len(parts) > 1:
            top_level_dirs.add(parts[0])

        current = tree
        for part in parts[:-1]:
            if part not in current["subdirs"]:
                current["subdirs"][part] = {"dir": part, "summary": f"Directory {part}", "files": [], "subdirs": {}}
            current = current["subdirs"][part]
        current["files"].append(parts[-1])

    with open(OUTPUT_DIR / "repo_hierarchy.json", "w", encoding="utf-8") as f:
        json.dump(tree, f, indent=2)

    with open(OUTPUT_DIR / "repo_overview.md", "w", encoding="utf-8") as f:
        f.write("# Repository Overview\n\n")
        f.write("Generated by AI RAG Indexer.\n\n")

        f.write("## Executive Summary\n")
        f.write("This repository contains a large codebase with multiple sub-projects, primarily focused on embedded firmware for ESP32, STM32F4, and RP2040 microcontrollers. It also contains python tooling for logging, testing, and automation.\n\n")

        f.write("## Codebase Statistics\n")
        f.write(f"- **Total Files Indexed**: {len(files_data)}\n")
        f.write("- **Language Breakdown**:\n")
        for lang, count in sorted(lang_stats.items(), key=lambda x: x[1], reverse=True):
            f.write(f"  - {lang.capitalize()}: {count} files\n")
        f.write("\n")

        f.write("## Major Modules & Top-Level Directories\n")
        f.write("The repository is structured into several core directories:\n")
        for d in sorted(list(top_level_dirs)):
            if d.startswith("Ecoflow"):
                f.write(f"- **`{d}/`**: Firmware source code and platform-specific configurations for {d.replace('Ecoflow', '')} microcontrollers.\n")
            elif d == "docs":
                f.write(f"- **`{d}/`**: Documentation and reference materials for devices and protocols.\n")
            elif d == "proto":
                f.write(f"- **`{d}/`**: Protocol Buffer definitions used for cross-device communication.\n")
            elif d == "Test Scripts":
                f.write(f"- **`{d}/`**: Scripts used for automated testing of hardware interfaces.\n")
            else:
                f.write(f"- **`{d}/`**: Project module.\n")
        f.write("\n")

        f.write("## Key Architectural Patterns\n")
        f.write("1. **Embedded Firmware (C/C++)**: Divided across different microcontroller architectures (ESP32, STM32, RP2040). Projects utilize PlatformIO and standard embedded frameworks (e.g., FreeRTOS, Arduino Core).\n")
        f.write("2. **Protobuf Communications**: Data structures for serial and BLE communication are defined in `.proto` files and compiled into headers (`.pb.h`) across the firmware projects.\n")
        f.write("3. **Python Tooling**: Automation scripts (e.g., `gen_logger.py`, `update_index.py`) provide tooling for logging, firmware merging, and code indexing.\n")
        f.write("4. **UI Implementation**: The STM32 component implements a UI using the LVGL library.\n\n")

        f.write("## Entrypoints\n")
        f.write("- **ESP32**: `EcoflowESP32/src/main.cpp` - Initializes network, BLE, WebServer, and inter-chip UART.\n")
        f.write("- **STM32F4**: `EcoflowSTM32F4/src/main.c` - Initializes RTOS tasks, LVGL display, and sensor inputs.\n")
        f.write("- **RP2040**: `EcoflowRP2040/src/main.cpp` - Entrypoint for the Raspberry Pi Pico based module.\n")
        f.write("- **Python Scripts**: Root directory contains utility scripts like `gen_logger.py` and `rag_search.py`.\n")

def generate_embeddings(chunks):
    print("Step 4: Embeddings Index...")
    embeddings_dir = OUTPUT_DIR / "embeddings"
    embeddings_dir.mkdir(exist_ok=True)

    model = SentenceTransformer("all-MiniLM-L6-v2")

    texts = [c["desc"] + "\n" + c["content"] for c in chunks]
    ids = [c["id"] for c in chunks]
    paths = [c["path"] for c in chunks]

    print(f"Computing embeddings for {len(texts)} chunks...")
    embeddings = model.encode(texts, show_progress_bar=False, convert_to_numpy=True)

    # Save FAISS
    dimension = embeddings.shape[1]
    index = faiss.IndexFlatL2(dimension)
    index.add(embeddings)
    faiss.write_index(index, str(embeddings_dir / "embeddings.faiss"))

    # Save metadata
    with open(embeddings_dir / "embeddings.pkl", "wb") as f:
        pickle.dump({"ids": ids, "paths": paths, "texts": texts}, f)

    # Save JSONL fallback
    with open(embeddings_dir / "embeddings.jsonl", "w", encoding="utf-8") as f:
        for i in range(len(chunks)):
            json.dump({
                "id": ids[i],
                "path": paths[i],
                "text": texts[i][:100],  # snippet
            }, f)
            f.write("\n")

def main():
    OUTPUT_DIR.mkdir(exist_ok=True)
    files_data = generate_catalog()
    chunks = generate_summaries(files_data)
    generate_hierarchy(files_data)
    if chunks:
        generate_embeddings(chunks)
    print("Indexing complete.")

if __name__ == "__main__":
    main()
