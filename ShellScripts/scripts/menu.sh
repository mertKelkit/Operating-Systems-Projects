#!/bin/bash

# Mert Kelkit - 150115013
# Furkan Nakıp - 150115032

clear

# INFINITE LOOP
while :
do
	# Print menu
	echo "MAIN MENU"
	echo "1. Cipher words"
	echo "2. Create story"
	echo "3. Move files"
	echo "4. Convert hexadecimal"
	echo "5. Delete files"
	echo "6. Exit"
	# Get user's choice
	read -r -p "Enter your choice: " choice
	
	case "$choice" in
	# If user entered 1, get required arguments from the user. There is no argument control because our script will control the arguments.
	1)	read -r -p "Enter the first argument: " first_arg
		read -r -p "Enter the second argument: " second_arg
		echo
		# Run script with given arguments.
		./firstQuestion.sh $first_arg $second_arg
		echo
		echo
		;;
	# If user entered 2, get required file name argument. Script controls the argument
	2)	read -r -p "Enter the first argument: " first_arg
		echo
		# Run second question's script.
		./secondQuestion.sh $first_arg
		echo
		echo
		;;
	# If user entered 3...
	3)	prev_dir=$PWD				# Store current directory. Third script will change current working directory because of it is a writable file too.
		echo
		# Run script
		./thirdQuestion.sh
		# Change directory in order to continue executing scripts, because they are all writable files for users.
		cd ./writable
		# Print user that current working directory is changed.
		echo "*Current directory $prev_dir changed to $PWD"
		echo
		echo
		;;
	# If user entered 4, get the first argument. Its control will be done in the script.
	4)	read -r -p "Enter the argument: " first_arg
		echo
		echo
		# Run script with it's argument.
		./fourthQuestion.sh $first_arg
		echo
		echo
		;;
	# If user entered 5, get the first and second arguments. First one is mandatory, second one is optional.	
	5)	read -r -p "Enter the first argument: " first_arg
		# If it's empty, it means that user did not give the second argument.
		read -r -p "Enter the second argument(Optional, if you dont want to give, press enter): " second_arg
		echo
		# If second argument is not given
		if [ ${#second_arg} -eq 0 ]; then
			# Run script with only first argument
			./fifthQuestion.sh "$first_arg"
		# If second argument is given
		else
			# Run script with both arguments
			./fifthQuestion.sh "$first_arg" $second_arg					
		fi
		echo
		echo
		;;
	# If user wants to exit, print information message and exit the program	
	6)	echo "Program exits. Bye !"
		exit 0
		;;
	# If user did not enter a valid choice...
	*)	echo
		echo
		# Warn user to enter a number between 1 and 6.
	 	echo "Please enter a number between 1 and 6."
		echo
		echo
		;;
	esac
done
