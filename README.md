# Mini_Shell
Support system built-in commands whose path are indicated by $PATH
support absolute / non-absolute path
*support directory operation cd / pushd / popd / dirstack
support redirect < / > / 2>
support multiple pipes, e.g. ./myProgram a b c < anInputFile | ./anotherProgram 23 45 | cat > someOutputFile 2> errorLog

*
The "pushd" command takes one argument (which names a directory) and "cd"s to that directory, but pushes the old directory onto a stack.
The "popd" command pops the top of the directory stack, and "cd"s to that directory. If the directory stack is empty, the popd command prints an error message.  
The "dirstack" command lists the directory stack, with the top of the stack last, and the bottom first.
