tests:
	@passed=0; failed=0;							\
	mkdir -p bin;								\
	mkdir -p log;								\
	cd mains; 								\
	for f in *; do 								\
		printf "Test $$f\r\t\t\t "; 					\
		../testbench/$$f.sh > ../log/$$f.log;				\
		diff ../desired/$$f.log ../log/$$f.log &> /dev/null;		\
		if [ $$? -eq 0 ]; then						\
			echo OK;						\
			let passed++;						\
		else								\
			echo FAILED; 						\
			let failed++;						\
		fi;								\
	done;									\
	if [ $$failed -eq 0 ]; then						\
		printf "\nALL TESTS PASSED!\n\n";				\
	else									\
		printf "\n%d passed tests, %d failed\n\n" $$passed $$failed;	\
	fi

clean:
	rm -rf bin
	rm -rf log

.PHONY:	tests
