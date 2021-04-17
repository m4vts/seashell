#include <unistd.h>
#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h> //termios, TCSANOW, ECHO, ICANON
#include <string.h>
#include <stdbool.h>
#include <errno.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#define PATH_SIZE 100
#define PIPE_SIZE 2
#define READ_END 0
#define WRITE_END 1

const char *sysname = "seashell";

enum return_codes
{
	SUCCESS = 0,
	EXIT = 1,
	UNKNOWN = 2,
};
struct command_t
{
	char *name;
	bool background;
	bool auto_complete;
	int arg_count;
	char **args;
	char *redirects[3];		// in/out redirection
	struct command_t *next; // for piping
};
/**
 * Prints a command struct
 * @param struct command_t *
 */
void print_command(struct command_t *command)
{
	int i = 0;
	printf("Command: <%s>\n", command->name);
	printf("\tIs Background: %s\n", command->background ? "yes" : "no");
	printf("\tNeeds Auto-complete: %s\n", command->auto_complete ? "yes" : "no");
	printf("\tRedirects:\n");
	for (i = 0; i < 3; i++)
		printf("\t\t%d: %s\n", i, command->redirects[i] ? command->redirects[i] : "N/A");
	printf("\tArguments (%d):\n", command->arg_count);
	for (i = 0; i < command->arg_count; ++i)
		printf("\t\tArg %d: %s\n", i, command->args[i]);
	if (command->next)
	{
		printf("\tPiped to:\n");
		print_command(command->next);
	}
}
/**
 * Release allocated memory of a command
 * @param  command [description]
 * @return         [description]
 */
int free_command(struct command_t *command)
{
	if (command->arg_count)
	{
		for (int i = 0; i < command->arg_count; ++i)
			free(command->args[i]);
		free(command->args);
	}
	for (int i = 0; i < 3; ++i)
		if (command->redirects[i])
			free(command->redirects[i]);
	if (command->next)
	{
		free_command(command->next);
		command->next = NULL;
	}
	free(command->name);
	free(command);
	return 0;
}
/**
 * Show the command prompt
 * @return [description]
 */
int show_prompt()
{
	char cwd[1024], hostname[1024];
	gethostname(hostname, sizeof(hostname));
	getcwd(cwd, sizeof(cwd));
	printf("%s@%s:%s %s$ ", getenv("USER"), hostname, cwd, sysname);
	return 0;
}
/**
 * Parse a command string into a command struct
 * @param  buf     [description]
 * @param  command [description]
 * @return         0
 */
int parse_command(char *buf, struct command_t *command)
{
	const char *splitters = " \t"; // split at whitespace
	int index, len;
	len = strlen(buf);
	while (len > 0 && strchr(splitters, buf[0]) != NULL) // trim left whitespace
	{
		buf++;
		len--;
	}
	while (len > 0 && strchr(splitters, buf[len - 1]) != NULL)
		buf[--len] = 0; // trim right whitespace

	if (len > 0 && buf[len - 1] == '?') // auto-complete
		command->auto_complete = true;
	if (len > 0 && buf[len - 1] == '&') // background
		command->background = true;

	char *pch = strtok(buf, splitters);
	command->name = (char *)malloc(strlen(pch) + 1);
	if (pch == NULL)
		command->name[0] = 0;
	else
		strcpy(command->name, pch);

	command->args = (char **)malloc(sizeof(char *));

	int redirect_index;
	int arg_index = 0;
	char temp_buf[1024], *arg;
	while (1)
	{
		// tokenize input on splitters
		pch = strtok(NULL, splitters);
		if (!pch)
			break;
		arg = temp_buf;
		strcpy(arg, pch);
		len = strlen(arg);

		if (len == 0)
			continue;										 // empty arg, go for next
		while (len > 0 && strchr(splitters, arg[0]) != NULL) // trim left whitespace
		{
			arg++;
			len--;
		}
		while (len > 0 && strchr(splitters, arg[len - 1]) != NULL)
			arg[--len] = 0; // trim right whitespace
		if (len == 0)
			continue; // empty arg, go for next

		// piping to another command
		if (strcmp(arg, "|") == 0)
		{
			struct command_t *c = malloc(sizeof(struct command_t));
			int l = strlen(pch);
			pch[l] = splitters[0]; // restore strtok termination
			index = 1;
			while (pch[index] == ' ' || pch[index] == '\t')
				index++; // skip whitespaces

			parse_command(pch + index, c);
			pch[l] = 0; // put back strtok termination
			command->next = c;
			continue;
		}

		// background process
		if (strcmp(arg, "&") == 0)
			continue; // handled before

		// handle input redirection
		redirect_index = -1;
		if (arg[0] == '<')
			redirect_index = 0;
		if (arg[0] == '>')
		{
			if (len > 1 && arg[1] == '>')
			{
				redirect_index = 2;
				arg++;
				len--;
			}
			else
				redirect_index = 1;
		}
		if (redirect_index != -1)
		{
			command->redirects[redirect_index] = malloc(len);
			strcpy(command->redirects[redirect_index], arg + 1);
			continue;
		}

		// normal arguments
		if (len > 2 && ((arg[0] == '"' && arg[len - 1] == '"') || (arg[0] == '\'' && arg[len - 1] == '\''))) // quote wrapped arg
		{
			arg[--len] = 0;
			arg++;
		}
		command->args = (char **)realloc(command->args, sizeof(char *) * (arg_index + 1));
		command->args[arg_index] = (char *)malloc(len + 1);
		strcpy(command->args[arg_index++], arg);
	}
	command->arg_count = arg_index;
	return 0;
}
void prompt_backspace()
{
	putchar(8);	  // go back 1
	putchar(' '); // write empty over
	putchar(8);	  // go back 1 again
}
/**
 * Prompt a command from the user
 * @param  buf      [description]
 * @param  buf_size [description]
 * @return          [description]
 */
int prompt(struct command_t *command)
{
	int index = 0;
	char c;
	char buf[4096];
	static char oldbuf[4096];

	// tcgetattr gets the parameters of the current terminal
	// STDIN_FILENO will tell tcgetattr that it should write the settings
	// of stdin to oldt
	static struct termios backup_termios, new_termios;
	tcgetattr(STDIN_FILENO, &backup_termios);
	new_termios = backup_termios;
	// ICANON normally takes care that one line at a time will be processed
	// that means it will return if it sees a "\n" or an EOF or an EOL
	new_termios.c_lflag &= ~(ICANON | ECHO); // Also disable automatic echo. We manually echo each char.
	// Those new settings will be set to STDIN
	// TCSANOW tells tcsetattr to change attributes immediately.
	tcsetattr(STDIN_FILENO, TCSANOW, &new_termios);

	//FIXME: backspace is applied before printing chars
	show_prompt();
	int multicode_state = 0;
	buf[0] = 0;
	while (1)
	{
		c = getchar();
		// printf("Keycode: %u\n", c); // DEBUG: uncomment for debugging

		if (c == 9) // handle tab
		{
			buf[index++] = '?'; // autocomplete
			break;
		}

		if (c == 127) // handle backspace
		{
			if (index > 0)
			{
				prompt_backspace();
				index--;
			}
			continue;
		}
		if (c == 27 && multicode_state == 0) // handle multi-code keys
		{
			multicode_state = 1;
			continue;
		}
		if (c == 91 && multicode_state == 1)
		{
			multicode_state = 2;
			continue;
		}
		if (c == 65 && multicode_state == 2) // up arrow
		{
			int i;
			while (index > 0)
			{
				prompt_backspace();
				index--;
			}
			for (i = 0; oldbuf[i]; ++i)
			{
				putchar(oldbuf[i]);
				buf[i] = oldbuf[i];
			}
			index = i;
			continue;
		}
		else
			multicode_state = 0;

		putchar(c); // echo the character
		buf[index++] = c;
		if (index >= sizeof(buf) - 1)
			break;
		if (c == '\n') // enter key
			break;
		if (c == 4) // Ctrl+D
			return EXIT;
	}
	if (index > 0 && buf[index - 1] == '\n') // trim newline from the end
		index--;
	buf[index++] = 0; // null terminate string

	strcpy(oldbuf, buf);

	parse_command(buf, command);

	// print_command(command); // DEBUG: uncomment for debugging

	// restore the old settings
	tcsetattr(STDIN_FILENO, TCSANOW, &backup_termios);
	return SUCCESS;
}
int process_command(struct command_t *command);
int main()
{
	while (1)
	{
		struct command_t *command = malloc(sizeof(struct command_t));
		memset(command, 0, sizeof(struct command_t)); // set all bytes to 0

		int code;
		code = prompt(command);
		if (code == EXIT)
			break;

		code = process_command(command);
		if (code == EXIT)
			break;

		free_command(command);
	}

	printf("\n");
	return 0;
}

void add_spaces(char *dest, int num_of_spaces)
{
	sprintf(dest, "%s%*s", dest, num_of_spaces, "");
}

void blue()
{
	printf("\033[;34m");
}
void green()
{
	printf("\033[;32m");
}

void red()
{
	printf("\033[1;31m");
}

void reset()
{
	printf("\033[0m");
}

char *getTheithLine(FILE *f, int ithLine, char filePath[PATH_SIZE])
{
	char *tempPath;
	long sz;
	int lineCounter = 1;
	char *tokenizer;

	f = fopen(filePath, "r");
	if (f < 0)
	{
		printf("The file could not be opened.\n");
		exit(1);
	}

	fseek(f, 0L, SEEK_END);
	sz = ftell(f);
	fseek(f, 0L, SEEK_SET);

	tempPath = malloc(sz);

	if (tempPath)
	{
		fread(tempPath, 1, sz, f); //tempPath stores the file
	}

	tokenizer = strtok(tempPath, "\n");
	while (tokenizer != NULL && lineCounter < ithLine)
	{
		tokenizer = strtok(NULL, "\n");
		lineCounter++;
	}
	fclose(f);
	return tokenizer;
}

int max(int num1, int num2)
{
	return (num1 > num2) ? num1 : num2;
}

int min(int num1, int num2)
{
	return (num1 > num2) ? num2 : num1;
}

int howManyLines(FILE *f, char filePath[PATH_SIZE])
{
	char *tempPath;
	long sz;
	int lineCounter = 0;
	char *tokenizer;
	f = fopen(filePath, "r");
	if (f < 0)
	{
		printf("The file could not be opened.\n");
		exit(1);
	}

	fseek(f, 0L, SEEK_END);
	sz = ftell(f);
	fseek(f, 0L, SEEK_SET);

	tempPath = malloc(sz);

	if (tempPath)
	{
		fread(tempPath, 1, sz, f); //tempPath stores the file
	}

	tokenizer = strtok(tempPath, "\n");
	while (tokenizer != NULL)
	{
		lineCounter++;
		tokenizer = strtok(NULL, "\n");
	}
	fclose(f);
	return lineCounter;
}

int process_command(struct command_t *command)
{
	int r;
	if (strcmp(command->name, "") == 0)
		return SUCCESS;

	if (strcmp(command->name, "exit") == 0)
		return EXIT;

	if (strcmp(command->name, "cd") == 0)
	{
		if (command->arg_count > 0)
		{
			r = chdir(command->args[0]);
			if (r == -1)
				printf("-%s: %s: %s\n", sysname, command->name, strerror(errno));
			return SUCCESS;
		}
	}

	int p[PIPE_SIZE]; //pipe
	char message[PATH_SIZE];

	pid_t pid;

	if (pipe(p) == -1)
	{ //pipe p is created
		perror("Pipe failed!");
		return 1;
	}

	pid = fork();

	if (pid == 0) // child
	{
		close(p[READ_END]);

		/// This shows how to do exec with environ (but is not available on MacOs)
		// extern char** environ; // environment variables
		// execvpe(command->name, command->args, environ); // exec+args+path+environ

		/// This shows how to do exec with auto-path resolve
		// add a NULL argument to the end of args, and the name to the beginning
		// as required by exec

		// increase args size by 2
		command->args = (char **)realloc(
			command->args, sizeof(char *) * (command->arg_count += 2));

		// shift everything forward by 1
		for (int i = command->arg_count - 2; i > 0; --i)
			command->args[i] = command->args[i - 1];

		// set args[0] as a copy of name
		command->args[0] = strdup(command->name);
		// set args[arg_count-1] (last) to NULL
		command->args[command->arg_count - 1] = NULL;

		//execvp(command->name, command->args); // exec+args+path

		/// TODO: do your own exec with path resolving using execv()

		//PART1
		const char *s = getenv("PATH");
		char path[PATH_SIZE] = "/bin/";
		strcat(path, command->name);
		execv(path, command->args);

		//PART2
		if (strcmp(command->name, "shortdir") == 0)
		{
			char *shortName; //short name that is going to be replaced with the whole path.
			char *tokenizer;
			char *cwd; //current path
			char pathName[PATH_SIZE];
			char buff[PATH_SIZE + 1];
			char *login = getlogin(); // get the user name
			char home[PATH_SIZE] = "/home/";
			char home2[PATH_SIZE] = "/home/";
			
			long sz; //file size
			char *tempPath;
			
			char tempFile[PATH_SIZE];
			cwd = getcwd(buff, PATH_SIZE + 1); //current path
			
			//create a file to store and then use it, lastly do not forget to delete it
			FILE *fileptr;

			//create the file in the direction of /home/username/part2.txt
			strcat(home, login);
			strcat(home, "/");
			strcat(home, "part2.txt");

			if (strcmp(command->args[1], "set") == 0)
			{

				shortName = command->args[2]; //short name
				fileptr = fopen(home, "a");

				if (fileptr < 0)
				{
					printf("The file part2 could not be opened.\n");
					exit(1);
				}
				//read the file until the end
				fseek(fileptr, 0L, SEEK_END);
				sz = ftell(fileptr);
				fseek(fileptr, 0L, SEEK_SET);

				tempPath = malloc(sz);

				if (tempPath)
				{
					fread(tempPath, 1, sz, fileptr); //tempPath stores the read file
				}

				tokenizer = strtok(tempPath, " ");
				while (tokenizer != NULL &&
					   strcmp(tokenizer, shortName) != 0)
				{

					tokenizer = strtok(NULL, " ");
				}
				fprintf(fileptr, "%s %s ", shortName, cwd); //store the shortname and corresponding path in the file separated by space
			}
			else if (strcmp(command->args[1], "jump") == 0)
			{
				shortName = command->args[2]; //short name
				fileptr = fopen(home, "r");
				if (fileptr < 0)
				{
					printf("The file part2 could not be opened.\n");
					exit(1);
				}

				//read the file until the end
				fseek(fileptr, 0L, SEEK_END);
				sz = ftell(fileptr);
				fseek(fileptr, 0L, SEEK_SET);

				if (sz != 0)
				{
					tempPath = malloc(sz);

					if (tempPath)
					{
						fread(tempPath, 1, sz, fileptr); //tempPath stores the read file
					}

					tokenizer = strtok(tempPath, " ");
					while (tokenizer != NULL &&
						   strcmp(tokenizer, shortName) != 0) //until reaching the short name
					{
						tokenizer = strtok(NULL, " ");
					}
					tokenizer = strtok(NULL, " "); //take the corresponding path, not the short name
					strcpy(message, tokenizer);	   //use pipe to send the path
				}
			}
			else if (strcmp(command->args[1], "del") == 0)
			{
				int found = 0; //states whether the short name that the user wants to delete is found or not

				shortName = command->args[2];
				fileptr = fopen(home, "r");
				if (fileptr < 0)
				{
					printf("The file part2 could not be opened.\n");
					exit(1);
				}

				//read the file until the end
				fseek(fileptr, 0L, SEEK_END);
				sz = ftell(fileptr);
				fseek(fileptr, 0L, SEEK_SET);
				if (sz != 0)
				{
					tempPath = malloc(sz);

					if (tempPath)
					{
						fread(tempPath, 1, sz, fileptr); //tempPath stores the read file
					}
					tokenizer = strtok(tempPath, " ");
					int i;
					for (i = 0; i < strlen(tempFile); i++)
					{
						tempFile[i] = '\0';
					}
					while (tokenizer != NULL)
					{
						if (strcmp(tokenizer, shortName) != 0)
						{ //if the short name is not found
							if (found == 0)
							{
								strcat(tempFile, tokenizer); //temp the short name that is not deleted
								strcat(tempFile, " ");		 //put a space between each entry
								i = i + strlen(tokenizer) + 1;
							}
							else
							{ //if the previous loop we found the short name make the found 0 again
								found = 0;
							}
						}
						else
						{			   //if the short name is found
							found = 1; //state that we find the short name so that in the other loop the corresponding path is not included in tempFile
						}

						tokenizer = strtok(NULL, " ");
					}
					tempFile[i] = '\0';
					strcat(tempFile, "\0");

					fclose(fileptr);
					fileptr = fopen(home, "w"); //erase all entries of the file

					fprintf(fileptr, "%s", tempFile); //write tempFile to the file without the deleted parts
					int doesExist = 0;				  //check whether the written part includes any path
					fseek(fileptr, 0L, SEEK_END);
					sz = ftell(fileptr);
					fseek(fileptr, 0L, SEEK_SET);
					for (int i = 0; i < sz; i++)
					{
						char c = tempFile[i];
						if (c == '/')
						{
							doesExist = 1;
							i = sz;
						}
					}
					if (!doesExist)
					{ //if the file is empty
						fclose(fileptr);
						fileptr = fopen(home, "w"); //make sure that the file is completely empty (no non-alphabetic character exists)
					}
				}
			}
			else if (strcmp(command->args[1], "clear") == 0)
			{
				fileptr = fopen(home, "w"); //it erases contents in the file and treats file as an empty new file
			}
			else if (strcmp(command->args[1], "list") == 0)
			{

				fileptr = fopen(home, "r");
				if (fileptr < 0)
				{
					printf("The file part2 could not be opened.\n");
					exit(1);
				}
				fseek(fileptr, 0L, SEEK_END);
				sz = ftell(fileptr);
				fseek(fileptr, 0L, SEEK_SET);

				if (sz == 0)
				{
					printf("List is empty!\n");
				}
				else
				{
					tempPath = malloc(sz);

					if (tempPath)
					{
						fread(tempPath, 1, sz, fileptr); //tempPath stores the read file
					}

					char *tempTokenizer;
					tokenizer = strtok(tempPath, " "); //tokenize for each space
					while (tokenizer != NULL)
					{

						tempTokenizer = tokenizer;
						tokenizer = strtok(NULL, " ");

						if (tempTokenizer != NULL && tokenizer != NULL && strchr(tokenizer, '/'))
						{

							printf("%s -> ", tempTokenizer);
							if (strchr(tokenizer, '/'))
								printf("%s\n", tokenizer);
						}
						tokenizer = strtok(NULL, " ");
					}
				}
			}

			fclose(fileptr);
		}
		write(p[WRITE_END], message, sizeof(message) + 1); //the path is sent from shortdir jump part
		wait(NULL);
		close(p[WRITE_END]);

		//PART3
		if (strcmp(command->name, "highlight") == 0)
		{
			//develop logic to search all occurences of the word in the file and color them

			//take a line, tokenize, go through each token and concatenate, if the word is found add red/blue before and reset after. If found, print
			char buffer[500];
			FILE *infile;
			char *filename = command->args[3];
			char *word_to_color = command->args[1];
			char *color = command->args[2];
			int print_flag = 0;
			char line_to_print[500];
			//open the file
			if ((infile = fopen(filename, "r")) == NULL)
			{
				printf("The file %s could not be opened.\n", filename);
				exit(1);
			}

			while (!feof(infile)) //while not EOF
			{
				if (fgets(buffer, 500, infile) != NULL)
				{
					char *tokenPtr;
					// initialize the string tokenizer by passing in buffer
					// capture the return value in tokenPtr.
					tokenPtr = strtok(buffer, " ,\n");
					// check to make sure the previous call found a token.
					while (tokenPtr != NULL)
					{
						if (strcasecmp(tokenPtr, word_to_color) == 0)
						{

							if (strcasecmp(color, "r") == 0)
							{
								print_flag = 1;
								printf("%s", line_to_print);
								red();
								printf("%s", tokenPtr);
								reset();
								printf("%s", " ");
							}
							else if (strcasecmp(color, "g") == 0)
							{
								print_flag = 1;
								printf("%s", line_to_print);
								green();
								printf("%s", tokenPtr);
								reset();
								printf("%s", " ");
							}
							else if (strcasecmp(color, "b") == 0)
							{
								print_flag = 1;
								printf("%s", line_to_print);
								blue();
								printf("%s", tokenPtr);
								reset();
								printf("%s", " ");
							}
						}
						else if (print_flag == 1)
						{
							printf("%s ", tokenPtr);
						}
						else
						{
							strcat(line_to_print, tokenPtr);
							strcat(line_to_print, " ");
						}

						// for all subsequent calls, use NULL as the first argument
						tokenPtr = strtok(NULL, " ,\n");
					}
				}
				print_flag = 0;
				printf("%s", "\n");
			}
			fclose(infile);
		}

		//PART4
		if (strcmp(command->name, "goodMorning") == 0)
		{

			//take the commands
			char *alarm_time = command->args[1];
			char *music_file = command->args[2];
			char hour[10] = "";
			char min[10] = "";

			char *token = strtok(alarm_time, ".");
			int i = 0;
			while (i < 2)
			{
				if (i == 0)
				{
					strcat(hour, token);
				}
				else if (i == 1)
				{
					strcat(min, token);
				}
				i++;
				token = strtok(NULL, ".");
			}

			char command[200];
			snprintf(command, sizeof(command), "%s%s", "rhythmbox-client --play-uri=", music_file);

			//save to a file
			FILE *fp;
			fp = fopen("my_sch_job.txt", "w+");
			fprintf(fp, "%s %s * * * %s\n", min, hour, command);
			fclose(fp);

			//run crontab
			char *command_name = "crontab";
			char *args1[] = {command_name, "my_sch_job.txt", NULL};
			execvp(command_name, args1);
		}

		//PART5
		if (strcmp(command->name, "kdiff") == 0)
		{
			char *ifANotUsed = command->args[1]; //if -a is not used temp the first file name
			char *file1Name = command->args[2];
			char *file2Name = command->args[3];
			char *txt = ".txt";
			char *bin = ".bin";
			FILE *file1;
			FILE *file2;
			char file1Path[PATH_SIZE] = ""; //path for file1
			char file2Path[PATH_SIZE] = ""; //path for file2
			
			char tempFile1Name[PATH_SIZE];
			char tempFile2Name[PATH_SIZE];
			int checkIfAUsed = 1;

			int differentLineCounter = 0;
			int differentByteCounter = 0;
			char *cwd;
			char buff[PATH_SIZE + 1];
			cwd = getcwd(buff, PATH_SIZE + 1); //current path
			
			if (file2Name == NULL) //if -a or -b is not used
			{
				file2Name = file1Name;
				file1Name = ifANotUsed;
				checkIfAUsed = 0;
			}

			int j = 0;
			for (int i = strlen(file1Name) - 4; i <= strlen(file1Name) - 1; i++)
			{
				tempFile1Name[j] = file1Name[i];
				tempFile2Name[j] = file2Name[i];
				j++;
			}
			tempFile1Name[j] = '\0';
			tempFile2Name[j] = '\0';

			if ((strcmp(tempFile1Name, txt) != 0 || strcmp(tempFile2Name, txt) != 0) &&
				(strcmp(tempFile1Name, bin) != 0 || strcmp(tempFile2Name, bin) != 0)) //if the file names are not valid
			{
				perror("Invalid file name!");
				return -1;
			}

			strcpy(file1Path, cwd);
			strcpy(file2Path, cwd);
			strcat(file1Path, "/");
			strcat(file2Path, "/");
			
			strcat(file1Path, file1Name);
			strcat(file2Path, file2Name);
			
			if (strcmp(command->args[1], "-a") == 0 || checkIfAUsed == 0) //-a or empty
			{
				for (int i = 1; i <= min(howManyLines(file1, file1Path), howManyLines(file2, file2Path)); i++)
				{
					char *ithLineOfFile1 = getTheithLine(file1, i, file1Path); //temp the ith line of file 1
					char *ithLineOfFile2 = getTheithLine(file2, i, file2Path); //temp the ith line of file 2

					if (strcmp(ithLineOfFile1, ithLineOfFile2) != 0) //whether these two lines are not equal
					{
						printf("%s: Line %d: %s\n", "file1.txt", i, ithLineOfFile1);
						printf("%s: Line %d: %s\n", "file2.txt", i, ithLineOfFile2);
						differentLineCounter++;
					}
				}

				if (differentLineCounter == 0) //if two files are identical
				{
					printf("The two text files are identical\n");
				}
				else
				{
					printf("%d different lines found\n", differentLineCounter);
				}
			}
			else if (strcmp(command->args[1], "-b") == 0) //-b
			{
				
				long sz1;
				long sz2;
				int lineCounter = 0;
				char *tokenizer;
				char tmp1;
				char tmp2;

				file1 = fopen(file1Path, "rb");
				if (file1 < 0)
				{
					printf("The file could not be opened.\n");
					exit(1);
				}

				fseek(file1, 0, SEEK_END);
				sz1 = ftell(file1);
				rewind(file1);

				file2 = fopen(file2Path, "rb");
				if (file2 < 0)
				{
					printf("The file could not be opened.\n");
					exit(1);
				}

				fseek(file2, 0, SEEK_END);
				sz2 = ftell(file2);
				rewind(file2);

				for (int i = 0; i < min(sz1, sz2); i++) //iterate for the minimum size of these two files
				{
					fread(&tmp1, 1, sizeof(tmp1), file1); //read byte by byte
					fread(&tmp2, 1, sizeof(tmp2), file2); //read byte by byte

					if (tmp1 != tmp2)
					{ //if two bytes are different
						differentByteCounter++;
					}
				}
				printf("The two files are different in %d bytes\n", differentByteCounter);
				fclose(file1);
				fclose(file2);
			}
		}

		//PART6
		if (strcmp(command->name, "siri") == 0)
		{
			int i = 1;
			char commandLine[PATH_SIZE] = "";
			char oneWordLess[PATH_SIZE] = "";
			char twoWordsLess[PATH_SIZE] = "";
			char lastWord[PATH_SIZE] = "";
			char lastSecondWord[PATH_SIZE] = "";
			char secondWord[PATH_SIZE] = "";
			char withoutSecondWord[PATH_SIZE] = "";
			char *tokenizer;
			int tokenizerCounter = 0;

			while (command->args[i] != NULL) //store the string without the command "siri" on commandLine
			{
				char tempWords[PATH_SIZE];
				strcpy(tempWords, command->args[i]);
				strcat(commandLine, tempWords);
				strcat(commandLine, " ");
				i++;
			}

			tokenizer = strtok(commandLine, " ");
			while (tokenizer != NULL)
			{
				tokenizerCounter++;
				if (tokenizerCounter < i - 2)
				{
					strcat(twoWordsLess, tokenizer);
					strcat(twoWordsLess, " ");
				}
				if (tokenizerCounter < i - 1)
				{
					strcat(oneWordLess, tokenizer);
					strcat(oneWordLess, " ");
				}

				if (tokenizerCounter == i - 1)
				{
					strcat(lastWord, tokenizer);
				}
				else if (tokenizerCounter == i - 2)
				{
					strcat(lastSecondWord, tokenizer);
					strcat(lastSecondWord, " ");
				}
				if (tokenizerCounter == 2)
				{
					strcat(secondWord, tokenizer);
				}
				else
				{
					strcat(withoutSecondWord, tokenizer);
					strcat(withoutSecondWord, " ");
				}

				tokenizer = strtok(NULL, " ");
			}

			if (strcmp(oneWordLess, "what does the weather look like in ") == 0) //weather
			{
				char *command_weather = "curl";
				char tempCity[PATH_SIZE] = "";
				strcpy(tempCity, "wttr.in/");
				strcat(tempCity, lastWord);
				char *args1[] = {command_weather, tempCity, NULL};
				execvp(command_weather, args1);
			}

			if (strcmp(twoWordsLess, "start a chronometer for ") == 0) //chronometer
			{
				int minutesToSeconds;
				tokenizer = strtok(lastSecondWord, " ");
				int time = atoi(tokenizer); //time to start a chronometer

				if (strcmp(lastWord, "minutes") == 0)
				{
					minutesToSeconds = 60 * time;
					if (time < 10)
						printf("0%d:00 minutes\n", time);
					else
						printf("%d:00 minutes\n", time);

					for (int i = time - 1; i >= 0; i--)
					{
						for (int j = 59; j >= 1; j--)
						{
							if (j < 10 && i < 10)
								printf("0%d:0%d minutes\n", i, j);
							else if (j < 10 && i >= 10)
								printf("%d:0%d minutes\n", i, j);
							else if (j >= 10 && i < 10)
								printf("0%d:%d minutes\n", i, j);
							else
								printf("%d:%d minutes\n", i, j);
							sleep(1);
						}

						if (time < 10)
							printf("0%d:%d%d minutes\n", i, 0, 0);
						else
							printf("%d:%d%d minutes\n", i, 0, 0);
					}
					printf("Time is up!\n");
				}
				else if (strcmp(lastWord, "seconds") == 0)
				{
					printf("%d seconds\n", time);
					for (int i = 0; i < time; i++)
					{
						sleep(1);
						printf("%d seconds\n", time - i - 1);
					}
					printf("Time is up!\n");
					//ring an alarm here
				}
			}

			if (strcmp(withoutSecondWord, "search related news ") == 0) //url
			{
				time_t rawtime;
				struct tm *timeinfo;
				time(&rawtime);
				timeinfo = localtime(&rawtime);
        
				char mon[10] = "";
				char year[10] = "";
				char *tokens;
				char *query = secondWord;
				char *api_key = "0fdhg78gst2rlie8fsn6ifgep";

				tokens = strtok(asctime(timeinfo), " ");
				int i = 0;
				while (tokens != NULL)
				{
					tokens[strcspn(tokens, "\n")] = 0;
					if (i == 2)
					{
						strcat(mon, tokens);
					}
					else if (i == 4)
					{

						strcat(year, tokens);
					}
					i++;
					tokens = strtok(NULL, " ");
				}

				char date_from[50];
				char date_to[50];
				char query_string[1000];
				snprintf(date_from, sizeof(date_from), "%s-0%s-%s", year, mon, "01");
				snprintf(date_to, sizeof(date_to), "%s-0%s-%s", year, mon, "30");

				snprintf(query_string, sizeof(query_string), "'api.datanews.io/v1/news?q=%s&from=%s&to=%s&language=en'", query, date_from, date_to);
				//curl - XGET 'api.datanews.io/v1/news?q=SpaceX&from=2020-07-01&to=2020-09-10&language=en' - H 'x-api-key: 0fdhg78gst2rlie8fsn6ifgep'
				FILE *fp;
				fp = fopen("news.txt", "w+");
				fprintf(fp, "%s %s %s %s %s %s %s\n", "curl", "-XGET", query_string, "-H", "'x-api-key: 0fdhg78gst2rlie8fsn6ifgep'", ">", "news_return.txt");
				fclose(fp);

				char *argz[3];
				argz[0] = "sh";
				argz[1] = "news.txt";
				argz[2] = NULL;
				execvp("/bin/sh", argz);

				//read json
				char buff[1024];
				int num;
				FILE *fptr;

				if ((fptr = fopen("news_return.txt", "r")) == NULL)
				{
					printf("Error! opening file");

					// Program exits if the file pointer returns NULL.
					exit(1);
				}

				fscanf(fptr, "%[^\n]", buff);
				printf("Data from the file:\n%s", buff);
				fclose(fptr);
			}
		}

		exit(0);
	}
	else //parent process
	{
		if (!command->background)
			wait(0); // wait for child process to finish

		//use chdir function here to change the direction with the help of pipes
		close(p[WRITE_END]);
		read(p[READ_END], message, sizeof(message) + 1);
		chdir(message);
		close(p[READ_END]);

		return SUCCESS;
	}

	// TODO: your implementation here

	printf("-%s: %s: command not found\n", sysname, command->name);
	return UNKNOWN;
}
