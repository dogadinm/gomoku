#!/bin/sh
# Opens a demonstration position: ./demo/play.sh 03
# In the menu choose 2 (two humans), then press L to load the position.
root=$(cd "$(dirname "$0")/.." && pwd)
file=$(ls "$root"/demo/"$1"-*.save 2>/dev/null | head -n 1)
if [ -z "$file" ] || [ ! -x "$root/Gomoku" ]; then
    echo "usage: demo/play.sh NN   (run make first; NN is 01..09)" >&2
    exit 1
fi
dir=$(mktemp -d)
cp "$file" "$dir/gomoku.save"
cd "$dir" && exec "$root/Gomoku"
