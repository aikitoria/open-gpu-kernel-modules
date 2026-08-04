CC ?= cc

POLICY_TEST := bar1_p2p_policy_test
POLICY_HEADER := ../src/nvidia/src/kernel/gpu/bus/arch/turing/bar1_p2p_policy.h
POLICY_TEST_CFLAGS := -std=c11 -Wall -Wextra -Werror

.PHONY: all check clean

all: $(POLICY_TEST)

$(POLICY_TEST): $(POLICY_TEST).c $(POLICY_HEADER)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(POLICY_TEST_CFLAGS) -o $@ $<

check: $(POLICY_TEST)
	./$(POLICY_TEST)

clean:
	$(RM) $(POLICY_TEST)
