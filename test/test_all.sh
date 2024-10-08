#!/bin/sh

# set -x

main_result=0

test_result() {
	if [ $1 = 0 ]; then
		echo ok
	else
		echo failed
		main_result=1
	fi
}

test_result_fail() {
	if [ $1 = 0 ]; then
		echo error: did not fail
		main_result=1
	else
		echo fail expected
	fi
}

# run the test workspaces
test_workspaces() {
	for i in /home/john/GIT/nip2/test/workspaces/*.ws; do
		base=$(basename $i)
		echo -n "testing $base ... "
		/home/john/GIT/nip2/src/nip2 --prefix=/home/john/GIT/nip2 --test "$i"
		result=$?

		# test_fail.ws is supposed to fail
		if [ x"$base" = x"test_fail.ws" ]; then
			test_result_fail $result
		else
			test_result $result
		fi
	done
}

# run the test defs
test_defs() {
	for i in /home/john/GIT/nip2/test/workspaces/*.def; do
		base=$(basename $i)
		echo -n "testing $base ... "
		/home/john/GIT/nip2/src/nip2 --prefix=/home/john/GIT/nip2 --test $i
		test_result $?
	done
}

# load all the example workspaces too
test_examples() {
	for i in /home/john/GIT/nip2/share/nip2/data/examples/*/*.ws; do
		base=$(basename $i)

		# have to skip these two, they use a non-free plugin
		if test x"$base" = x"framing.ws" ; then
			continue
		fi
		if test x"$base" = x"registering.ws" ; then
			continue
		fi

		echo -n "testing $base ... "
		/home/john/GIT/nip2/src/nip2 --prefix=/home/john/GIT/nip2 --test $i 
		test_result $?
	done
}

test_workspaces
test_defs
test_examples

echo "repeating tests with the vectorising system disabled"

export IM_NOVECTOR=1
test_workspaces
test_defs
test_examples

exit $main_result
