# C Terminal Shell
 
## Overview
`EasyShell` is a basic bash-like shell implemented in C that that provides a subset of features from `bash`. It features include running commands, I/O redirection, background processing, and built-in signal handling. 

## Features

- **Prompt for Commands**: Provides a colon (`:`) as the prompt for user input.
- **Command Execution**: Runs any valid command using `fork()`, `exec()`, and/or `waitpid()`.
- **Built-in Commands**:
  - `exit`: Exits the shell, terminating all child processes.
  - `cd`: Changes the current working directory.
  - `status`: Displays the exit status or the signal of the last foreground process.
- **Input and Output Redirection**: Supports both input and output redirection using `<` and `>`.
- **Foreground and Background Execution**: Runs commands in the foreground or background depending on the presence of `&`.
- **Signal Handling**:
  - `SIGINT` (Ctrl+C): Terminates foreground processes but does not affect the shell.
  - `SIGTSTP` (Ctrl+Z): Toggles between allowing and disallowing background execution.

## How to Use

### 1. Command Prompt
The shell uses `:` as the prompt. The general command format is:

```bash
command [arg1 arg2 ...] [< input_file] [> output_file] [&]
```

- Commands are words separated by spaces.
- Redirection symbols `<`, `>`, and `&` must be surrounded by spaces.
- `$$(PID expansion)` replaces instances of `$$` with the process ID of the shell.
  
Example:
```bash
ls -l > output.txt &
```

### 2. Built-in Commands

#### `exit`
Exit the shell and terminate all background processes.
```bash
exit
```

#### `cd [directory]`
Change the current directory. Without arguments, changes to the home directory.
```bash
cd /path/to/directory
```

#### `status`
Displays the exit status or terminating signal of the last foreground command.
```bash
status
```

### 3. Input & Output Redirection
Redirect input and output of commands using `<` and `>`. For example, to read from `input.txt` and write to `output.txt`:
```bash
sort < input.txt > output.txt
```

### 4. Foreground & Background Execution
- **Foreground**: Default for commands without `&`. The shell waits for these commands to finish.
  ```bash
  sleep 5
  ```
- **Background**: Append `&` to run commands in the background.
  ```bash
  sleep 5 &
  ```

### 5. Signal Handling

#### `SIGINT` (Ctrl+C)
- Foreground processes are terminated.
- Background processes and the shell ignore the signal.

#### `SIGTSTP` (Ctrl+Z)
- Toggles background execution. When background execution is disabled, commands with `&` run in the foreground.
  
Message for background suspension:
```bash
Entering foreground-only mode (& is now ignored)
```

Message to resume background execution:
```bash
Exiting foreground-only mode
```

## Compilation
To compile `EasyShell`, use the following command:
```bash
gcc -std=gnu99 main.c -o EasyShell
```

## Running EasyShell
Run the shell by executing:
```bash
./EasyShell
```
---

### Example Usage

```bash
: ls -l > directory_listing.txt
: cat < directory_listing.txt
: sleep 10 &
: status
: exit
```
