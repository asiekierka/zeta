#!/bin/bash
EXEC_DIR="$(dirname "$0")"
if [ -f "$EXEC_DIR"/ZZT.EXE ] || [ -f "$EXEC_DIR"/SUPERZ.EXE ]; then
	cd "$EXEC_DIR"
else
	cd "$EXEC_DIR"/../../..
fi
SCRIPT_DIR=$(pwd)
"$EXEC_DIR"/zeta86 "$@"
