all: profile global_profile

profile: profile.c
	gcc -o profile profile.c
	gcc -o global_profile global_profile.c

clean:
	@rm -f profile
	@rm -f global_profile
