#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/types.h>
#include <dirent.h>

int getargs(char *cmd, char **argv);

void handle_signal(int signo)
{
    if (signo == SIGINT)
    {
        printf("\nSIGINT (Ctrl-C)\n");
    }
    else if (signo == SIGQUIT)
    {
        printf("\nSIGQUIT (Ctrl-Z)\n");
    }
}

void ls_command()
{
    struct dirent *entry;
    DIR *dir = opendir(".");

    if (dir == NULL)
    {
        perror("opendir");
        exit(EXIT_FAILURE);
    }

    while ((entry = readdir(dir)) != NULL)
    {
        printf("%s\n", entry->d_name);
    }

    closedir(dir);
}

void pwd_command()
{
    char cwd[1024];
    if (getcwd(cwd, sizeof(cwd)) != NULL)
    {
        printf("%s\n", cwd);
    }
    else
    {
        perror("getcwd");
    }
}

void cd_command(char *path)
{
    if (chdir(path) != 0)
    {
        perror("chdir");
    }
}

void mkdir_command(char *path)
{
    if (mkdir(path, 0777) != 0)
    {
        perror("mkdir");
    }
}

void rmdir_command(char *path)
{
    if (rmdir(path) != 0)
    {
        perror("rmdir");
    }
}

void ln_command(char *source, char *link_name)
{
    if (link(source, link_name) != 0)
    {
        perror("ln");
    }
}

void cp_command(char *source, char *destination)
{
    int source_fd = open(source, O_RDONLY);
    if (source_fd == -1)
    {
        perror("cp");
        return;
    }

    int dest_fd = open(destination, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (dest_fd == -1)
    {
        perror("cp");
        close(source_fd);
        return;
    }

    char buffer[4096];
    ssize_t read_bytes, write_bytes;

    while ((read_bytes = read(source_fd, buffer, sizeof(buffer))) > 0)
    {
        write_bytes = write(dest_fd, buffer, read_bytes);
        if (write_bytes != read_bytes)
        {
            perror("cp");
            close(source_fd);
            close(dest_fd);
            return;
        }
    }

    close(source_fd);
    close(dest_fd);
}

void rm_command(char *path)
{
    if (unlink(path) != 0)
    {
        perror("rm");
    }
}

void mv_command(char *source, char *destination)
{
    if (rename(source, destination) != 0)
    {
        perror("mv");
    }
}

void cat_command(char *path)
{
    int fd = open(path, O_RDONLY);
    if (fd == -1)
    {
        perror("cat");
        return;
    }

    char buffer[4096];
    ssize_t read_bytes;

    while ((read_bytes = read(fd, buffer, sizeof(buffer))) > 0)
    {
        write(STDOUT_FILENO, buffer, read_bytes);
    }

    close(fd);
}

int main()
{
    // Set up signal handlers
    signal(SIGINT, handle_signal);
    signal(SIGQUIT, handle_signal);

    char buf[256];
    char *argv[50];
    int narg;
    pid_t pid;
    int background;

    while (1)
    {
        printf("shell> ");
        fgets(buf, sizeof(buf), stdin);
        buf[strcspn(buf, "\n")] = '\0'; // Remove newline character

        // Check if the user entered "exit"
        if (strcmp(buf, "exit") == 0)
        {
            printf("Exiting the shell.\n");
            break;
        }

        // Check if the command should run in the background
        background = 0;
        if (buf[strlen(buf) - 1] == '&')
        {
            background = 1;
            buf[strlen(buf) - 1] = '\0'; // Remove the '&' character
        }

        narg = getargs(buf, argv);
        pid = fork();

        if (pid == 0)
        {
            // Child process
            if (background)
            {
                // If running in the background, detach from the terminal
                setsid();
            }

            // File redirection
            int fd_in, fd_out;

            for (int i = 0; argv[i] != NULL; i++)
            {
                if (strcmp(argv[i], "<") == 0)
                {
                    fd_in = open(argv[i + 1], O_RDONLY);
                    dup2(fd_in, STDIN_FILENO);
                    close(fd_in);

                    argv[i] = NULL;
                }
                else if (strcmp(argv[i], ">") == 0)
                {
                    fd_out = open(argv[i + 1], O_WRONLY | O_TRUNC | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
                    dup2(fd_out, STDOUT_FILENO);
                    close(fd_out);

                    argv[i] = NULL;
                }
            }

            // Pipe handling
            for (int i = 0; argv[i] != NULL; i++)
            {
                if (strcmp(argv[i], "|") == 0)
                {
                    argv[i] = NULL;

                    int pipe_fd[2];
                    if (pipe(pipe_fd) == -1)
                    {
                        perror("pipe");
                        exit(EXIT_FAILURE);
                    }

                    pid_t pipe_pid = fork();

                    if (pipe_pid == 0)
                    {
                        // Child process for the pipe
                        close(pipe_fd[0]); // Close the reading end of the pipe
                        dup2(pipe_fd[1], STDOUT_FILENO);
                        close(pipe_fd[1]); // Close the duplicated writing end of the pipe

                        execvp(argv[0], argv);
                        perror("execvp failed");
                        exit(EXIT_FAILURE);
                    }
                    else if (pipe_pid > 0)
                    {
                        // Parent process
                        waitpid(pipe_pid, NULL, 0);
                        close(pipe_fd[1]); // Close the writing end of the pipe
                        dup2(pipe_fd[0], STDIN_FILENO);
                        close(pipe_fd[0]); // Close the duplicated reading end of the pipe

                        break;
                    }
                    else
                    {
                        perror("fork failed");
                        exit(EXIT_FAILURE);
                    }
                }
            }

            // Check if the command is 'ls' and execute the ls_command
            if (strcmp(argv[0], "ls") == 0)
            {
                ls_command();
                exit(EXIT_SUCCESS);
            }
            // Check if the command is 'pwd' and execute the pwd_command
            else if (strcmp(argv[0], "pwd") == 0)
            {
                pwd_command();
                exit(EXIT_SUCCESS);
            }
            // Check if the command is 'cd' and execute the cd_command
            else if (strcmp(argv[0], "cd") == 0)
            {
                cd_command(argv[1]); // Pass the argument to cd_command
                exit(EXIT_SUCCESS);
            }
            // Check if the command is 'mkdir' and execute the mkdir_command
            else if (strcmp(argv[0], "mkdir") == 0)
            {
                mkdir_command(argv[1]); // Pass the argument to mkdir_command
                exit(EXIT_SUCCESS);
            }
            // Check if the command is 'rmdir' and execute the rmdir_command
            else if (strcmp(argv[0], "rmdir") == 0)
            {
                rmdir_command(argv[1]); // Pass the argument to rmdir_command
                exit(EXIT_SUCCESS);
            }
            // Check if the command is 'ln' and execute the ln_command
            else if (strcmp(argv[0], "ln") == 0)
            {
                ln_command(argv[1], argv[2]); // Pass the arguments to ln_command
                exit(EXIT_SUCCESS);
            }
            // Check if the command is 'cp' and execute the cp_command
            else if (strcmp(argv[0], "cp") == 0)
            {
                cp_command(argv[1], argv[2]); // Pass the arguments to cp_command
                exit(EXIT_SUCCESS);
            }
            // Check if the command is 'rm' and execute the rm_command
            else if (strcmp(argv[0], "rm") == 0)
            {
                rm_command(argv[1]); // Pass the argument to rm_command
                exit(EXIT_SUCCESS);
            }
            // Check if the command is 'mv' and execute the mv_command
            else if (strcmp(argv[0], "mv") == 0)
            {
                mv_command(argv[1], argv[2]); // Pass the arguments to mv_command
                exit(EXIT_SUCCESS);
            }
            // Check if the command is 'cat' and execute the cat_command
            else if (strcmp(argv[0], "cat") == 0)
            {
                cat_command(argv[1]); // Pass the argument to cat_command
                exit(EXIT_SUCCESS);
            }

            execvp(argv[0], argv);
            perror("execvp failed");
            exit(EXIT_FAILURE);
        }
        else if (pid > 0)
        {
            // Parent process
            if (!background)
            {
                // If not running in the background, wait for the child to finish
                waitpid(pid, NULL, 0);
            }
        }
        else
        {
            perror("fork failed");
        }
    }

    return 0;
}

int getargs(char *cmd, char **argv)
{
    int narg = 0;
    while (*cmd)
    {
        if (*cmd == ' ' || *cmd == '\t')
            *cmd++ = '\0';
        else
        {
            argv[narg++] = cmd++;
            while (*cmd != '\0' && *cmd != ' ' && *cmd != '\t')
                cmd++;
        }
    }
    argv[narg] = NULL;
    return narg;
}
