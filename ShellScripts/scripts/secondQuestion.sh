#!/bin/bash

# Mert Kelkit - 150115013
# Furkan Nakıp - 150115032

# Get first argument as the file name
file_name=$1

# Check argument number. If it's not exactly 1, print error and exit
if [ $# -ne 1 ]
then
	echo 'You have to give 1 argument as file name'
	echo 'Program exits...'
	exit 1
fi

# If given file name is found
if [ -f $file_name ]; then
	# Prompt user to overwrite it or not
    read -r -p "File found! Do you want it to be overwritten? (y/n): " y_or_n
    # If no, exit program
    if [ "$y_or_n" = n ]; then
    	echo "Program exits. Bye !"
    	exit 0
    fi
    # If user didn't enter a valid choice, warn the user until he/she enters a valid choice.
    while [ "$y_or_n" != n ] && [ "$y_or_n" != y ]; do
    	read -r -p "Please enter y or n: " y_or_n
    	if [ "$y_or_n" = n ]; then
	    	echo "Program exits. Bye !"
	    	exit 0
	    # If yes, program will overwrite to the given file name.
	    elif [ "$y_or_n" = y ]; then
	    	break
    	fi
    done
fi

# These are the input files, we will get one lines from each of them.
inputs=("giris.txt" "gelisme.txt" "sonuc.txt")
declare -a lines

# Iterate through each input file
for i in `seq 0 $((${#inputs[@]} - 1))`
do
	# If any input files cannot be found, print error and exit
	if [ ! -f "${inputs[i]}" ]; then
    	echo "${inputs[i]} cannot found!"
    	echo "Program exits..."
    	exit 1
	fi
	
	# If file exists, read file line by line (IFS determines seperator which is newline (\n))
	declare -a temp_lines
	IFS=$'\n'
	counter=0
	# Read all file
	for next in `cat ${inputs[i]}`
	do
		 # Append each line to the temporary array
	     temp_lines[counter]="$next"
	     # Counts number of lines(paragraphs)
	     ((counter++))
	done
	# Choose a random element from the array - which corresponds to a line.
	# Inner operation returns a random number between 0 and (line_count-1).
	lines[i]=${temp_lines[$((RANDOM % counter))]}
done

# Output the chosen lines to the given file name. Each line seperated with \n\n for a good appearance
printf "%s\n\n" "${lines[@]}" > $file_name

echo "A random story is created to the file ${file_name}"

exit 0