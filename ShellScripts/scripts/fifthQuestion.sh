#!/bin/bash

# Get arguments. First argument (wildcard) is mandatory, second (path name) is optional.
first_arg=$1
second_arg=$2

# If there is not 1 argument...
if [ "$#" -ne 1 ]; then
	# ...also there is not 2 arguments...
	if [ "$#" -ne 2 ]; then
		# Print error message and exit. There must be one or two arguments.
		echo "You must provide a wildcard as first argument. Pathname argument is optional."
		echo "Program exits..."
		exit 1
	fi
	# If second argument is given, but this path name doesn't exist in current working directory, print error message and exit.
	if [ ! -d $second_arg ]; then
		echo "No directory found named $second_arg"
		echo "Program exits..."
		exit 1
	fi
fi

# Wildcards in bash scripting language.
wildcards=( '*' '.' '?' '|' ']' '[' )
wildcard_counter=0
# Iterate through all wildcard characters
for wildcard in "${wildcards[@]}"
do
  if [[ $first_arg == *"${wildcard}"* ]]; then
  	# Increment counter if there is a wildcard found in the first argument.
  	((wildcard_counter++))
  fi
done

# If counter is never incremented...
if [ "$wildcard_counter" -eq 0 ]; then
	  # Print corresponding error message and exit. First argument doesn't include any wildcards.
	  echo "There is no wildcard found in the first argument."
	  echo "Program exits..."
	  exit 1
fi

# If there is only one argument - just wildcard -, look for matching files with find command in the current working directory.
# -maxdepth 1 means just look for current working directory.
# -type f means just look for files
# -name "$first_arg" means look for files which their names matches with the wildcard argument. 
if [ "$#" -eq 1 ]; then
	found_files=($(find . -maxdepth 1 -type f -name "$first_arg"))
# If second argument is given...
else
	# Look for every file matches with wildcard in the "path_name" directory.
	#There is no -maxdepth option because we want to look for every file in its subdirectories.
	found_files=($(find ./$second_arg -type f -name "$first_arg"))
fi

# If no file is found, print error message and exit
if [ ${#found_files[@]} -eq 0 ]; then
	echo "No files found matching with wildcard."
	exit 0
fi

# Iterate through every found files.
for f in "${found_files[@]}"
do
	# Prompt user to delete current file or not
    read -r -p "Do you want to delete $f? (y/n): " y_or_n
    # If yes, remove current file
    if [ "$y_or_n" = y ]; then
    	rm $f
    	echo "$f removed."
    # If no, continue with other files if exist.
    elif [ "$y_or_n" = n ]; then
    	continue
    # If user doesn't enter a valid option...
    else
    	# Until user enters a valid option...
    	while [ "$y_or_n" != n ] && [ "$y_or_n" != y ]
    	do
    		# Warn the user and get input again
    		read -r -p "Please enter y or n: " y_or_n
    		# If yes, remove file
    		if [ "$y_or_n" = y ]; then
    			rm $f
    			echo "$f removed."
    		# If no, continue with other files if exist.
   	 		elif [ "$y_or_n" = n ]; then
    			break
    		fi
		done
    fi
done

exit 0
