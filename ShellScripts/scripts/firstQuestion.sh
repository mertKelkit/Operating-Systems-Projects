#!/bin/bash

# Mert Kelkit - 150115013
# Furkan Nakıp - 150115032

# Get first two arguments
first_arg=$1
second_arg=$2


# $# gives the number of arguments given by user. Ensure that user gave exactly 2 arguments.
if [ $# -ne 2 ]
then
	echo 'You have to give exactly 2 arguments'
	echo 'Program exits...'
	exit 1
fi

# Regular expression that matches only with positive integer
re='^[0-9]+$'
# If no match, print error
if ! [[ $second_arg =~ $re ]] ; then
	echo 'Second argument must be a nonnegative integer.'
	echo 'Program exits...'
	exit 1
fi

# First condition is first argument and second argument will have the same length
# Second contition is second argument will have length 1 independent from first argument's length
if [ ${#first_arg} -ne ${#second_arg} ]
then
	# If both conditions are not true, print error and exit
	if [ ${#second_arg} -ne 1 ]
	then
		echo 'Second parameter should be one or has the same length with the first parameter.'
		echo 'Program exits...'
		exit 1
	fi
fi

# Checks if each character of the first argument is a lowercase letter
limit=${#first_arg}
for (( i=0; i<limit; i++ ))
do
	# Even 1 character is not a lowercase letter, print error and exit
	if [[ ! ${first_arg:i:1} =~ [a-z] ]] ; then
	    echo "First argument must contain only lower case letters"
	    echo "Program exits..."
	    exit 1
	fi
done

# Declare array of ciphered word
declare -a ciphered

# If second argument has the same length with the first argument
if [ ${#second_arg} -ne 1 ]
	then
	# Iterate through all characters of first argument
	for i in `seq 0 $((${#first_arg} - 1))`
	do
		# Get the ascii value of current character	
		ascii_val=`echo -n ${first_arg:$i:1} | od -An -tuC`
		# Get the corresponding number which will be added to the character
		offset=${second_arg:$i:1}
		# Get ciphered ascii value
		new_ascii_val=$((ascii_val + offset))
		# If new ascii value comes after letter z ...
		if [ $new_ascii_val -ge 123 ]
		then
			# This operation finds actual ascii value for example: z + 1 = a, y + 5 = d
			new_ascii_val=$(( (new_ascii_val % 123) + 97 ))	
		fi
		# Convert ascii value to its hex value in order to convert hex value to corresponding character
		hex_ascii_val=`printf "%x\n" $new_ascii_val`
		# Append character to the array
		ciphered[i]=`echo -e "\x$hex_ascii_val"`
	done
# If second argument has the length 1
else
	# Iterate through every character in the first argument
	for i in `seq 0 $((${#first_arg} - 1))`
	do
		# Get ascii value of the current character
		ascii_val=`echo -n ${first_arg:$i:1} | od -An -tuC`
		# Add second argument to the current character's ascii value
		new_ascii_val=$((ascii_val + second_arg))
		if [ $new_ascii_val -ge 123 ]
		then
			# If new ascii value comes after z, this operation rewinds ascii values as exampled above
			new_ascii_val=$(( (new_ascii_val % 123) + 97 ))	
		fi
		# Get corresponding hex value
		hex_ascii_val=`printf "%x\n" $new_ascii_val`
		# Add ciphered character to the array
		ciphered[i]=`echo -e "\x$hex_ascii_val"`
	done
fi

# Print ciphered word
printf %s "Ciphered word: ${ciphered[@]}" $'\n'

exit 0
