#!/bin/bash

# input file require ending line break, due to read not returning last line (waiting for \n)
INPUT_FILE=$1

prev=-1
inc=-1

while IFS= read -r line; do
  if [ "$line" -gt "$prev" ]; then
      let "inc+=1"
  fi
  prev="$line"
done < $INPUT_FILE

echo "Number of increasing numbers: $inc"