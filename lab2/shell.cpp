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

std::vector<std::string> split(std::string s, const std::string &delimiter);

void prompt() {
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
    std::cout << "[MyShell]" << username << "@" << hostname << ":" << path << prompt << " ";
}

int main() {
  // 不同步 iostream 和 cstdio 的 buffer
  std::ios::sync_with_stdio(false);

  // 用来存储读入的一行命令
  std::string cmd;
  while (true) {

    // 打印提示符
    prompt();

    // 读入一行。std::getline 结果不包含换行符。
    std::getline(std::cin, cmd);

    // 按空格分割命令为单词
    std::vector<std::string> args = split(cmd, " ");

    // 没有可处理的命令
    if (args.empty()) {
      continue;
    }

    // 退出
    if (args[0] == "exit") {
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
        continue;
      }

      return code;
    }

    if (args[0] == "pwd") {
        char cwd[PATH_MAX];
        if (getcwd(cwd, PATH_MAX) != NULL) {
            std::cout << cwd << "\n";
        } else {
            std::cout << "Error getting current directory\n";
        }
        continue;
    }

    if (args[0] == "cd") {
        if (args.size() < 2) {
            char* home = getenv("HOME");
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
        continue;
    }

    // 处理外部命令
    pid_t pid = fork();

    // std::vector<std::string> 转 char **
    char *arg_ptrs[args.size() + 1];
    for (auto i = 0; i < args.size(); i++) {
      arg_ptrs[i] = &args[i][0];
    }
    // exec p 系列的 argv 需要以 nullptr 结尾
    arg_ptrs[args.size()] = nullptr;

    if (pid == 0) {
      // 这里只有子进程才会进入
      // execvp 会完全更换子进程接下来的代码，所以正常情况下 execvp 之后这里的代码就没意义了
      // 如果 execvp 之后的代码被运行了，那就是 execvp 出问题了
      execvp(args[0].c_str(), arg_ptrs);

      // 所以这里直接报错
      exit(255);
    }

    // 这里只有父进程（原进程）才会进入
    int ret = wait(nullptr);
    if (ret < 0) {
      std::cout << "wait failed";
    }
  }
}

// 经典的 cpp string split 实现
// https://stackoverflow.com/a/14266139/11691878
std::vector<std::string> split(std::string s, const std::string &delimiter) {
  std::vector<std::string> res;
  size_t pos = 0;
  std::string token;
  while ((pos = s.find(delimiter)) != std::string::npos) {
    token = s.substr(0, pos);
    if(!token.empty())
      res.push_back(token);
    s = s.substr(pos + delimiter.length());
  }
  res.push_back(s);
  return res;
}