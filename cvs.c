#include "mylogging.h"


#include <string.h>
#include <stdio.h>
#include <stdlib.h> /* for setenv() */
#include <malloc.h>
#if defined(_MSC_VER)
#include <direct.h>
#else
#include <unistd.h>
#endif

#include <ctype.h>

#include <process.h>

#if defined _WIN32 || defined __CYGWIN__
#include <windows.h>
#else
#error "asd"
#endif

#ifdef __cplusplus
extern "C" {
#endif

	LPWSTR* CommandLineToArgvExW(LPCWSTR lpCmdline, int* numargs, /* out */ LPCWSTR** lppOrigPositions);
	LPSTR * CommandLineToArgvExA(LPCSTR lpCmdline, int* numargs, /* out */ LPCSTR** lppOrigPositions);

#ifdef __cplusplus
}
#endif

#include "mylastheader.h"

static const char usage[] = 
"\n" ""
"\n" "CVS commands are:"
"\n" "        add          Add a new file/directory to the repository"
"\n" "        admin        Administration front end for rcs"
"\n" "        annotate     Show last revision where each line was modified"
"\n" "        checkout     Checkout sources for editing"
"\n" "        commit       Check files into the repository"
"\n" "        diff         Show differences between revisions"
"\n" "        edit         Get ready to edit a watched file"
"\n" "        editors      See who is editing a watched file"
"\n" "        export       Export sources from CVS, similar to checkout"
"\n" "        history      Show repository access history"
"\n" "        import       Import sources into CVS, using vendor branches"
"\n" "        init         Create a CVS repository if it doesn't exist"
"\n" "        log          Print out history information for files"
"\n" "        login        Prompt for password for authenticating server"
"\n" "        logout       Removes entry in .cvspass for remote repository"
"\n" "        pserver      Password server mode"
"\n" "        rannotate    Show last revision where each line of module was modified"
"\n" "        rdiff        Create 'patch' format diffs between releases"
"\n" "        release      Indicate that a Module is no longer in use"
"\n" "        remove       Remove an entry from the repository"
"\n" "        rlog         Print out history information for a module"
"\n" "        rtag         Add a symbolic tag to a module"
"\n" "        server       Server mode"
"\n" "        status       Display status information on checked out files"
"\n" "        tag          Add a symbolic tag to checked out version of files"
"\n" "        unedit       Undo an edit command"
"\n" "        update       Bring work tree in sync with repository"
"\n" "        version      Show current CVS version(s)"
"\n" "        watch        Set watches"
"\n" "        watchers     See who is watching a file"
"\n";

static void prinusage(const char *arg) {
	fprintf(stderr, "Unknown command: `%s'\n", arg);
	fputs(usage, stderr);
}

size_t dirname_len(const char *path)
{
	const char *p = path;
	size_t len = 0, sublen = 0;

	for (;; p++) {
		if (*p == 0) return len;
		sublen++;
		if (*p == '/' || *p == '\\') {
			len += sublen;
			sublen = 0;
		}
	}
}

#if defined _WIN32 || defined __CYGWIN__
#define PATSEP ';'
#else
#define PATSEP ':'
#endif

#undef UC
#define UC(c) (c & 0xdf)

char cwd[4096] = { PATSEP };

#define ENVPREF "PATH="

char *newpath, *newpath2;

#define XXX ".\\cvs.exe"

char cmdln[4096] = XXX;
char *path = cmdln + sizeof(XXX) - 1;
char exepath[MAX_PATH];

static BOOL WINAPI handler(DWORD fdwCtrlType) {
	printf("handler %d\n", fdwCtrlType);
	switch (fdwCtrlType)
	{
		// Handle the CTRL-C signal. 
	case CTRL_C_EVENT:
		printf("Ctrl-C event\n\n");
		Beep(750, 300);
		return(TRUE);

		// CTRL-CLOSE: confirm that the user wants to exit. 
	case CTRL_CLOSE_EVENT:
		Beep(600, 200);
		printf("Ctrl-Close event\n\n");
		return(TRUE);

		// Pass other signals to the next handler. 
	case CTRL_BREAK_EVENT:
		Beep(900, 200);
		printf("Ctrl-Break event\n\n");
		return FALSE;

	case CTRL_LOGOFF_EVENT:
		Beep(1000, 200);
		printf("Ctrl-Logoff event\n\n");
		return FALSE;

	case CTRL_SHUTDOWN_EVENT:
		Beep(750, 500);
		printf("Ctrl-Shutdown event\n\n");
		return FALSE;

	default:
		return FALSE;
	}
}

static void checkrls(int argc, char *argv[])
{
	int i;
	for (i = 1; i < argc; i++) {
		if (0 == strcmp(argv[i], "rls") || 0 == strcmp(argv[i], "ls")) {
			prinusage(argv[i]);
			exit(1);
		}
	}
}

static void excludepath()
{
	int i, j, cwdlen;
	const char *envpath;
	const char *rest;
	size_t dirlen, patlen;

	GetModuleFileNameA(NULL, cwd + 1, sizeof(cwd) - 1);
	dirlen = dirname_len(cwd);
	cwd[dirlen-1] = 0;

	for (cwdlen = 1;; cwdlen++) {
		cwd[cwdlen] = cwd[cwdlen] == '\\' ? '/' : toupper(cwd[cwdlen]);
		if (cwd[cwdlen] == 0) break;
	}
	cwd[cwdlen] = PATSEP;
	cwd[cwdlen + 1] = 0;

	envpath = getenv("PATH");
	patlen = strlen(envpath);
	newpath = malloc(patlen + 3);
	newpath2 = malloc(sizeof(ENVPREF)-1 + patlen + 3);

	newpath2 += (sizeof(ENVPREF)-2);

	newpath[0] = PATSEP;
	newpath2[0] = PATSEP;

	for(i = 0, j = 1;;i++, j++) {
		newpath[j] = envpath[i] == '\\' ? '/' : toupper(envpath[i]);
		newpath2[j] = envpath[i];
		if (envpath[i] == PATSEP || envpath[i] == 0) {
			if (newpath[j - 1] == '/') {
				j--;
				newpath[j] = envpath[i];
				newpath2[j] = envpath[i];
			}
			if (envpath[i] == 0) break;
		}
	}
	newpath[j] = PATSEP;
	newpath[j + 1] = 0;
	newpath2[j] = PATSEP;
	newpath2[j + 1] = 0;

	rest = newpath;

	for(;;) {
		rest = strstr(rest, cwd);
		if (!rest) break;
		strcpy(rest, rest + cwdlen);
		strcpy(newpath2 + (rest - newpath), newpath2 + (rest - newpath) + cwdlen);
	}

	newpath2[strlen(newpath2) - 1] = 0;
	newpath2 -= (sizeof(ENVPREF)-2);
	memcpy(newpath2, ENVPREF, sizeof(ENVPREF)-1);

	putenv(newpath2);
	newpath2 = NULL;
	free(newpath);
}

static int real_main(int argc, CHAR* argv[], LPCSTR origPositions[]) {
	PROCESS_INFORMATION pifo;
	STARTUPINFO sifo;
	DWORD exitCode = 1;
	DWORD dw;
	char ** newargs;
	int i;

	/* shift cmdln */
	if (argc > 1) {
		*(path++) = ' ';
		strcpy(path, origPositions[1]);
	}
	else {
		*path = 0;
	}

	newargs = malloc((argc + 1) * sizeof(char*));

	for (i = 1; i < argc; i++) {
		newargs[i] = origPositions[i];
		newargs[i][-1] = 0;
	}
	newargs[i] = NULL;
	newargs[0] = "cvs.exe";

	ZeroMemory(&sifo, sizeof(sifo));

	if (!SetConsoleCtrlHandler(handler, TRUE)) {
		return 1;
	}

	exitCode = spawnvp(_P_WAIT, newargs[0], newargs);
	if (exitCode == -1) {
		perror(newargs[0]);
		return 1;
	}



	if (FALSE) {
		if (CreateProcessA(NULL, "git.exe", NULL, NULL, TRUE, 0, NULL, NULL, &sifo, &pifo)) {
			WaitForSingleObject(pifo.hProcess, INFINITE);
			GetExitCodeProcess(pifo.hProcess, &exitCode);
		}
		else {
			dw = GetLastError();
			pWin32Error(ERR, "%s", cmdln);
			dw = 0;
		}

	}
	return exitCode;
}

int main() {
	//DebugBreak();

	int rslt;
	LPCSTR* origPositions;
	int argc;
	CHAR**argv;

	LPSTR cmdln = GetCommandLineA();

	/* use a predictable CommandLine parser */
	argv = CommandLineToArgvExA(cmdln, &argc, &origPositions);

	checkrls(argc, argv);

	excludepath();

	rslt = real_main(argc, argv, origPositions);
	LocalFree(argv);
	return rslt;
}

