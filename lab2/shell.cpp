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
#include <fcntl.h>
#include <pwd.h>
#include <signal.h>
#include <string.h>
#include <sys/wait.h>

std::vector<std::string> split(std::string s, const std::string &delimiter);

std::string trim(const std::string &str);

void PrintPrompt();

bool IsEmptyCmd(std::string cmdLine);

bool IsExit(std::string cmdLine);

int ExitHandler(std::string cmdLine);

bool IsBuiltInCmd(std::string cmdLine);

void BuiltInCmdHandler(std::string cmdLine);

bool IsBackgroundCmd(std::string cmdLine);

void RemoveBackgroundSymbol(std::string &cmdLine);

void SimpleCmdHandler(const std::string &cmdLine);

bool IsRedirect(std::string cmdLine);

void RedirectCmdHandler(std::string cmdLine);

bool IsPipe(std::string cmdLine);

void PipeCmdHandler(std::string cmdLine);

void sigint_handler(int signum);

void process_bgs(std::vector<pid_t> &bg_pids);

void hide_inout();

void wait(std::vector<pid_t> &bg_pids);

bool IsProcessing;

std::vector<pid_t> bg_pids; // 用于存储后台进程的pid

int main() {
  // 信号处理
  struct sigaction shell, child,
      ign; // shell进程需要忽略SIGINT,子进程需要处理SIGINT

  shell.sa_flags = 0;
  shell.sa_handler = sigint_handler;
  ign.sa_flags = 0;
  ign.sa_handler = SIG_IGN;

  sigaction(SIGINT, &shell, &child);
  sigaction(SIGTTOU, &ign, nullptr);

  // 用来存储读入的一行命令
  std::string cmdLine;

  // 不同步 iostream 和 cstdio 的 buffer
  std::ios::sync_with_stdio(false);

  while (true) {
    process_bgs(bg_pids); // 处理后台进程

    IsProcessing = false;

    // 打印提示符
    PrintPrompt();

    // 读入一行。std::getline 结果不包含换行符。
    std::getline(std::cin, cmdLine);

    // 如果输入为空，继续下一轮循环
    if (IsEmptyCmd(cmdLine)) {
      continue;
    }

    // 如果是exit命令，退出
    if (IsExit(cmdLine)) {
      return ExitHandler(cmdLine);
    }

    // 如果是exit外的其他内建命令，执行内建命令
    if (IsBuiltInCmd(cmdLine)) {
      BuiltInCmdHandler(cmdLine);
      continue;
    }

    bool isBackground = IsBackgroundCmd(cmdLine);
    if (isBackground) {
      RemoveBackgroundSymbol(cmdLine);
    }

    // 处理外部命令
    pid_t pid = fork();

    IsProcessing = true;

    if (pid == 0) {
      // 子进程不能忽略SIG_IGN，故需要恢复默认行为
      sigaction(SIGINT, &child, nullptr);
      // signal(SIGTTOU,SIG_DFL);
      if (isBackground) {
        hide_inout();
      }
      // 子进程0，用于执行外部命令
      if (IsPipe(cmdLine)) {
        PipeCmdHandler(cmdLine);
        exit(0);
      } else if (IsRedirect(cmdLine)) {
        RedirectCmdHandler(cmdLine);
        exit(0);
      } else {
        // 不包含管道和重定向符号的简单外部命令
        SimpleCmdHandler(cmdLine);
      }
    } else {
      // 这里只有父进程（原进程）才会进入
      if (isBackground) {
        bg_pids.push_back(pid);
        continue;
      }
      setpgid(pid, pid); // 将进程组id设置为子进程id
      tcsetpgrp(0, pid); // 将前台进程组设置为子进程的进程组
      sigaction(SIGTTOU, &ign, nullptr);
      int status; // 表示子进程是否结束
      // 等待子进程pid结束
      int ret = waitpid(
          pid, &status,
          0); // 这里需要修改为waitpid，以知道子程序是正常结束还是因为接收到信号而结束
      tcsetpgrp(0, getpgrp()); // 将前台进程组设置为shell进程的进程组
      // int ret = wait(&over);
      if (ret < 0) {
        std::cout << "wait failed";
      } else if (WIFSIGNALED(status)) {
        // 子程序因接收到信号而结束时，需要输出换行符（正常结束时不需要做任何处理）
        std::cout << std::endl;
      }
    }
  }
  return 0;
}

std::string trim(const std::string &str) {
  size_t first = str.find_first_not_of(' ');
  if (std::string::npos == first) {
    return str;
  }
  size_t last = str.find_last_not_of(' ');
  return str.substr(first, (last - first + 1));
}

// 经典的 cpp string split 实现
// https://stackoverflow.com/a/14266139/11691878
std::vector<std::string> split(std::string s, const std::string &delimiter) {
  std::vector<std::string> res;
  size_t pos = 0;
  std::string token;

  s = trim(s); // trim the string

  while ((pos = s.find(delimiter)) != std::string::npos) {
    token = s.substr(0, pos);
    token = trim(token); // trim the token
    if (!token.empty())
      res.push_back(token);
    s = s.substr(pos + delimiter.length());
  }
  s = trim(s); // trim the last part
  res.push_back(s);
  return res;
}

void PrintPrompt() {
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
  if (path.find(home) == 0) {
    path.replace(0, home.length(), "~");
  }

  // 使用geteuid()检查用户权限，如果是0，使用"#"作为提示符，否则使用"$"
  const char *prompt = (geteuid() == 0) ? "#" : "$";

  // 打印提示符
  std::cout << "[MyShell]" << username << "@" << hostname << ":" << path
            << prompt << " ";
}

bool IsEmptyCmd(std::string cmdLine) { return cmdLine.empty(); }

bool IsExit(std::string cmdLine) {
  std::vector<std::string> args = split(cmdLine, " ");
  return args[0] == "exit";
}

int ExitHandler(std::string cmdLine) {
  std::vector<std::string> args = split(cmdLine, " ");
  if (args.size() <= 1) {
    return 0;
  }

  // std::string 转 int
  std::stringstream code_stream(args[1]);
  int code = 0;
  code_stream >> code;

  // 转换失败
  if (!code_stream.eof() || code_stream.fail()) {
    std::cout << "Invalid exit code\n";
    return -1;
  }

  return code;
}

bool IsBuiltInCmd(std::string cmdLine) {
  std::vector<std::string> args = split(cmdLine, " ");
  return args[0] == "pwd" || args[0] == "cd" || args[0] == "wait";
}

void BuiltInCmdHandler(std::string cmdLine) {
  std::vector<std::string> args = split(cmdLine, " ");

  if (args[0] == "pwd") {
    char cwd[PATH_MAX];
    if (getcwd(cwd, PATH_MAX) != NULL) {
      std::cout << cwd << "\n";
    } else {
      std::cout << "Error getting current directory\n";
    }
    return;
  }

  if (args[0] == "cd") {
    if (args.size() < 2) {
      char *home = getenv("HOME");
      if (home) {
        if (chdir(home) != 0) {
          std::cout << "cd: " << strerror(errno) << "\n";
        }
      } else {
        std::cout << "cd: HOME environment variable not set\n";
      }
    } else {
      if (chdir(args[1].c_str()) != 0) {
        std::cout << "cd: " << strerror(errno) << "\n";
      }
    }
    return;
  }

  if (args[0] == "wait") {
    if (args.size() > 1) {
      std::cout << "Too many arguments!\n";
      return;
    }
    wait(bg_pids);
    return;
  }

  return;
}

bool IsBackgroundCmd(std::string cmdLine) {
  std::vector<std::string> args = split(cmdLine, " ");
  return args[args.size() - 1] == "&";
}

void RemoveBackgroundSymbol(std::string &cmdLine) {
  cmdLine = cmdLine.substr(0, cmdLine.find_last_of("&"));
}

// 需要在一个独立的子进程中被调用，因为使用了execvp
void SimpleCmdHandler(const std::string &cmdLine) {
  // 分割命令行参数
  std::vector<std::string> args = split(cmdLine, " ");
  // 将 std::string 转换为 char*
  char **argv = new char *[args.size() + 1];
  for (size_t i = 0; i < args.size(); i++) {
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

bool IsRedirect(std::string cmdLine) {
  return cmdLine.find(">") != std::string::npos ||
         cmdLine.find("<") != std::string::npos;
}

void RedirectCmdHandler(std::string cmdLine) {
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
    if (parts[i].find(">") != std::string::npos ||
        parts[i].find("<") != std::string::npos) {
      redirectionSymbol = parts[i];
      filename = parts[i + 1];
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

  if (redirectionSymbol.find(">") != std::string::npos) {
    fdRedirected = 1;
  } else {
    fdRedirected = 0;
  }

  // Check if the redirection symbol starts with a number (file descriptor)
  if (isdigit(redirectionSymbol[0])) {
    customFd = std::stoi(redirectionSymbol.substr(
        0, redirectionSymbol.find_first_not_of("0123456789")));
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

bool IsPipe(std::string cmdLine) {
  return cmdLine.find("|") != std::string::npos;
}

void PipeCmdHandler(std::string cmdLine) {
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
      if (IsRedirect(cmds[i])) {
        RedirectCmdHandler(cmds[i]);
        // 子进程执行完毕后，直接退出
        exit(0);
      } else {
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

void sigint_handler(int signum) {
  // 清空输入流中的残留数据并重置输入流状态
  std::cin.ignore(std::cin.rdbuf()->in_avail());
  // 开始下一行
  std::cout << "\n";
  if (!IsProcessing) {
    PrintPrompt();
  };
  // 清空输出缓冲区
  std::cout.flush();
}

// 处理后台进程，删除bg_pids中已经结束的命令，防止出现僵尸进程
void process_bgs(std::vector<pid_t> &bg_pids) {
  for (int i = bg_pids.size() - 1; i >= 0; i--) {
    int status;
    if (waitpid(bg_pids[i], &status, WNOHANG) > 0) {
      bg_pids.erase(bg_pids.begin() + i);
    }
  }
}

void hide_inout() {
  int null_fd = open("/dev/null", O_RDWR);
  if (null_fd < 0) {
    std::cout << "open /dev/null failed\n";
    return;
  }
  dup2(null_fd, 0);
  dup2(null_fd, 1);
  dup2(null_fd, 2);
  close(null_fd);
}

void wait(std::vector<pid_t> &bg_pids) {
  for (size_t i = 0; i < bg_pids.size(); i++) {
    int status;
    waitpid(bg_pids[i], &status, 0);
    if (WIFEXITED(status)) {
      std::cout << "Process " << bg_pids[i] << " exited with status "
                << WEXITSTATUS(status) << std::endl;
    } else if (WIFSIGNALED(status)) {
      std::cout << "Process " << bg_pids[i] << " terminated by signal "
                << WTERMSIG(status) << std::endl;
    }
  }
}