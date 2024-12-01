#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>        // Для open
#include <sys/resource.h> // Для setpriority и PRIO_PROCESS
#include <dirent.h>       // Для opendir, readdir
#include <ctype.h>        // Для isdigit

#define LSH_RL_BUFSIZE 1024
#define LSH_TOK_BUFSIZE 64
#define LSH_TOK_DELIM " \t\r\n\a"

// Прототипы встроенных функций
int lsh_cd(char **args);
int lsh_help(char **args);
int lsh_exit(char **args);
int lsh_ls(char **args);
int lsh_cat(char **args);
int lsh_nice_cmd(char **args);
int lsh_killall(char **args);
int lsh_pwd(char **args);

// Список встроенных команд
char *builtin_str[] = {
    "cd",
    "help",
    "exit",
    "ls",
    "cat",
    "nice",
    "killall",
    "pwd"
};

// Массив указателей на встроенные функции
int (*builtin_func[]) (char **) = {
    &lsh_cd,
    &lsh_help,
    &lsh_exit,
    &lsh_ls,
    &lsh_cat,
    &lsh_nice_cmd,
    &lsh_killall,
    &lsh_pwd,
};

// Количество встроенных команд
int lsh_num_builtins() {
    return sizeof(builtin_str) / sizeof(char *);
}

//Обработчик сигнала SIGINT (CTRL+C)
void sigint_handler(int sig){
    // Просто выводим новую строку и промпт
    printf("Получен сигнал CTRL + C");
    exit(0);
}

// Встроенная команда: cd
int lsh_cd(char **args){
    if(args[1] == NULL){
        fprintf(stderr, "lsh: expected argument to \"cd\"\n");
    } else {
        // Отладочный вывод
        printf("DEBUG: Changing directory to: '%s'\n", args[1]);

        // Используем системный вызов chdir
        if (chdir(args[1]) != 0){
            perror("lsh");
        }
    }
    return 1;
}

// Встроенная команда: help
int lsh_help(char **args){
    int i;
    printf("Klim Konoplev Shell\n");
    printf("The following are built in:\n");

    for(i = 0; i < lsh_num_builtins(); i++){
        printf("   %s\n", builtin_str[i]);
    }

    printf("Use the man command for information on other programs.\n");
    return 1;
}

// Встроенная команда: exit
int lsh_exit(char **args){
    return 0;
}

// Встроенная команда: ls (реализация с использованием opendir и readdir)
int lsh_ls(char **args){
    DIR *d;
    struct dirent *dir;
    char *path = "."; // По умолчанию текущая директория

    // Если указан путь, используем его
    if (args[1] != NULL) {
        path = args[1];
    }

    // Отладочный вывод
    printf("DEBUG: ls command with path: '%s'\n", path);

    d = opendir(path);
    if (d) {
        while ((dir = readdir(d)) != NULL) {
            // Пропускаем текущую и родительскую директории
            if (strcmp(dir->d_name, ".") != 0 && strcmp(dir->d_name, "..") != 0) {
                printf("%s\t", dir->d_name);
            }
        }
        printf("\n");
        closedir(d);
    } else {
        perror("lsh");
    }
    return 1;
}

// Встроенная команда: cat (реализация с использованием open, read и write)
int lsh_cat(char **args){
    if (args[1] == NULL) {
        fprintf(stderr, "lsh: expected argument to \"cat\"\n");
        return 1;
    }

    int fd = open(args[1], O_RDONLY);
    if (fd == -1) {
        perror("lsh");
        return 1;
    }

    ssize_t nread;
    char buffer[256];

    while ((nread = read(fd, buffer, sizeof(buffer))) > 0) {
        ssize_t nwritten = write(STDOUT_FILENO, buffer, nread);
        if (nwritten == -1) {
            perror("lsh");
            close(fd);
            return 1;
        }
    }

    if (nread == -1) {
        perror("lsh");
    }

    close(fd);
    return 1;
}

// Встроенная команда: nice (изменение приоритета процесса)
int lsh_nice_cmd(char **args){
    if (args[1] == NULL || args[2] == NULL) {
        fprintf(stderr, "lsh: expected arguments to \"nice\" (PID and priority)\n");
        return 1;
    }

    pid_t pid = atoi(args[1]);
    int priority = atoi(args[2]);

    if (pid <= 0) {
        fprintf(stderr, "lsh: invalid PID\n");
        return 1;
    }

    // Используем системный вызов setpriority


if (setpriority(PRIO_PROCESS, pid, priority) == -1) {
        perror("lsh");
    } else {
        printf("lsh: Changed priority of process %d to %d.\n", pid, priority);
    }

    return 1;
}

// Встроенная команда: killall (завершение всех процессов с указанным именем)
int lsh_killall(char **args){
    if (args[1] == NULL) {
        fprintf(stderr, "lsh: expected argument to \"killall\" (process name)\n");
        return 1;
    }

    char *target = args[1];
    DIR *proc_dir = opendir("/proc");
    if (proc_dir == NULL) {
        perror("lsh");
        return 1;
    }

    struct dirent *entry;
    while ((entry = readdir(proc_dir)) != NULL) {
        // Проверяем, что имя директории состоит только из цифр (PID)
        int is_pid = 1;
        for (int i = 0; entry->d_name[i] != '\0'; i++) {
            if (!isdigit(entry->d_name[i])) {
                is_pid = 0;
                break;
            }
        }

        if (!is_pid) {
            continue;
        }

        // Формируем путь к файлу comm
        char comm_path[512]; // Увеличен размер буфера с 256 до 512
        snprintf(comm_path, sizeof(comm_path), "/proc/%s/comm", entry->d_name);

        int comm_fd = open(comm_path, O_RDONLY);
        if (comm_fd == -1) {
            continue;
        }

        char comm_name[256];
        ssize_t nread = read(comm_fd, comm_name, sizeof(comm_name) - 1);
        if (nread > 0) {
            comm_name[nread] = '\0';
            // Удаляем символ новой строки
            comm_name[strcspn(comm_name, "\n")] = '\0';
            if (strcmp(comm_name, target) == 0) {
                pid_t pid = atoi(entry->d_name);
                if (kill(pid, SIGTERM) == -1) {
                    perror("lsh");
                } else {
                    printf("lsh: Process %d (%s) terminated.\n", pid, comm_name);
                }
            }
        }

        close(comm_fd);
    }

    closedir(proc_dir);
    return 1;
}

// Встроенная команда: pwd (вывод текущей директории)
int lsh_pwd(char **args){
    char cwd[1024];
    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        printf("%s\n", cwd);
    } else {
        perror("lsh");
    }
    return 1;
}

// Функция для запуска внешних команд с автоматическим перезапуском
int lsh_launch(char **args){
    pid_t pid, wpid;
    int status;

    pid = fork();

    if (pid == 0) {
        // Дочерний процесс
        // Восстанавливаем стандартную обработку сигнала SIGINT
        signal(SIGINT, SIG_DFL);

        if (execvp(args[0], args) == -1){
            perror("lsh");
        }
        exit(EXIT_FAILURE);
    } else if (pid < 0){
        // Ошибка при форке
        perror("lsh");
    } else {
        // Родительский процесс
        do {
            wpid = waitpid(pid, &status, 0);
            if (wpid == -1) {
                perror("lsh");
                return 1;
            }

            if (WIFEXITED(status)) {
                printf("lsh: Process %s exited with status %d.\n", args[0], WEXITSTATUS(status));
                break; // Выходим из цикла, не перезапуская
            } else if (WIFSIGNALED(status)) {
                printf("lsh: Process %s killed by signal %d. Restarting...\n", args[0], WTERMSIG(status));
                // Автоматически перезапускаем процесс
                pid = fork();
                if (pid == 0) {
                    // Дочерний процесс
                    signal(SIGINT, SIG_DFL);
                    if (execvp(args[0], args) == -1){
                        perror("lsh");
                    }
                    exit(EXIT_FAILURE);
                } else if (pid < 0){
                    perror("lsh");
                    break;
                }
                // Родительский процесс продолжает ожидать
            }
        } while (!WIFEXITED(status) && !WIFSIGNALED(status));
    }

    return 1;
}

// Функция для чтения строки ввода от пользователя
char *lsh_read_line(void){
    char *line = NULL;
    size_t buffsize = 0; // Изменено с ssize_t на size_t

    if(getline(&line, &buffsize, stdin) == -1) {
        if(feof(stdin)){

exit(EXIT_SUCCESS);  // Получен EOF
        } else {
            perror("lsh: getline");
            exit(EXIT_FAILURE);
        }
    }

    // Удаляем символ новой строки, если он есть
    size_t len = strlen(line);
    if (len > 0 && line[len-1] == '\n') {
        line[len-1] = '\0';
    }

    return line;
}

// Функция для разбивки строки на аргументы вручную без использования strtok
char **lsh_split_line(char *line){
    int bufsize = LSH_TOK_BUFSIZE;
    int position = 0;
    char **tokens = malloc(sizeof(char*) * bufsize);
    int i = 0;

    if(!tokens){
        fprintf(stderr, "lsh: allocation error\n");
        exit(EXIT_FAILURE);
    }

    while (line[i] != '\0') {
        // Пропускаем начальные пробелы
        while (line[i] == ' ' || line[i] == '\t') {
            line[i] = '\0';
            i++;
        }

        if (line[i] == '\0') {
            break;
        }

        // Обработка кавычек
        if (line[i] == '"') {
            i++;
            char *start = &line[i];
            while (line[i] != '"' && line[i] != '\0') {
                i++;
            }
            if (line[i] == '"') {
                line[i] = '\0';
                i++;
            }
            tokens[position++] = start;
        } else {
            // Обычные аргументы
            char *start = &line[i];
            while (line[i] != ' ' && line[i] != '\t' && line[i] != '\0') {
                i++;
            }
            if (line[i] != '\0') {
                line[i] = '\0';
                i++;
            }
            tokens[position++] = start;
        }

        // Перевыделение памяти при необходимости
        if (position >= bufsize) {
            bufsize += LSH_TOK_BUFSIZE;
            tokens = realloc(tokens, bufsize * sizeof(char*));
            if(!tokens){
                fprintf(stderr, "lsh: allocation error\n");
                exit(EXIT_FAILURE);
            }
        }
    }

    tokens[position] = NULL;

    // Отладочный вывод всех аргументов
    printf("DEBUG: Parsed arguments:\n");
    for(int j = 0; tokens[j] != NULL; j++){
        printf("  args[%d]: '%s'\n", j, tokens[j]);
    }

    return tokens;
}

// Функция для выполнения команд
int lsh_execute(char **args){
    int i;

    if(args[0] == NULL){
        return 1; // Пустая команда
    }

    // Отладочный вывод
    printf("DEBUG: Executing command: '%s'\n", args[0]);
    if(args[1] != NULL){
        printf("DEBUG: Argument 1: '%s'\n", args[1]);
    }

    for(i = 0; i < lsh_num_builtins(); i++){
        if(strcmp(args[0], builtin_str[i]) == 0){
            return (*builtin_func[i])(args);
        }
    }

    return lsh_launch(args);
}

// Основной цикл шелла
void lsh_loop(void){
    char *line;
    char **args;
    int status;

    do {
        printf("> ");
        line = lsh_read_line();
        args = lsh_split_line(line);
        status = lsh_execute(args);

        free(line);
        free(args);
    } while (status);
}

// Основная функция
int main(int argc, char **argv) { 
    if(signal(SIGINT, sigint_handler) == SIG_ERR)
    {
        perror("сигнал");
        exit(EXIT_FAILURE);
    }
    while(1)
    {

        // Запускаем основной цикл шелла
        lsh_loop();
    }

    return EXIT_SUCCESS;
}
