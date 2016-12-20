#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <dirent.h>
#include <fcntl.h>

std::vector<std::string> parsing(std::string strin);
bool findBin(char * path, std::string bin);
void redirect(std::vector<std::string> & parsed);
void pipeRedirect(std::vector<std::string> & parsed);
void printPrompt();
void execute(std::vector<std::string> & parsed);
void execDirectory(std::vector<std::string> & parsed,std::vector<std::string> & dirStack);
void execExistBin(std::vector<std::string> & parsed);

std::vector<std::string> parsing(std::string strin){
  std::vector<std::string> parsed;
  size_t space = strin.find_first_not_of(" ");
  std::string str = strin.substr(space);
  size_t head = 0;
  space = 0;
  while(space <= str.length()){
    space = str.find_first_of(" ", space);
    if((space - str.rfind("\\", space)) != 1 || space == std::string::npos){
      parsed.push_back(str.substr(head, space-head));
      if(space == std::string::npos){
        break;
      }
      head = str.find_first_not_of(" ", space);
      if(head == std::string::npos){
        break;
      }
      space = head;
    }
    else{
      str.erase(str.begin()+space-1);
    }
  }
  return parsed;
}

void redirect(std::vector<std::string> & parsed){
  std::vector<std::string>::iterator it;
  for(it = parsed.begin(); it < parsed.end()-1; it++){
    if(!it->compare("<")){
      if(*(it+1) == "<" || *(it+1) == ">" || *(it+1) == "2>" || *(it+1) == "|"){
        std::cerr << "Redirect < : multiple redirect is not allowed" << std::endl;
        if(parsed[0] == "cd" || parsed[0] == "pushd" || parsed[0] == "popd" || parsed[0] == "dirstack"){
          parsed.erase(it, it+2);
          return;
        }
        exit(EXIT_FAILURE);
      }
      int fdin = open((it+1)->c_str(), O_RDONLY);
      if(fdin == -1){
        std::cerr << "Redirect < : error open file " << (it+1)->c_str() << std::endl;
        if(parsed[0] == "cd" || parsed[0] == "pushd" || parsed[0] == "popd" || parsed[0] == "dirstack"){
          parsed.erase(it, it+2);
          return;
        }
        exit(EXIT_FAILURE);
      }
      close(0);
      if(dup2(fdin, 0) < 0){
        std::cerr << "Redirect < : duplicate stdin fd error" << std::endl;
        if(parsed[0] == "cd" || parsed[0] == "pushd" || parsed[0] == "popd" || parsed[0] == "dirstack"){
          parsed.erase(it, it+2);
          return;
        }
        exit(EXIT_FAILURE);
      }
      close(fdin);
      parsed.erase(it, it+2);
      it--;
    }
    else if(!it->compare(">")){
      if(*(it+1) == "<" || *(it+1) == ">" || *(it+1) == "2>" || *(it+1) == "|"){
        std::cerr << "Redirect > : multiple redirect is not allowed" << std::endl;
        if(parsed[0] == "cd" || parsed[0] == "pushd" || parsed[0] == "popd" || parsed[0] == "dirstack"){
          parsed.erase(it, it+2);
          return;
        }
        exit(EXIT_FAILURE);
      }
      int fdout = open((it+1)->c_str(), O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);
      if(fdout == -1){
        std::cerr << "Redirect > : error open file " << (it+1)->c_str() << std::endl;
        if(parsed[0] == "cd" || parsed[0] == "pushd" || parsed[0] == "popd" || parsed[0] == "dirstack"){
          parsed.erase(it, it+2);
          return;
        }
        exit(EXIT_FAILURE);
      }
      close(1);
      if(dup2(fdout, 1) < 0){
        std::cerr << "Redirect > : duplicate stdout fd error" << std::endl;
        if(parsed[0] == "cd" || parsed[0] == "pushd" || parsed[0] == "popd" || parsed[0] == "dirstack"){
          parsed.erase(it, it+2);
          return;
        }
        exit(EXIT_FAILURE);
      }
      close(fdout);
      parsed.erase(it, it+2);
      it--;
    }
    else if(!it->compare("2>")){
      if(*(it+1) == "<" || *(it+1) == ">" || *(it+1) == "2>" || *(it+1) == "|"){
        std::cerr << "Redirect 2> : multiple redirect is not allowed" << std::endl;
        if(parsed[0] == "cd" || parsed[0] == "pushd" || parsed[0] == "popd" || parsed[0] == "dirstack"){
          parsed.erase(it, it+2);
          return;
        }
        exit(EXIT_FAILURE);
      }
      int fderr = open((it+1)->c_str(), O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);
      if(fderr == -1){
        std::cerr << "Redirect 2> : error open file "<< (it+1)->c_str() << std::endl;
        if(parsed[0] == "cd" || parsed[0] == "pushd" || parsed[0] == "popd" || parsed[0] == "dirstack"){
          parsed.erase(it, it+2);
          return;
        }
        exit(EXIT_FAILURE);
      }
      close(2);
      if(dup2(fderr, 2) < 0){
        std::cerr << "Redirect 2> : duplicate stderr fd error" << std::endl;
        if(parsed[0] == "cd" || parsed[0] == "pushd" || parsed[0] == "popd" || parsed[0] == "dirstack"){
          parsed.erase(it, it+2);
          return;
        }
        exit(EXIT_FAILURE);
      }
      close(fderr);
      parsed.erase(it, it+2);
      it--;
    }
  }
}

void pipeRedirect(std::vector<std::string> & parsed){
  int pipefd[2];
  int status;
  std::vector<std::string>::iterator it;
  for(it = parsed.begin(); it < parsed.end(); it++){
    if(!it->compare("|")){
      pid_t pipepid;
      if(pipe(pipefd) == -1){
        std::cerr << "Pipe initialization error" << std::endl;
        exit(EXIT_FAILURE);
      }
      pipepid = fork();
      if(pipepid == -1){
        std::cerr << "Pipe fork error" << std::endl;
        exit(EXIT_FAILURE);
      }
      else if(pipepid == 0){
        parsed.erase(it, parsed.end());
        close(pipefd[0]);
        dup2(pipefd[1], 1);
        close(pipefd[1]);
        fcntl(1, F_SETPIPE_SZ, 1048576);
        if(parsed[0] == "cd" || parsed[0] == "pushd" || parsed[0] == "popd" || parsed[0] == "dirstack"){
          std::cerr << "Manipulating directory in pipe is not allowed" << std::endl;
          return;
        }
        if(parsed[0].find('/') == std::string::npos){
          execExistBin(parsed);
        }
        else{
          execute(parsed);
          std::cerr << "Error executing command " << parsed[0] << std::endl;
        }
        return;
      }
      else{
        pid_t cpid = waitpid(pipepid, &status, WUNTRACED | WCONTINUED);
        if(cpid == -1){
          std::cerr << "Pipe waitpid error" << std::endl;
          exit(EXIT_FAILURE);
        }
        parsed.erase(parsed.begin(), it+1);
        it = parsed.begin();
        close(pipefd[1]);
        dup2(pipefd[0], 0);
        close(pipefd[0]);
        fcntl(0, F_SETPIPE_SZ, 1048576);
      }
    }
  }
  if(it == parsed.end()){
    if(parsed[0] == "cd" || parsed[0] == "pushd" || parsed[0] == "popd" || parsed[0] == "dirstack"){
      std::cerr << "Manipulating directory in pipe is not allowed" << std::endl;
      return;
    }
    if(parsed[0].find('/') == std::string::npos){
      execExistBin(parsed);
    }
    else{
      execute(parsed);
      std::cerr << "Error executing command " << parsed[0] << std::endl;
    }
    return;
  }
}

void printPrompt(){
  char * currDir = get_current_dir_name();
  std::cout << "myShell:" << currDir << " $ ";
  free(currDir);
}

void execute(std::vector<std::string> & parsed){
  redirect(parsed);
  char ** execArgv = new char*[parsed.size()+1]();
  for(size_t i = 0; i < parsed.size(); i++){
    execArgv[i] = (char *)(parsed[i].c_str());
  }
  execArgv[parsed.size()] = NULL;
  char * execEnvp[1] = {NULL};
  execve(parsed[0].c_str(), execArgv, execEnvp);
  delete[] execArgv;
}

void execDirectory(std::vector<std::string> & parsed,std::vector<std::string> & dirStack){
  int fdin = dup(0);
  int fdout = dup(1);
  int fderr = dup(2);
  redirect(parsed);
  if(parsed[0] == "cd" || parsed[0] == "pushd"){
    char * curr = get_current_dir_name();
    std::string currDir(curr);
    free(curr);
    if(parsed.size() == 1){
      chdir(".");
      if(parsed[0] == "pushd"){
        dirStack.push_back(currDir);
      }
    }
    else if(chdir(parsed[1].c_str())){
      std::cerr << "cd/pushd : change directory failed" << std::endl;
    }
    else if(parsed[0] == "pushd"){
      dirStack.push_back(currDir);
    }
  }
  else if(parsed[0] == "popd"){
    if(dirStack.size() == 0){
      std::cerr << "popd : directory stack is empty" << std::endl;
    }
    else{
      chdir(dirStack.back().c_str());
      dirStack.pop_back();
    }
  }
  else{
    std::vector<std::string>::iterator it;
    for(it = dirStack.begin(); it < dirStack.end(); it++){
      std::cout << *it << std::endl;
    }
  }
  dup2(fdin, 0);
  dup2(fdout, 1);
  dup2(fderr, 2);
}

bool findBin(char * path, std::string bin){
  DIR * dir = opendir(path);
  struct dirent * childDir;
  if(dir == NULL){
    return false;
  }
  while((childDir = readdir(dir)) != NULL){
    if((childDir->d_type == DT_REG || childDir->d_type == DT_LNK) && !bin.compare(childDir->d_name)){
      closedir(dir);
      return true;
    }
  }
  closedir(dir);
  return false;
}

void execExistBin(std::vector<std::string> & parsed){
  std::string path(getenv("PATH"));
  size_t colonPos = path.find(":");
  while(path.length() > 0){
    std::string binPath = path.substr(0, colonPos);
    if(findBin((char *)binPath.c_str(), parsed[0])){
      parsed[0].insert(0, "/");
      parsed[0].insert(0, binPath);
      break;
    }
    if(colonPos == std::string::npos){
      path.erase(0, path.length());
    }
    else{
      path.erase(0, colonPos+1);
      colonPos = path.find(":");
    }
  }
  if(path.length() == 0){
    std::cerr << "Command " << parsed[0] << " not found" << std::endl;
  }
  else{
    execute(parsed);
  }
}

int main(int argc, char **argv){
  int status;
  pid_t pid;
  std::string strin;
  std::vector<std::string> dirStack;
  std::cout << "[Entering myShell...]" << std::endl << "[Done]" << std::endl;
  printPrompt();
  while(std::getline(std::cin, strin) && !std::cin.eof()){
    if(strin.length() == 0 || strin.find_first_not_of(" ") == std::string::npos){
      printPrompt();
      continue;
    }
    std::vector<std::string> parsed = parsing(strin);
    if(parsed[0] == "exit"){
      std::cout << "[Exiting myShell...]" << std::endl << "[Done]" << std::endl << std::endl;
      break;
    }
    std::vector<std::string>::iterator it;
    for(it = parsed.begin(); it < parsed.end(); it++){
      if(!it->compare("|")){
        break;
      }
    }
    if(it == parsed.end() && (parsed[0] == "cd" || parsed[0] == "pushd" || parsed[0] == "popd" || parsed[0] == "dirstack")){
      execDirectory(parsed, dirStack);
    }
    else{
      pid = fork();
      if(pid == -1){
        std::cerr << "fork error" << std::endl;
      }
      else if(pid == 0){
        pipeRedirect(parsed);
        return EXIT_FAILURE;
      }
      else{
        pid_t childpid = waitpid(pid, &status, WUNTRACED | WCONTINUED);
        if(childpid == -1){
          std::cerr << "waitpid error" << std::endl;
        }
        if(WIFEXITED(status)){
          std::cout << "Program exited with status " << WEXITSTATUS(status) << std::endl;
        }
        else if(WIFSIGNALED(status)){
          std::cout << "Program was killed by signal " << WTERMSIG(status) << std::endl;
        }
      }
    }
    printPrompt();
  }
  return EXIT_SUCCESS;
}
