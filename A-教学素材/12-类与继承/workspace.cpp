/**
 * @file workspace.cpp
 * @author HiBer2007
 * @details 工作区相关的工具类与函数，用于本地的工作区维护
 */

// Including necessary Qt headers
#include <QFile>      // 文件读写
#include <QDir>       // 目录操作
#include <QFileInfo>  // 获取文件属性
#include <QTextStream> // 文本流读写
#include <QDataStream> // 二进制数据读写
#include <QProcess> //进程调用

// Including standard library headers
#include <iostream>
#include <algorithm>
#include <vector>
#include <queue>
#include <string>

// Including the JSON utility header
#include <nlohmann/json.hpp>

// Including utils
#include <lasterrorhandler.h>

// NameSpace settings
using json = nlohmann::json;

namespace Workspace {

    /**
     * @class WorkspaceManager
     * @details 用于创建工作区，读写工作区。其类型为实例。
     */
    class WorkspaceManager {
        private:
            std::string WorkspaceDIR; ///<Workspace的根目录，所有的工作区文件都在此目录下
            json WorkspaceConfig;     ///<Workspace的配置信息
            std::string RemoteGitDIR; ///<Workspace的远程Git仓库目录，若为空则表示没有启用远程仓库
            LastError *PLastError;     ///<最后的报错信息对应的实例指针

            /**
             * @brief 执行本地Git命令，采用同步模式，阻塞执行
             * @param GitAddress Git的地址，作为命令的第一个路径值，如果使用path中的，可以直接传递"git"
             * @param ArgsString 字符串类型Vector的参数表
             * @param ProcessOutputBufferString 传递为std::string的引用，用于接受程序的输出流
             * @param ProcessErrorBufferString 传递为std::string的引用，用于接受程序的错误流
             * @return int 传递进程返回值。如果返回值为-1，则表示执行失败，具体的错误信息可以通过LastError获取
             */
            int ExecuteGitCommand(const std::string &GitAddress, const std::vector<std::string> &ArgsString, std::string &ProcessOutputBufferString, std::string &ProcessErrorBufferString)
            {
                QProcess process;
                QStringList args;
                for (const auto &arg : ArgsString)
                {
                    args << QString::fromStdString(arg);
                }

                process.start(QString::fromStdString(GitAddress), args);
                if (!process.waitForFinished())
                {
                    // ---------- 错误分类 ----------
                    int errorCode = 0;

                    // 1. 检查是否超时（进程仍在运行）
                    if (process.state() == QProcess::Running)
                    {
                        process.kill();            // 强制终止
                        process.waitForFinished(); // 等待终止完成
                        errorCode = 1001;          // 超时
                    }
                    // 2. 检查进程是否崩溃（启动后崩溃）
                    else if (process.exitStatus() == QProcess::CrashExit)
                    {
                        errorCode = 1003; // 进程崩溃
                    }
                    // 3. 检查 QProcess 自身错误
                    else
                    {
                        QProcess::ProcessError err = process.error();
                        switch (err)
                        {
                        case QProcess::FailedToStart:
                            errorCode = 1002; // 启动失败
                            break;
                        case QProcess::Timedout:
                            errorCode = 1001; // 超时（理论上前面已捕获，但做安全兜底）
                            break;
                        case QProcess::WriteError:
                            errorCode = 1004; // 写入错误
                            break;
                        case QProcess::ReadError:
                            errorCode = 1005; // 读取错误
                            break;
                        case QProcess::UnknownError:
                        default:
                            errorCode = 0; // 未知错误
                            break;
                        }
                    }
                    return -1;
                    HeapReplaceLastError(PLastError, errorCode);
                }

                ProcessOutputBufferString = process.readAllStandardOutput().toStdString();
                ProcessErrorBufferString = process.readAllStandardError().toStdString();
                return process.exitCode();
            }

            /**
             * @brief 同步远程仓库到Git目录。如果未初始化工作区，则设置工作区。
             * @param dir 远程Git仓库的目录
             * @param url 远程Git仓库的地址，HTTP协议
             * 
             */
            bool SynGitRepository(const std::string &dir, const std::string &url)
            {
                // 检查工作区是否未初始化
                bool isInitialized = IsGitRepositoryInitialized(dir);

                if (isInitialized)
                {
                    // 如果工作区未初始化，则执行 git clone
                    std::vector<std::string> args = {"clone", url, dir};
                    std::string output, error;
                    int exitCode = ExecuteGitCommand("git", args, output, error);
                    if (exitCode != 0)
                    {
                        HeapReplaceLastError(PLastError, 1003); // Git命令执行失败
                        return false;
                    }
                }
                else
                {
                    // 如果工作区已初始化，则执行 git pull
                    std::vector<std::string> args = {"-C", dir, "pull"};
                    std::string output, error;
                    int exitCode = ExecuteGitCommand("git", args, output, error);
                    if (exitCode != 0)
                    {
                        HeapReplaceLastError(PLastError, 1003); // Git命令执行失败
                        return false;
                    }
                }
                return true;
            }

            /**
             * @brief 检查指定目录是否是一个已初始化的 Git 仓库（存在 .git 目录）
             * @param dir 要检查的目录路径
             * @return true 如果是一个有效的 Git 仓库，否则 false
             */
            bool IsGitRepositoryInitialized(const std::string &dir)
            {
                // 使用 git -C <dir> rev-parse --git-dir 检测
                std::vector<std::string> args = {"-C", dir, "rev-parse", "--git-dir"};
                std::string output, error;
                int exitCode = ExecuteGitCommand("git", args, output, error);
                // 返回码为 0 表示成功（即存在 .git）
                return exitCode == 0;
            }

        public:

            /**
             * @brief 构造函数A，传递工作区的根目录，配置信息，远程Git仓库目录
             * @param dir 工作区的根目录
             * @param config 工作区的配置信息
             * @param remoteGitDir 远程Git仓库目录，若为空则表示没有启用远程仓库
             */
            WorkspaceManager(const std::string& dir, const json& config, const std::string& remoteGitDir = "") : WorkspaceDIR(dir), WorkspaceConfig(config), RemoteGitDIR(remoteGitDir) {
                // Constructor implementation
            }

            /**
             * @brief 构造函数B，传递工作区的根目录，配置文件目录，远程Git仓库目录
             * @param dir 工作区的根目录
             * @param configDIR 配置文件的目录
             * @param remoteGitDir 远程Git仓库目录，若为空则表示没有启用远程仓库
             */
            WorkspaceManager(const std::string& dir, const std::string& configDIR, const std::string& remoteGitDir = "") : WorkspaceDIR(dir), RemoteGitDIR(remoteGitDir) {
                // Load configuration from file
                QFile configFile(QString::fromStdString(configDIR));
                if (configFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
                    QTextStream in(&configFile);
                    std::string configContent = in.readAll().toStdString();
                    WorkspaceConfig = json::parse(configContent);
                    configFile.close();
                } else {
                    std::cerr << "Failed to open configuration file: " << configDIR << std::endl;
                }
            }

            /**
             * @details 禁用默认构造函数，必须提供构造值
             * @deprecated
             */
            WorkspaceManager() = delete;

            /**
             * @brief 析构函数，清理工作区管理器
             */
            ~WorkspaceManager() {
                // Destructor implementation
            }
    
            /**
             * @details 获取最后的错误信息实例
             * @return LastError 最后的错误实例
             */
            LastError GetLastError(){
                return *PLastError;
            }
    
            


    };

} // namespace Workspace