# This file was generated with the assistance of ChatGPT 5.2 (OpenAI).

import toml
import os
import re

# =====================
# TYPE MAPS
# =====================

CPP_TYPES = {
    "uint8_t": "uint8_t",
    "uint16_t": "uint16_t",
    "int16_t": "int16_t",
    "uint32_t": "uint32_t",
    "int": "int",
    "float": "double",
    "bool": "bool",
    "string": "const char*",
}

# =====================
# HELPERS
# =====================

def array_dims(v):
    dims = []
    while isinstance(v, list):
        dims.append(len(v))
        v = v[0] if v else []
    return dims

def cpp_init(v):
    if isinstance(v, list):
        return "{ " + ", ".join(cpp_init(x) for x in v) + " }"
    if isinstance(v, bool):
        return "true" if v else "false"
    if isinstance(v, str):
        return f'"{v}"'
    return str(v)

def py_init(v):
    if isinstance(v, list):
        return "[" + ", ".join(py_init(x) for x in v) + "]"
    if isinstance(v, bool):
        return "True" if v else "False"
    if isinstance(v, str):
        return f'"{v}"'
    return str(v)

# =====================
# C++ GENERATION
# =====================

def cpp_declare(name, t, v):
    name = name.upper()

    # STRING SCALAR
    if t == "string":
        return f'constexpr const char* {name} = "{v}";'

    # ARRAY (N-D)
    if t.endswith("[]"):
        base = t.replace("[]", "")
        dims = array_dims(v)
        dim_str = "".join(f"[{d}]" for d in dims)
        return (
            f"constexpr {CPP_TYPES[base]} {name}{dim_str} = "
            f"{cpp_init(v)};"
        )

    # BOOL
    if t == "bool":
        return f"constexpr bool {name} = {'true' if v else 'false'};"

    # SCALAR
    return f"constexpr {CPP_TYPES[t]} {name} = {v};"

# =====================
# PYTHON GENERATION
# =====================

def py_declare(name, t, v):
    return f"{name.upper()} = {py_init(v)}"

# =====================
# BLOCK REPLACER
# =====================

def replace_block(filepath, new_lines):
    with open(filepath, "r") as f:
        code = f.read()

    block = "\n".join([
        "# ==== AUTO-GENERATED CONFIG START ====",
        "# (do not edit this block manually)",
        *new_lines,
        "# ==== AUTO-GENERATED CONFIG END ===="
    ])

    updated = re.sub(
        r"# ==== AUTO-GENERATED CONFIG START ====[\s\S]*?# ==== AUTO-GENERATED CONFIG END ====",
        block,
        code
    )

    with open(filepath, "w") as f:
        f.write(updated)

# =====================
# MAIN
# =====================

def main():
    project_dir = os.getenv("PROJECT_DIR", os.getcwd())
    repo_root = os.path.abspath(os.path.join(project_dir, ".."))

    cfg = toml.load(os.path.join(repo_root, "config", "config.toml"))

    # -------- C++ --------
    cpp_lines = [
        "// Auto-generated config",
        "#pragma once",
        "#include <stdint.h>",
        "",
    ]

    for name, e in cfg.items():
        cpp_lines.append(cpp_declare(name, e["type"], e["value"]))

    for path in [
        "sensor_node/include/config.h",
        "BLE_gateway/include/config.h",
    ]:
        full = os.path.join(repo_root, path)
        os.makedirs(os.path.dirname(full), exist_ok=True)
        with open(full, "w") as f:
            f.write("\n".join(cpp_lines))

    # -------- Python --------
    py_lines = [py_declare(n, e["type"], e["value"]) for n, e in cfg.items()]

    replace_block(os.path.join(repo_root, "control_station", "control_station.py"), py_lines)
    replace_block(os.path.join(repo_root, "server", "server.py"), py_lines)

    print("[CONFIG] Done.")

if __name__ == "__main__":
    main()
