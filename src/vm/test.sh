make clean && make check -j16 | grep "\<FAIL tests\>\|\<pass tests\>\|\<Warning\>" | sort -u > failures.txt
cat failures.txt | grep "\<FAIL tests\>\|\<Warning\>"
