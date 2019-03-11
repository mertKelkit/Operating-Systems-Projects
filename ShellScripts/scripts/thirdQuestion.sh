#!/bin/bash

# Mert Kelkit - 150115013
# Furkan Nakıp - 150115032

# Create directory "writable" if it's absent.
if [ ! -d "writable" ]; then
	mkdir writable
fi

# Array to store all writable files by user
declare -a writable_files

# Execute a find command which with a maxdepth 1 - means that look for only current directory. Type f means look for only files, not directories. 
# perm -u+w means look for files which are writable for users. print0 for null escape characters
# This loop iterates through each line of this "find" command's response.
while IFS=  read -r -d $'\0'; do
	# Append each line to the writable files array
	writable_files+=("$REPLY")
done < <(find . -maxdepth 1 -type f -perm -u+w -print0)

# Get the number of files
no_of_files=${#writable_files[@]}

# Iterate through all file names
for i in `seq 0 $((no_of_files - 1))`
do
	# Move given file to the given directory
	# If a file name contains space...
	if [ `echo ${writable_files[i]} | grep \  | wc -l` -ne 0 ]; then
		# File name must written between double quotes in order to prevent errors.	
		mv "${writable_files[i]}" writable
	else
		mv ${writable_files[i]} writable
	fi
done

# Information message
echo "$no_of_files files are moved to the writable directory."

exit 0