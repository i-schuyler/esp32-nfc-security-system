# tools/pio_set_srcdir.py
# Sets the project source directory for diagnostic envs so they build src_diagnostics/
Import("env")
import os

pioenv = env.get("PIOENV", "")
project_dir = env.get("PROJECT_DIR", "")

# PlatformIO supports PROJECT_SRC_DIR and also a legacy PROJECTSRC_DIR internally.
# We set both for maximum compatibility.
if pioenv.startswith("diagnostic_"):
    diag_src = os.path.join(project_dir, "src_diagnostics")
    env["PROJECTSRC_DIR"] = diag_src
    env["PROJECT_SRC_DIR"] = diag_src
    print(f"[pio_set_srcdir] Using diagnostic source dir: {diag_src}")

# tools/pio_set_srcdir.py EOF
