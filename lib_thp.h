#define _LARGEFILE64_SOURCE

int update_thp_usage(struct process *proc)
{
	FILE *smaps_file = NULL;
	char line[LINELENGTH], smaps_name[FILENAMELENGTH];
	unsigned long anon_size = 0, anon_thp = 0;
	int count_idle_pages = 0, prev = -1, pid;
	bool error = false;

	pid = proc->pid;
	/* get smaps file for the given pid */
	sprintf(smaps_name, "/proc/%d/smaps", pid);

	smaps_file = fopen(smaps_name, "r");
	if (smaps_file == NULL) {
		error = true;
		goto exit;
	}

	/* read process map line by line */
	while (fgets(line, LINELENGTH, smaps_file) != NULL) {
		char region[LINELENGTH];
		unsigned long size;
		int n, num_pages;

		/* Skip everything other than anon memory mappings */
		if (!strstr(line, "Anon"))
			continue;

		n = sscanf(line, "%s %ld", region, &size);
		if (n != 2) {
			printf ("Could not read addresses from the line: %d\n", n);
			continue;
		}
		if (strstr(region, "Anonymous"))
			anon_size += size;

		if (strstr(region, "AnonHugePages"))
			anon_thp += size;
	}
	proc->anon_size = anon_size;
	proc->anon_thp = anon_thp;
exit:
	if (smaps_file)
		fclose(smaps_file);
	if (error)
		return -1;
	else
		return 0;
}

/*
int main(int argc, char *argv[])
{
	int pid;

	pid = atoi(argv[1]);
	get_pid_thp_usage(pid);
}
*/
