#!/bin/bash
set -e

for file in /entrypoint.d/*.sh
do
  source "$file"
done

exec "$@"
