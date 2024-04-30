// IO
#include <iostream>
// std::string
#include <string>
// std::vector
#include <vector>
// std::string 转 int
#include <sstream>
// PATH_MAX 等常量
#include <climits>
// POSIX API
#include <unistd.h>
// wait
#include <sys/wait.h>
#include <pwd.h>
#include <string.h>
#include <fcntl.h> // Include the necessary header file

std::vector<std::string> split(std::string s, const std::string &delimiter);

std::string trim(const std::string& str);

void PrintPrompt()
{
  // 获取用户名和家目录
  struct passwd *pw = getpwuid(getuid());
  const char *username = pw->pw_name;
  const char *homedir = pw->pw_dir;

  // 获取主机名
  char hostname[HOST_NAME_MAX];
  gethostname(hostname, HOST_NAME_MAX);

  // 获取当前工作目录
  char cwd[PATH_MAX];
  getcwd(cwd, PATH_MAX);

  // 检查当前工作目录是否包含家目录，如果是，用~替换家目录
  std::string path(cwd);
  std::string home(homedir);
  if (path.find(home) == 0)
  {
    path.replace(0, home.length(), "~");
  }

  // 使用geteuid()检查用户权限，如果是0，使用"#"作为提示符，否则使用"$"
  const char *prompt = (geteuid() == 0) ? "#" : "$";

  // 打印提示符
  std::cout << "[MyShell]" << username << "@" << hostname << ":" << path << prompt << " ";
}

bool IsEmptyCmd(std::string cmdLine){
  return cmdLine.empty();
}

bool IsExit(std::string cmdLine){
  std::vector<std::string> args = split(cmdLine, " ");
  return args[0] == "exit";
}

int ExitHandler(std::string cmdLine){
  std::vector<std::string> args = split(cmdLine, " ");
  if (args.size() <= 1)
  {
    return 0;
  }

  // std::string 转 int
  std::stringstream code_stream(args[1]);
  int code = 0;
  code_stream >> code;

  // 转换失败
  if (!code_stream.eof() || code_stream.fail())
  {
    std::cout << "Invalid exit code\n";
    return -1;
  }

  return code;
}

bool IsBuiltInCmd(std::string cmdLine){
  std::vector<std::string> args = split(cmdLine, " ");
  return args[0] == "pwd" || args[0] == "cd";
}

void BuiltInCmdHandler(std::string cmdLine){
  std::vector<std::string> args = split(cmdLine, " ");

  if (args[0] == "pwd")
  {
    char cwd[PATH_MAX];
    if (getcwd(cwd, PATH_MAX) != NULL)
    {
      std::cout << cwd << "\n";
    }
    else
    {
      std::cout << "Error getting current directory\n";
    }
    return;
  }

  if (args[0] == "cd")
  {
    if (args.size() < 2)
    {
      char *home = getenv("HOME");
      if (home)
      {
        if (chdir(home) != 0)
        {
          std::cout << "cd: " << strerror(errno) << "\n";
        }
      }
      else
      {
        std::cout << "cd: HOME environment variable not set\n";
      }
    }
    else
    {
      if (chdir(args[1].c_str()) != 0)
      {
        std::cout << "cd: " << strerror(errno) << "\n";
      }
    }
    return;
  }

  return;
}

bool IsBackgroundCmd(std::string cmdLine){
  std::vector<std::string> args = split(cmdLine, " ");
  return args[args.size() - 1] == "&";
}

void RemoveBackgroundSymbol(std::string &cmdLine){
  cmdLine = cmdLine.substr(0, cmdLine.find_last_of("&"));
}

//需要在一个独立的子进程中被调用，因为使用了execvp
void SimpleCmdHandler(const std::string& cmdLine) {
  // 分割命令行参数
    std::vector<std::string> args = split(cmdLine, " ");
    // 将 std::string 转换为 char*
    char **argv = new char*[args.size() + 1];
    for(size_t i = 0; i < args.size(); i++){
        argv[i] = new char[args[i].size() + 1];
        strcpy(argv[i], args[i].c_str());
    }
    // execvp 需要以 nullptr 结尾
    argv[args.size()] = nullptr;
    // 执行外部命令
    execvp(argv[0], argv);
    // 如果 execvp 返回，说明出错
    std::cout << "Command not found\n";
    exit(1);
}

bool IsRedirect(std::string cmdLine){
  return cmdLine.find(">") != std::string::npos || cmdLine.find("<") != std::string::npos;
}

void RedirectCmdHandler(std::string cmdLine){
  std::vector<std::string> parts = split(cmdLine, " ");
  if (parts.size() < 3) {
    std::cerr << "error: cmdLine does not contain enough parts" << std::endl;
    return;
  }

  std::string command;
  std::string redirectionSymbol;
  std::string filename;
  int redirectionPos = -1;

  // Find the redirection symbol and its position
  for (size_t i = 0; i < parts.size(); ++i) {
    if (parts[i].find(">") != std::string::npos || parts[i].find("<") != std::string::npos) {
      redirectionSymbol = parts[i];
      filename = parts[i+1];
      redirectionPos = i;
      break;
    }
  }

  // If no redirection symbol is found, return an error
  if (redirectionPos == -1) {
    std::cerr << "error: no redirection symbol found" << std::endl;
    return;
  }

  // Construct the command with all parts before the redirection symbol
  for (int i = 0; i < redirectionPos; ++i) {
    command += parts[i] + " ";
  }

  int fd;
  int oldfd;
  int fdRedirected;
  int customFd = -1;

  if(redirectionSymbol.find(">") != std::string::npos){
    fdRedirected = 1;
  }else
  {
    fdRedirected = 0;
  }

  // Check if the redirection symbol starts with a number (file descriptor)
  if (isdigit(redirectionSymbol[0])) {
    customFd = std::stoi(redirectionSymbol.substr(0, redirectionSymbol.find_first_not_of("0123456789")));
    redirectionSymbol = redirectionSymbol.back();
  }

  fdRedirected = customFd != -1 ? customFd : fdRedirected;

  if (redirectionSymbol == ">") {
    fd = open(filename.c_str(), O_WRONLY | O_TRUNC | O_CREAT, 0666);
    oldfd = dup(fdRedirected);
    dup2(fd, fdRedirected);
  } else if (redirectionSymbol == ">>") {
    fd = open(filename.c_str(), O_WRONLY | O_APPEND | O_CREAT, 0666);
    oldfd = dup(fdRedirected);
    dup2(fd, fdRedirected);
  } else if (redirectionSymbol == "<") {
    fd = open(filename.c_str(), O_RDONLY);
    oldfd = dup(fdRedirected);
    dup2(fd, fdRedirected);
  } else if (redirectionSymbol == "<<<") {
    fd = open("/tmp/tempfile", O_WRONLY | O_TRUNC | O_CREAT, 0666);
    std::string input = filename + "\n";
    write(fd, input.c_str(), input.size());
    close(fd);
    fd = open("/tmp/tempfile", O_RDONLY);
    oldfd = dup(0);
    dup2(fd, 0);
  } else if (redirectionSymbol == "<<") {
      // Handle EOF redirection
      // This is a simplified version and does not handle all cases
      fd = open("/tmp/tempfile", O_WRONLY | O_TRUNC | O_CREAT, 0666);
      std::string line;
      while (std::getline(std::cin, line) && line != "EOF") {
          line += '\n';
          write(fd, line.c_str(), line.size());
      }
      close(fd);
      fd = open("/tmp/tempfile", O_RDONLY);
      oldfd = dup(0);
      dup2(fd, 0);
  } else {
    std::cerr << "error: unknown redirection symbol" << std::endl;
    return;
  }

  pid_t pid = fork();
  if (pid == 0) {
    // Child process
    SimpleCmdHandler(command);
    exit(0);
  } else if (pid > 0) {
    // Parent process
    waitpid(pid, nullptr, 0);
    dup2(oldfd, fdRedirected);
    close(fd);
    close(oldfd);
  } else {
    // Fork failed
    std::cerr << "error: fork failed" << std::endl;
    return;
  }
}

bool IsPipe(std::string cmdLine){
  return cmdLine.find("|") != std::string::npos;
}

void PipeCmdHandler(std::string cmdLine){
  std::vector<std::string> cmds = split(cmdLine, "|");
  if (cmds.size() < 2) {
    std::cerr << "error: cmdLine does not contain enough parts" << std::endl;
    return;
  }

  int pipefd[2];
  int oldfd = 0;

  for (size_t i = 0; i < cmds.size(); ++i) {
    if (pipe(pipefd) == -1) {
      std::cerr << "error: pipe failed" << std::endl;
      return;
    }

    pid_t pid = fork();
    if (pid == 0) {
      // Child process
      dup2(oldfd, 0);
      if (i < cmds.size() - 1) {
        dup2(pipefd[1], 1);
      }
      close(pipefd[0]);
      if(IsRedirect(cmds[i])){
        RedirectCmdHandler(cmds[i]);
        exit(0);
      }else{
        SimpleCmdHandler(cmds[i]);
      }
    } else if (pid > 0) {
      // Parent process
      waitpid(-1, nullptr, 0);
      close(pipefd[1]);
      oldfd = pipefd[0];
    } else {
      // Fork failed
      std::cerr << "error: fork failed" << std::endl;
      return;
    }
  }
}

int main()
{
  // 不同步 iostream 和 cstdio 的 buffer
  std::ios::sync_with_stdio(false);

  // 用来存储读入的一行命令
  std::string cmdLine;

  // // 用来存储被管道分隔符分割的cmdLine
  // std::vector<std::string> cmds;

  // // 用来存储被重定向分隔符分割的cmd
  // std::vector<std::string> cmdParts; 

  // // 用来存储被空格分隔符分割cmdParts的命令行参数
  // std::vector<std::string> args;

//Main Loop
  while (true)
  {

    // 打印提示符
    PrintPrompt();

    // 读入一行。std::getline 结果不包含换行符。
    std::getline(std::cin, cmdLine);

    // 如果输入为空，继续下一轮循环
    if(IsEmptyCmd(cmdLine)){
      continue;
    }

    // 如果是exit命令，退出
    if(IsExit(cmdLine)){
      return ExitHandler(cmdLine);
    }

    // 如果是exit外的其他内建命令，执行内建命令
    if(IsBuiltInCmd(cmdLine)){
      BuiltInCmdHandler(cmdLine);
      continue;
    }

    bool isBackground = IsBackgroundCmd(cmdLine);
    if(isBackground){
      RemoveBackgroundSymbol(cmdLine);
    }

    // //处理外部命令
    // pid_t pid = fork();

    // if(pid == 0){
    //   //子进程0，用于执行外部命令
    //   if(IsPipe(cmdLine)){
    //     PipeCmdHandler(cmdLine);
    //   }
    //   else if(IsRedirect(cmdLine)){
    //     RedirectCmdHandler(cmdLine);
    //   }
    //   else{
    //     //不包含管道和重定向符号的简单外部命令
    //     SimpleCmdHandler(cmdLine);
    //   }
    // }
    // else{
    //   //父进程
    //   if(!isBackground){
    //     int ret = waitpid(pid, nullptr, 0);
    //     if(ret < 0){
    //       std::cout << "wait failed\n";
    //     }
    //   }
    // }

    //debug
      if(IsPipe(cmdLine)){
        PipeCmdHandler(cmdLine);
      }
      else if(IsRedirect(cmdLine)){
        RedirectCmdHandler(cmdLine);
      }
      else{
        //不包含管道和重定向符号的简单外部命令
        SimpleCmdHandler(cmdLine);
      }
  }
}

std::string trim(const std::string& str)
{
    size_t first = str.find_first_not_of(' ');
    if (std::string::npos == first)
    {
        return str;
    }
    size_t last = str.find_last_not_of(' ');
    return str.substr(first, (last - first + 1));
}

// 经典的 cpp string split 实现
// https://stackoverflow.com/a/14266139/11691878
std::vector<std::string> split(std::string s, const std::string &delimiter)
{
    std::vector<std::string> res;
    size_t pos = 0;
    std::string token;

    s = trim(s);  // trim the string

    while ((pos = s.find(delimiter)) != std::string::npos)
    {
        token = s.substr(0, pos);
        token = trim(token);  // trim the token
        if (!token.empty())
            res.push_back(token);
        s = s.substr(pos + delimiter.length());
    }
    s = trim(s);  // trim the last part
    res.push_back(s);
    return res;
}