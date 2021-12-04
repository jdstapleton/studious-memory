#!/bin/bash

# input file require ending line break, due to read not returning last line (waiting for \n)
INPUT_FILE=$1

prev1=""
prev2=""
prev3=""
inc=0

while IFS= read -r line; do
  if [[ ! -z "$prev1" ]]; then
    let "prev_win=$prev1+$prev2+$prev3"
    let "cur_win=$prev2+$prev3+$line"
    if [[ "$cur_win" -gt "$prev_win" ]]; then
        let "inc+=1"
    fi
  fi
  prev1=$prev2
  prev2=$prev3
  prev3=$line
done < $INPUT_FILE

echo "Number of increasing numbers: $inc"