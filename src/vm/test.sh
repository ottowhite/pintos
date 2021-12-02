make clean && make check -j16 | grep -w "FAIL" | uniq -u | sort > failures.txt

