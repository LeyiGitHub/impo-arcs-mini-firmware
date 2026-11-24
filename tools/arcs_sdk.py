"""Select an explicit ListenAI runtime ABI; never infer it from product naming."""
from pathlib import Path

SUPPORTED = ("0.1.6", "0.1.8")

def validate(base, requested):
    if requested not in SUPPORTED:
        raise ValueError("Unsupported ListenAI SDK profile: " + requested)
    version_file = Path(base) / "VERSION"
    if not version_file.is_file():
        raise ValueError("Set ARCS_BASE to your ListenAI ARCS SDK directory")
    values = {}
    for line in version_file.read_text().splitlines():
        if "=" in line:
            key, value = line.split("=", 1)
            values[key.strip()] = value.strip()
    actual = ".".join(values.get(k, "?") for k in ("VERSION_MAJOR", "VERSION_MINOR", "PATCHLEVEL"))
    if actual != requested:
        raise ValueError(f"ARCS_BASE contains {actual}, but --arcs-sdk-version selects {requested}; choose the profile matching the retained AP")
    return actual

def build_directory(product_id, version):
    if version not in SUPPORTED:
        raise ValueError("Unsupported ListenAI SDK profile: " + version)
    return "build-" + product_id + ("-arcs018" if version == "0.1.8" else "")
