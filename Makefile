all: profile global_profile

CC=gcc

profile: profile.c
global_profile: global_profile.c

clean:
	@rm -f profile
	@rm -f global_profile
