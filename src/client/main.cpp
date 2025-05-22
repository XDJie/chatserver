#include "json.hpp"
#include <iostream>
#include <thread>
#include <string>
#include <vector>
#include <chrono>
#include <ctime>
using namespace std;
using json = nlohmann::json;

#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "group.hpp"
#include "user.hpp"
#include "public.hpp"

// 记录当前系统登陆的用户信息
User g_currentUser;
// 记录当前用户的好友列表
vector<User> g_currentUserFriendList;
// 记录当前用户的群组列表
vector<Group> g_currentUserGroupList;
// 显示当前登录成功用户的基本信息
void showCurrentUserData();
// 控制主菜单页面程序
bool isMainMenuRunning = false;

// 接收线程
void readTaskHandler(int clintfd);
// 获取系统时间（聊天信息需要添加时间信息）
string getCurrentTime();
// 主聊天页面程序
void mainMenu(int clintfd);

// 客户端程序实现，main线程用作发送线程，子线程用作接收线程（聊天的时候不能阻塞消息接收）
int main(int argc, char **argv)
{
    if (argc < 3)
    {
        cerr << "command invalid! example: ./ChatClient 127.0.0.1 6000" << endl;
        exit(-1);
    }

    // 解析通过命令行参数传递的ip和port
    char *ip = argv[1];
    int port = atoi(argv[2]);

    // 创建client的socket
    int clientfd = socket(AF_INET, SOCK_STREAM, 0);
    if (-1 == clientfd)
    {
        cerr << "socket create error!" << endl;
        exit(-1);
    }

    // 填写client需要连接的server信息ip+port（绑定服务器）
    sockaddr_in serveraddr;
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_port = htons(port);
    serveraddr.sin_addr.s_addr = inet_addr(ip);

    // 连接服务器
    if (-1 == connect(clientfd, (sockaddr *)&serveraddr, sizeof(serveraddr)))
    {
        cerr << "connect server error!" << endl;
        close(clientfd);
        exit(-1);
    }

    // 连接成功，main线程用于接收用户输入，发送数据
    for (;;)
    {
        // 显示首页面菜单 登录 注册 退出
        cout << "========================" << endl;
        cout << "1. login" << endl;
        cout << "2. register" << endl;
        cout << "3. quit" << endl;
        cout << "========================" << endl;
        cout << "choice:";
        int choice = 0;
        cin >> choice;
        cin.get(); // 清除换行符

        switch (choice)
        {
        case 1:
        {
            int id = 0;
            char pwd[50] = {0};
            cout << "userid:";
            cin >> id;
            cin.get();
            cout << "userpassword:";
            cin.getline(pwd, 50);

            json js;
            js["msgid"] = LOGIN_MSG;
            js["id"] = id;
            js["password"] = pwd;
            string request = js.dump();

            // 通过网络发送登录请求
            int len = send(clientfd, request.c_str(), request.size(), 0);
            if (len == -1)
            {
                cerr << "send login msg error:" << request << endl;
            }
            else
            {
                char buffer[1024] = {0};
                len = recv(clientfd, buffer, 1024, 0);
                if (-1 == len)
                {
                    cerr << "recv login resonse error" << endl;
                }
                else
                {
                    json responsejs = json::parse(buffer);
                    if (0 != responsejs["errno"].get<int>()) // 登陆失败
                    {
                        cerr << responsejs["errmsg"] << endl;
                    }
                    else // 登陆成功
                    {
                        // 记录当前用户的基本信息
                        g_currentUser.setID(responsejs["id"].get<int>());
                        g_currentUser.setName(responsejs["name"]);

                        // 记录当前用户的好友列表
                        if (responsejs.contains("friends"))
                        {
                            // 初始化
                            g_currentUserFriendList.clear();

                            vector<string> vec = responsejs["friends"];
                            for (string &str : vec)
                            {
                                json js = json::parse(str);
                                User user;
                                user.setID(js["id"].get<int>());
                                user.setName(js["name"]);
                                user.setState(js["state"]);
                                g_currentUserFriendList.push_back(user);
                            }
                        }

                        // 记录当前用户的群组列表 g_currentUserGroupList
                        if (responsejs.contains("groups"))
                        {
                            // 初始化
                            g_currentUserGroupList.clear();

                            vector<string> vec1 = responsejs["groups"];
                            for (string &grpstr : vec1)
                            {
                                json grpjs = json::parse(grpstr);
                                Group group;
                                group.setID(grpjs["id"].get<int>());
                                group.setName(grpjs["groupname"]);
                                group.setDesc(grpjs["groupdesc"]);
                                // 记录群组成员
                                vector<string> vec2 = grpjs["users"];
                                for (string &str : vec2)
                                {
                                    json js = json::parse(str);
                                    GroupUser user;
                                    user.setID(js["id"].get<int>());
                                    user.setName(js["name"]);
                                    user.setState(js["state"]);
                                    user.setRole(js["role"]);
                                    group.getUsers().push_back(user);
                                }
                                g_currentUserGroupList.push_back(group);
                            }
                        }

                        // 显示当前用户的基本信息
                        showCurrentUserData();

                        // 显示当前用户的离线消息 个人聊天信息或群组聊天信息
                        if (responsejs.contains("offlinemsg"))
                        {
                            vector<string> vec = responsejs["offlinemsg"];
                            for (string &str : vec)
                            {
                                json js = json::parse(str);
                                // time + [id] + name + " said: " + xxx
                                int msgtype = js["msgid"].get<int>();
                                if (ONE_CHAT_MSG == msgtype)
                                {
                                    cout << js["time"].get<string>() << " [" << js["id"] << "] " << js["name"].get<string>()
                                         << " said: " << js["msg"].get<string>() << endl;
                                }

                                if (GROUP_CHAT_MSG == msgtype)
                                {
                                    cout << "群消息[" << js["groupid"] << "]:" << js["time"].get<string>() << " [" << js["id"] << "] " << js["name"].get<string>()
                                         << " said: " << js["msg"].get<string>() << endl;
                                }
                            }
                        }

                        static int threadnumber = 0;
                        if(threadnumber == 0)
                        {
                            // 登陆成功，启动接收线程接收数据，该线程只启动一次
                            std::thread readTask(readTaskHandler, clientfd);
                            // 主线程无需阻塞等待子线程结束
                            readTask.detach();
                            threadnumber++;
                        }

                        // 登录成功
                        isMainMenuRunning = true;
                        // 进入聊天主菜单页面
                        mainMenu(clientfd);
                    }
                }
            }
        }
        break;
        case 2:
        {
            char name[50] = {0};
            char pwd[50] = {0};
            cout << "username:";
            cin.getline(name, 50);
            cout << "userpassword:";
            cin.getline(pwd, 50);

            json js;
            js["msgid"] = REG_MSG;
            js["name"] = name;
            js["password"] = pwd;
            string request = js.dump();

            // 通过网络发送注册请求
            int len = send(clientfd, request.c_str(), request.size(), 0);
            if (len == -1)
            {
                cerr << "send reg msg error:" << request << endl;
            }
            else
            {
                char buffer[1024] = {0};
                // 阻塞等待响应，将响应json数据存储在buffer中
                len = recv(clientfd, buffer, 1024, 0);
                if (-1 == len)
                {
                    cerr << "recv reg response error" << endl;
                }
                else
                {
                    json responsejs = json::parse(buffer);
                    if (0 != responsejs["errno"].get<int>()) // 注册失败
                    {
                        cout << name << " is already exist, register error!" << endl;
                    }
                    else // 注册成功
                    {
                        cout << name << " register success, userid is " << responsejs["id"] << ", do not forget it!" << endl;
                    }
                }
            }
        }
        break;
        case 3:
            close(clientfd);
            exit(0);
        default:
            cout << "invalid input!" << endl;
            break;
        }
    }

    return 0;
}

void readTaskHandler(int clintfd)
{
    for (;;)
    {
        char buffer[1024] = {0};
        int len = recv(clintfd, buffer, 1024, 0);
        if (-1 == len || 0 == len)
        {
            close(clintfd);
            exit(-1);
        }

        // 接收ChatServer转发的数据，反序列化生成json数据对象
        json js = json::parse(buffer);
        int msgtype = js["msgid"].get<int>();
        if (ONE_CHAT_MSG == msgtype)
        {
            cout << js["time"].get<string>() << "[" << js["id"] << "]" << js["name"].get<string>()
                 << "said: " << js["msg"].get<string>() << endl;
            continue;
        }

        if (GROUP_CHAT_MSG == msgtype)
        {
            cout << "群消息[" << js["groupid"] << "]:" << js["time"].get<string>() << "[" << js["id"] << "]" << js["name"].get<string>()
                 << "said: " << js["msg"].get<string>() << endl;
            continue;
        }
    }
}

void showCurrentUserData()
{
    cout << "========================login user========================" << endl;
    cout << "current login user => id:" << g_currentUser.getID() << " name:" << g_currentUser.getName() << endl;
    cout << "------------------------friend list------------------------" << endl;
    if (!g_currentUserFriendList.empty())
    {
        for (User &user : g_currentUserFriendList)
        {
            cout << user.getID() << " " << user.getName() << " " << user.getState() << endl;
        }
    }
    cout << "-----------------------group list------------------------" << endl;
    if (!g_currentUserGroupList.empty())
    {
        for (Group &group : g_currentUserGroupList)
        {
            // 先输出群组信息
            cout << group.getID() << " " << group.getName() << " " << group.getDesc() << endl;
            // 在输出群组中所有成员的信息
            for (GroupUser &user : group.getUsers())
            {
                cout << user.getID() << " " << user.getName() << " " << user.getState() << " " << user.getRole() << endl;
            }
        }
    }
    cout << "==========================================================" << endl;
}

// "help" command handler
void help(int fd = 0, string = "");
// "chat" command handler
void chat(int, string);
// "addfriend" command handler
void addfriend(int, string);
// "creategroup" command handler
void creategroup(int, string);
// "addgroup" command handler
void addgroup(int, string);
// "groupchat" command handler
void groupchat(int, string);
// "loginout" command handler
void loginout(int, string);

// 系统支持的客户端命令列表
unordered_map<string, string> commandMap = {
    {"help", "显示所有支持的命令, 格式help"},
    {"chat", "一对一聊天, 格式chat::friendid::message"},
    {"addfriend", "添加好友, 格式addfriend::friendid"},
    {"creategroup", "创建群组, 格式creategroup::groupname::groupdesc"},
    {"addgroup", "加入群组, 格式addgroup::groupid"},
    {"groupchat", "群聊, 格式groupchat::groupid::message"},
    {"loginout", "注销, 格式loginout"}};

// 注册系统支持的客户端命令处理
unordered_map<string, function<void(int, string)>> commandHandlerMap = {
    {"help", help},
    {"chat", chat},
    {"addfriend", addfriend},
    {"creategroup", creategroup},
    {"addgroup", addgroup},
    {"groupchat", groupchat},
    {"loginout", loginout}};

void mainMenu(int clintfd)
{
    help();

    char buffer[1024] = {0};
    while(isMainMenuRunning)
    {
        cin.getline(buffer, 1024);
        string commandbuf(buffer);
        string command; // 存储命令
        int idx = commandbuf.find(":");
        if (-1 == idx)
        {
            command = commandbuf;
        }
        else
        {
            command = commandbuf.substr(0, idx);
        }

        auto it = commandHandlerMap.find(command);
        if (it == commandHandlerMap.end())
        {
            cerr << "invalid input command!" << endl;
            continue;
        }

        // 调用相应命令的事件处理回调，mainMenu对修改封闭，添加新功能不需要修改该函数
        it->second(clintfd, commandbuf.substr(idx + 1, commandbuf.size() - idx));
    }
}

// "help" command handler
void help(int, string)
{
    cout << "show command list >>> " << endl;
    for (auto &p : commandMap)
    {
        cout << p.first << " : " << p.second << endl;
    }
    cout << endl;
}

// "chat" command handler
void chat(int clientfd, string str)
{
    int idx = str.find(":");
    if (-1 == idx)
    {
        cerr << "char command invalid!" << endl;
        return;
    }

    int friendid = atoi(str.substr(0, idx).c_str());
    string message = str.substr(idx + 1, str.size() - idx);

    json js;
    js["msgid"] = ONE_CHAT_MSG;
    js["id"] = g_currentUser.getID();
    js["name"] = g_currentUser.getName();
    js["toid"] = friendid;
    js["msg"] = message;
    js["time"] = getCurrentTime();
    string buffer = js.dump();

    int len = send(clientfd, buffer.c_str(), buffer.size(), 0);
    if (-1 == len)
    {
        cerr << "send chat msg error ->" << buffer << endl;
    }
}

// "addfriend" command handler
void addfriend(int clientfd, string str)
{
    int friednid = atoi(str.c_str());

    json js;
    js["msgid"] = ADD_FRIEND_MSG;
    js["id"] = g_currentUser.getID();
    js["friendid"] = friednid;
    string buffer = js.dump();

    int len = send(clientfd, buffer.c_str(), buffer.size(), 0);
    if (-1 == len)
    {
        cerr << "send addfriend msg error ->" << buffer << endl;
    }
}

// "creategroup" command handler
void creategroup(int clientfd, string str)
{
    int idx = str.find(":");
    if (-1 == idx)
    {
        cerr << "creategroup command invalid!" << endl;
        return;
    }

    // groupname::groupdesc
    string groupname = str.substr(0, idx);
    string groupdesc = str.substr(idx + 1, str.size() - idx);

    json js;
    js["msgid"] = CREATE_GROUP_MSG;
    js["id"] = g_currentUser.getID();
    js["groupname"] = groupname;
    js["groupdesc"] = groupdesc;
    string buffer = js.dump();

    int len = send(clientfd, buffer.c_str(), buffer.size(), 0);
    if (-1 == len)
    {
        cerr << "send creategroup msg error ->" << buffer << endl;
    }
}

// "addgroup" command handler
void addgroup(int clientfd, string str)
{
    int groupid = atoi(str.c_str());

    json js;
    js["msgid"] = ADD_GROUP_MSG;
    js["id"] = g_currentUser.getID();
    js["groupid"] = groupid;
    string buffer = js.dump();

    int len = send(clientfd, buffer.c_str(), buffer.size(), 0);
    if (-1 == len)
    {
        cerr << "send addgroup msg error ->" << buffer << endl;
    }
}

// "groupchat" command handler
void groupchat(int clientfd, string str)
{
    // groupchat::groupid::message
    int idx = str.find(":");
    if (-1 == idx)
    {
        cerr << "groupchat command invalid!" << endl;
        return;
    }

    // groupname::groupdesc
    int groupid = atoi(str.substr(0, idx).c_str());
    string message = str.substr(idx + 1, str.size() - idx);

    json js;
    js["msgid"] = GROUP_CHAT_MSG;
    js["id"] = g_currentUser.getID();
    js["name"] = g_currentUser.getName();
    js["groupid"] = groupid;
    js["msg"] = message;
    js["time"] = getCurrentTime();
    string buffer = js.dump();

    int len = send(clientfd, buffer.c_str(), buffer.size(), 0);
    if (-1 == len)
    {
        cerr << "send groupchat msg error ->" << buffer << endl;
    }
}

// "loginout" command handler
void loginout(int clientfd, string str)
{
    json js;
    js["msgid"] = LOGINOUT_MSG;
    js["id"] = g_currentUser.getID();
    string buffer = js.dump();

    int len = send(clientfd, buffer.c_str(), buffer.size(), 0);
    if(-1 == len)
    {
        cerr << "send loginout msg error ->" << buffer << endl;
    }
    else
    {
        isMainMenuRunning = false;
    }
}

string getCurrentTime()
{
    auto tt = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    struct tm *ptm = localtime(&tt);
    char data[60] = {0};
    sprintf(data, "%d-%02d-%02d %02d:%02d:%02d",
            (int)ptm->tm_year + 1900, (int)ptm->tm_mon + 1, (int)ptm->tm_mday,
            (int)ptm->tm_hour, (int)ptm->tm_min, (int)ptm->tm_sec);

    return std::string(data);
}