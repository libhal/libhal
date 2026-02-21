#!/usr/bin/env python3

# Copyright 2024 - 2025 Khalil Estell and the libhal contributors
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""
Search for clangd binary associated with LLVM toolchain.

This script searches for the clangd binary that matches the LLVM toolchain
specified in compile_commands.json. It's designed to help locate the correct
clangd version for use with language servers.

Usage:
    python find_clangd.py

Requirements:
    - compile_commands.json must exist in the project directory
    - LLVM toolchain must be properly installed

Returns:
    Path to the clangd binary or exits with error code.
"""

import json
import os
import sys
from pathlib import Path


def main():
    script_dir = os.path.dirname(os.path.abspath(__file__))

    # Find compile_commands.json
    compile_commands = os.path.join(script_dir, "compile_commands.json")
    if not os.path.isfile(compile_commands):
        print(
            f"ERROR: Could not find compile_commands.json in {script_dir}", file=sys.stderr)
        sys.exit(1)

    # Extract clangd path from compile_commands.json
    with open(compile_commands, "r") as f:
        data = json.load(f)

    if not data:
        print(
            f"ERROR: Could not extract clangd path from {compile_commands}", file=sys.stderr)
        sys.exit(1)

    # Get compiler path from first entry
    first_entry = data[0]
    command = first_entry.get("command", "")
    if not command:
        print(
            f"ERROR: Could not extract clangd path from {compile_commands}", file=sys.stderr)
        sys.exit(1)

    compiler_path = Path(command.split(' ')[0])
    clangd_path = compiler_path.with_name("clangd")

    # Verify clangd exists and is executable
    if not (clangd_path.exists() and os.access(clangd_path, os.X_OK)):
        print(
            f"ERROR: Could not find clangd at {clangd_path}", file=sys.stderr)
        sys.exit(1)

    # Execute clangd with all passed arguments
    os.execv(clangd_path, [str(clangd_path)] + sys.argv[1:])


if __name__ == "__main__":
    main()
