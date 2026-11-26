#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
: "${ARCS_BASE:?Set ARCS_BASE for IJG and cJSON}"
output=$(mktemp -d)
trap 'rm -rf "$output"' EXIT
J="$ARCS_BASE/modules/ijg"
# Compile the actual SDK IJG compression sources, not a mock JPEG writer.
python3 - "$J" "$output" <<'PY'
import re,subprocess,sys
from pathlib import Path
j,o=map(Path,sys.argv[1:])
c=(j/'CMakeLists.txt').read_text().split('list(APPEND ijg_dempression_src')[0]
c='\n'.join(l for l in c.splitlines() if not l.lstrip().startswith('#'))
for leaf in re.findall(r'\$\{IJG_DIR\}/(\w+\.c)',c):
    subprocess.run(['cc','-std=c11','-w','-fsanitize=address','-I'+str(j),'-c',str(j/leaf),'-o',str(o/(leaf+'.o'))],check=True)
PY
cc -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined -Isdk/include -I"$J" \
 tests/test_camera_encode.c sdk/src/impo_arcs_camera_encode.c "$output"/*.o -lm -o "$output/jpeg"
"$output/jpeg" "$output/colors.jpg"
cc -std=c11 -Wall -Wextra -Werror -Wno-deprecated-declarations -fsanitize=address,undefined \
 -Itests/camera_async_stubs -Iupstream/arcs/link -I"$ARCS_BASE/modules/cjson" \
 tests/test_camera_async.c upstream/arcs/link/link_camera.c upstream/arcs/link/link_glue_logic.c \
 "$ARCS_BASE/modules/cjson/cJSON.c" -o "$output/async"
"$output/async"
