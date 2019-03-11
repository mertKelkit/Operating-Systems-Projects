#!/bin/bash

# Mert Kelkit - 150115013
# Furkan Nakıp - 150115032

# This is a function to check an integer's primeness
# If integer is prime, returns 1
# If integer is not prime, returns 0
function isprime {
	# Get the first argument - number to be checked
	local var=$1
	
	# If number is 1, 1 is not prime by definition
	if [ "$var" -eq 1 ]; then
		return 0
	fi
	
	# If number is 2, 2 is prime by definition
	if [ "$var" -eq 2 ]; then
		return 1
	fi

	# Get the possible maximum divider of the number. Square root of the number is the largest possible divider from number theory
	maximum_divider=$(echo "sqrt($var)" | bc -l)
	# Get the ceil of the square rooted number - most probably it'll be a floating number
	maximum_divider=${maximum_divider%.*}
	
	# Start possible dividers with 2, increment each step, stop when it reaches to the maximum possible driver
	for (( j=2; j<=$maximum_divider; j++ )); do
		# If number is divided by divider, it means it's a prime number. Return 1
		if [ $(( $var % $j )) -eq 0 ]; then
			return 0
		fi
	done
	# If none of 
	return 1
}

# Get the first argument
number=$1

# If there is not only 1 argument, print error and exit
if [ $# -ne 1 ]
then
	echo 'You have to give exactly 1 integer argument'
	echo 'Program exits...'
	exit 1
fi

# This is the regular expression, matches with only positive integers
re='^[0-9]+$'
# If there is no match(argument is not positive integer), print error and exit
if ! [[ $number =~ $re ]] ; then
	echo 'You must enter a nonnegative integer.'
	echo 'Program exits...'
	exit 1
fi

# Test all numbers which are less than argument
for (( i=1; i<$number; i++ )); do
	isprime $i
	# Get the last returning value from last function call, which is isprime
	is_prime=$?
	# If current number (i) is prime, print it's hexadecimal representation
	if [ "$is_prime" -eq 1 ]; then
		hex_val=`printf "%X\n" $i`
   	    echo "Hexadecimal of $i is $hex_val"
	fi
done


exit 0