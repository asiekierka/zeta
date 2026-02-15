#!/bin/sh
EXEC_DIR="$(dirname "$0")"
cd "$EXEC_DIR"/../../..
SCRIPT_DIR=$(pwd)
"$EXEC_DIR"/zeta "$@"
