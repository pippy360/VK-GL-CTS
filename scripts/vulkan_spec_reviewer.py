#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Vulkan Specification Conformance Reviewer Agent (vulkan_spec_reviewer)
----------------------------------------------------------------------
An autonomous Khronos Vulkan Specification and Conformance Auditor script.
Reviews proposed VK-GL-CTS / dEQP-VK test changes and performance optimizations
against the official Khronos Vulkan Specification in external/vulkan-docs/src/
to ensure 100% default conformance and identical test coverage without
requiring command-line arguments.

Usage Examples:
  # Review a proposed change described in text:
  python3 scripts/vulkan_spec_reviewer.py "Reduce default WSI swapchain render loop to 10 frames"

  # Review a proposal or patch file:
  python3 scripts/vulkan_spec_reviewer.py --file path/to/proposal.md

  # Dump the JSON agent configuration:
  python3 scripts/vulkan_spec_reviewer.py --dump-json

  # Dump the system prompt:
  python3 scripts/vulkan_spec_reviewer.py --dump-prompt
"""

import argparse
import json
import os
import subprocess
import sys

# Default standalone configuration (in sync with scripts/vulkan_spec_reviewer.json)
DEFAULT_CONFIG = {
    "name": "vulkan_spec_reviewer",
    "description": "Expert Khronos Vulkan Specification and Conformance Auditor. Reviews dEQP-VK test optimizations against the authoritative Vulkan Specification in external/vulkan-docs/ to ensure conformance and coverage equivalence without command line arguments.",
    "enable_write_tools": False,
    "enable_mcp_tools": False,
    "enable_subagent_tools": False,
    "system_prompt": (
        "You are an expert Khronos Vulkan Specification and Conformance Auditor. Your mandate is to audit proposed "
        "performance optimizations and code changes for VK-GL-CTS / dEQP-VK against the authoritative Vulkan Specification "
        "in external/vulkan-docs/src/ (including chapters/, appendices/, and xml/vk.xml).\n\n"
        "CRITICAL REQUIREMENT:\n"
        "\"No command line arguments should be used/needed.\"\n"
        "All optimizations and code modifications must be evaluated as DEFAULT CTS changes. Do NOT recommend or rely on "
        "command-line flags or optional runtime arguments to make an optimization conformant. The default behavior must be 100% conformant.\n\n"
        "For every proposed optimization or code change, you MUST:\n"
        "1. Search and read the authoritative Vulkan Specification files in external/vulkan-docs/src/ (e.g., chapters/wsi.adoc, "
        "chapters/pipelines.adoc, chapters/drawing.adoc, appendices/VK_KHR_swapchain.adoc, appendices/VK_KHR_surface.adoc, "
        "appendices/VK_EXT_surface_maintenance1.adoc, and xml/vk.xml).\n"
        "2. Assess Conformance Validity: Verify whether the proposed default CTS change complies with all Valid Usage (VU) statements "
        "and normative specification language.\n"
        "3. Assess Coverage Equivalence (\"won't change how much we're testing\"): Check whether reducing loop iterations, caching "
        "resources, batching draw calls, or reusing OS windows/surfaces tests the exact same API requirements, state transitions, "
        "synchronization guarantees, and edge cases as the unmodified CTS. If an optimization reduces coverage, explain exactly what "
        "coverage would be lost and how to adjust the optimization so zero coverage is lost.\n"
        "4. Provide explicit Specification Citations: Cite specific file names, chapter headers, section titles, and Valid Usage IDs "
        "from external/vulkan-docs/src/.\n"
        "5. Assign a clear Verdict for each proposal:\n"
        "   - [APPROVED]: The change is 100% conformant by default and maintains identical test coverage.\n"
        "   - [APPROVED WITH CONDITIONS]: The change is conformant by default if specific implementation conditions are met "
        "     (without using command line arguments). Detail the exact condition and required C++ code changes.\n"
        "   - [REJECTED]: The change would violate Vulkan conformance or reduce required test coverage by default.\n\n"
        "When responding, output a detailed, structured Conformance Audit Report."
    )
}

def load_config():
    """Loads vulkan_spec_reviewer.json if present in the same directory, otherwise uses default configuration."""
    script_dir = os.path.dirname(os.path.abspath(__file__))
    json_path = os.path.join(script_dir, "vulkan_spec_reviewer.json")
    if os.path.exists(json_path):
        try:
            with open(json_path, "r", encoding="utf-8") as f:
                return json.load(f)
        except Exception as e:
            sys.stderr.write(f"Warning: Could not load {json_path}: {e}\nUsing default configuration.\n")
    return DEFAULT_CONFIG

def main():
    parser = argparse.ArgumentParser(
        description="Khronos Vulkan Specification and Conformance Auditor (vulkan_spec_reviewer)"
    )
    parser.add_argument("prompt", nargs="?", help="Text description of the proposed change or optimization to review")
    parser.add_argument("-f", "--file", help="Path to a proposal document, patch file, or source diff to review")
    parser.add_argument("--model", default="pro", choices=["flash_lite", "flash", "pro"], help="Agent API model tier to use (default: pro)")
    parser.add_argument("--title", default="Vulkan Spec Conformance Review", help="Title for the agent conversation")
    parser.add_argument("--dump-json", action="store_true", help="Print the JSON agent configuration and exit")
    parser.add_argument("--dump-prompt", action="store_true", help="Print the agent system prompt and exit")
    
    args = parser.parse_args()
    config = load_config()
    
    if args.dump_json:
        print(json.dumps(config, indent=2))
        return 0
        
    if args.dump_prompt:
        print(config.get("system_prompt", ""))
        return 0
        
    review_content = ""
    if args.file:
        if not os.path.exists(args.file):
            sys.stderr.write(f"Error: File not found: {args.file}\n")
            return 1
        with open(args.file, "r", encoding="utf-8") as f:
            review_content += f"=== Content of {args.file} ===\n" + f.read() + "\n"
            
    if args.prompt:
        review_content += "\n=== Proposed Change / Review Prompt ===\n" + args.prompt
        
    if not review_content.strip():
        parser.print_help()
        sys.stderr.write("\nError: Please provide a prompt string or use --file <path> to specify the changes to review.\n")
        return 1
        
    full_prompt = (
        f"{config['system_prompt']}\n\n"
        "--- START OF PROPOSED CHANGES TO REVIEW ---\n"
        f"{review_content.strip()}\n"
        "--- END OF PROPOSED CHANGES TO REVIEW ---\n\n"
        "Please conduct a comprehensive Vulkan Specification Conformance Audit on the above proposal(s). "
        "Enforce that zero command line arguments are used/needed, check coverage equivalence, and output a structured "
        "Conformance Audit Report with verdicts ([APPROVED], [APPROVED WITH CONDITIONS], or [REJECTED])."
    )
    
    cmd = [
        "agentapi",
        "new-conversation",
        f"--model={args.model}",
        f"--title={args.title}",
        full_prompt
    ]
    
    print(f"Launching Vulkan Spec Reviewer agent conversation (Model: {args.model})...")
    try:
        result = subprocess.run(cmd, check=True)
        return result.returncode
    except subprocess.CalledProcessError as e:
        sys.stderr.write(f"Error launching agentapi: {e}\n")
        return e.returncode
    except FileNotFoundError:
        sys.stderr.write("Error: 'agentapi' CLI not found in PATH.\n")
        return 1

if __name__ == "__main__":
    sys.exit(main())
