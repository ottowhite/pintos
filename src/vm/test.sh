make clean && make check -j45 | grep "\<FAIL tests\>\|\<pass tests\>\|\<Warning\>" | sort -u > failures.txt
cat failures.txt | grep "\<FAIL tests\>\|\<Warning\>"
