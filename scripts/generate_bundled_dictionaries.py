#!/usr/bin/env python3

import argparse
import tarfile
from pathlib import Path


DICT_FILES = {
    "4.0.3": "python_ags4-1.1.0/python_ags4/Standard_dictionary_v4_0_3.ags",
    "4.0.4": "python_ags4-1.1.0/python_ags4/Standard_dictionary_v4_0_4.ags",
    "4.1": "python_ags4-1.1.0/python_ags4/Standard_dictionary_v4_1.ags",
    "4.1.1": "python_ags4-1.1.0/python_ags4/Standard_dictionary_v4_1_1.ags",
}


def to_c_byte_array(data: bytes) -> str:
    lines = []
    line = []

    for index, byte in enumerate(data):
        line.append(f"0x{byte:02x}")
        if len(line) == 12 or index == len(data) - 1:
            lines.append("  " + ", ".join(line))
            line = []

    return ",\n".join(lines) + "\n"


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--tarball", required=True)
    parser.add_argument("--output", required=True)
    args = parser.parse_args()

    output = Path(args.output)
    output.parent.mkdir(parents=True, exist_ok=True)

    blobs = {}
    with tarfile.open(args.tarball, "r:gz") as tar:
        for version, member_name in DICT_FILES.items():
            member = tar.extractfile(member_name)
            if member is None:
                raise SystemExit(f"missing dictionary member: {member_name}")
            data = member.read()
            blobs[version] = data

    with output.open("w", encoding="utf-8", newline="\n") as f:
        f.write("#include <stddef.h>\n")
        f.write("#include <string.h>\n\n")
        f.write("#include \"dictionary_bundle_data.h\"\n\n")
        for version, data in blobs.items():
            symbol = version.replace(".", "_")
            f.write(f"static const unsigned char dictionary_{symbol}[] = {{\n")
            f.write(to_c_byte_array(data))
            f.write("};\n\n")

        f.write("const char *ags_bundled_dictionary_data(const char *version, size_t *out_length) {\n")
        f.write("  if (version == NULL) {\n")
        f.write("    return NULL;\n")
        f.write("  }\n")
        for version in blobs:
            symbol = version.replace(".", "_")
            f.write(f"  if (strcmp(version, \"{version}\") == 0) {{\n")
            f.write(f"    if (out_length != NULL) *out_length = sizeof(dictionary_{symbol});\n")
            f.write(f"    return (const char *)dictionary_{symbol};\n")
            f.write("  }\n")
        f.write("  return NULL;\n")
        f.write("}\n")


if __name__ == "__main__":
    main()
