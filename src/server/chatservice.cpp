#include "public.hpp"
#include "chatservice.hpp"
#include "usermodel.hpp"
#include <iostream>
#include <vector>
#include <muduo/base/Logging.h>

using namespace muduo;
using namespace std;

ChatService *ChatService::instance()
{
    static ChatService service;

    return &service;
}

// 注册消息以及对应的Handler回调操作
ChatService::ChatService()
{
    _msgHandlerMap.insert({LOGIN_MSG, std::bind(&ChatService::login, this, _1, _2, _3)});
    _msgHandlerMap.insert({REG_MSG, std::bind(&ChatService::reg, this, _1, _2, _3)});
    _msgHandlerMap.insert({ONE_CHAT_MSG, std::bind(&ChatService::oneChat, this, _1, _2, _3)});
    _msgHandlerMap.insert({ADD_FRIEND_MSG, std::bind(&ChatService::addFriend, this, _1, _2, _3)});
    _msgHandlerMap.insert({CREATE_GROUP_MSG, std::bind(&ChatService::createGroup, this, _1, _2, _3)});
    _msgHandlerMap.insert({ADD_GROUP_MSG, std::bind(&ChatService::addGroup, this, _1, _2, _3)});
    _msgHandlerMap.insert({GROUP_CHAT_MSG, std::bind(&ChatService::groupChat, this, _1, _2, _3)});
    _msgHandlerMap.insert({LOGINOUT_MSG, std::bind(&ChatService::loginout, this, _1, _2, _3)});

    // 连接redis服务器
    if (_redis.connect())
    {
        // 设置上报信息的回调
        _redis.init_notify_handler(std::bind(&ChatService::handleRedisSubscribeMessage, this, _1, _2));
    }
}

MsgHandler ChatService::getHandler(int msgid)
{
    // 记录错误日志，msgid没有对应的事件处理回调
    auto it = _msgHandlerMap.find(msgid);
    if (it == _msgHandlerMap.end())
    {
        // 返回一个空操作
        return [=](const TcpConnectionPtr &conn, json &js, Timestamp)
        {
            LOG_ERROR << "msgid:" << msgid << " can not find handler!";
        };
    }
    else
    {
        return _msgHandlerMap[msgid];
    }
}

// 处理客户端异常退出
void ChatService::clientCloseException(const TcpConnectionPtr &conn)
{
    User user;
    // 加锁，避免此时有用户修改Map表导致冲突
    {
        lock_guard<mutex> lock(_connMutex);
        for (auto it = _userConnMap.begin(); it != _userConnMap.end(); it++)
        {
            if (it->second == conn)
            {
                // 从map表删除用户的连接信息
                user.setID(it->first);
                _userConnMap.erase(it);
                break;
            }
        }
    }

    // 用户下线，在redis中取消订阅channel(id)
    _redis.unsubscribe(user.getID());

    // 更新用户的状态信息
    if (user.getID() != -1)
    {
        user.setState("offline");
        _userModel.updateState(user);
    }
}

// 处理注销业务
void ChatService::loginout(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int userid = js["id"].get<int>();

    {
        lock_guard<mutex> lock(_connMutex);
        auto it = _userConnMap.find(userid);
        if (it != _userConnMap.end())
        {
            // 从map表删除用户的连接信息
            _userConnMap.erase(it);
        }
    }

    // 用户下线，在redis中取消订阅channel(id)
    _redis.unsubscribe(userid);

    // 更新用户的状态信息
    User user(userid, "", "", "offline");
    _userModel.updateState(user);
}

// 服务器异常，业务重置
void ChatService::reset()
{
    // 将所有online状态的用户，设置为offline
    _userModel.resetState();
}

// 处理登陆业务 ORM 业务层操作的都是对象，否则迁移时困难
void ChatService::login(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    // LOG_INFO << "do login service!!!";
    int id = js["id"].get<int>();
    string pwd = js["password"];

    User user = _userModel.query(id);
    if (user.getID() == id && user.getPwd() == pwd)
    {
        if (user.getState() == "online")
        {
            // 已经登陆，不允许重复登陆
            json response;
            response["msgid"] = LOGIN_MSG_ACK;
            response["errno"] = 2;
            response["errmsg"] = "账户已登陆";
            conn->send(response.dump());
        }
        else
        {
            // 加锁，多线程，考虑线程安全
            {
                // 登录成功，记录用户连接信息，用于服务器给用户推送信息
                lock_guard<mutex> lock(_connMutex);
                _userConnMap.insert({id, conn});
            }

            // id用户登陆成功后，向redis定于channel(id)
            _redis.subscribe(id);

            // 登陆成功，更新用户状态信息 state offline => online
            user.setState("online");
            _userModel.updateState(user);

            json response;
            response["msgid"] = LOGIN_MSG_ACK;
            response["errno"] = 0;
            response["id"] = user.getID();
            response["name"] = user.getName();

            // 查询该用户是否有离线消息
            vector<string> vec;
            vec = _offlineMsgModel.query(id);
            if (!vec.empty())
            {
                response["offlinemsg"] = vec;
                // 读取离线消息之后将其删除
                _offlineMsgModel.remove(id);
            }

            // 查询用户的好友信息并返回
            vector<User> userVec = _friendModel.query(id);
            if (!userVec.empty())
            {
                vector<string> vec2;
                for (User &user : userVec)
                {
                    json js;
                    js["id"] = user.getID();
                    js["name"] = user.getName();
                    js["state"] = user.getState();
                    vec2.push_back(js.dump());
                }
                response["friends"] = vec2;
            }

            // 查询用户的群组信息并返回
            vector<Group> groupuserVec = _groupModel.queryGroups(id);
            if (!groupuserVec.empty())
            {
                vector<string> groupV;
                for (Group &group : groupuserVec)
                {
                    json grpjs;
                    grpjs["id"] = group.getID();
                    grpjs["groupname"] = group.getName();
                    grpjs["groupdesc"] = group.getDesc();
                    // 获取群组成员信息
                    vector<GroupUser> userVec = group.getUsers();
                    // 将群组成员信息存储在json中
                    vector<string> userV;
                    for (GroupUser &user : userVec)
                    {
                        json js;
                        js["id"] = user.getID();
                        js["name"] = user.getName();
                        js["state"] = user.getState();
                        js["role"] = user.getRole();
                        userV.push_back(js.dump());
                    }
                    grpjs["users"] = userV;
                    // 现在只是获取了一个群组
                    groupV.push_back(grpjs.dump());
                }
                response["groups"] = groupV;
            }

            conn->send(response.dump());
        }
    }
    else
    {
        // 登陆失败
        json response;
        response["msgid"] = LOGIN_MSG_ACK;
        response["errno"] = 1;
        response["errmsg"] = "用户名或密码错误";
        conn->send(response.dump());
    }
}

// 处理注册业务
void ChatService::reg(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    // LOG_INFO << "do reg service!!!";
    string name = js["name"];
    string pwd = js["password"];

    User user;
    user.setName(name);
    user.setPwd(pwd);
    bool state = _userModel.insert(user);
    if (state)
    {
        // 注册成功
        json response;
        response["msgid"] = REG_MSG_ACK;
        response["errno"] = 0;
        response["id"] = user.getID();
        conn->send(response.dump());
    }
    else
    {
        // 注册失败
        json response;
        response["msgid"] = REG_MSG_ACK;
        response["errno"] = 1;
        conn->send(response.dump());
    }
}

void ChatService::oneChat(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int toid = js["toid"].get<int>();

    // 加锁，防止发送过程中conn从MAP表中被删除
    {
        lock_guard<mutex> lock(_connMutex);
        auto it = _userConnMap.find(toid);
        if (it != _userConnMap.end())
        {
            // toid在线，转发消息，服务器主动推送消息给toid用户
            it->second->send(js.dump());
            return;
        }
    }

    // 如果在当前服务器查找不到toid，则在mysql中查询toid用户是否在线
    User user = _userModel.query(toid);
    if (user.getState() == "online")
    {
        _redis.publish(toid, js.dump());
        return;
    }

    // toid不在线，存储离线消息
    _offlineMsgModel.insert(toid, js.dump());
}

// 添加好友业务 msgid id friendid
void ChatService::addFriend(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int id = js["id"].get<int>();
    int friendid = js["friendid"].get<int>();

    _friendModel.insert(id, friendid);
}

// 创建群组业务
void ChatService::createGroup(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int userid = js["id"].get<int>();
    string groupname = js["groupname"];
    string groupdesc = js["groupdesc"];

    // 存储新创建的群组信息
    Group group(-1, groupname, groupdesc);
    if (_groupModel.createGroup(group))
    {
        // 存储群聊创建人信息
        _groupModel.addGroup(userid, group.getID(), "creator");
    }
}

// 加入群组业务
void ChatService::addGroup(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int userid = js["id"].get<int>();
    int groupid = js["groupid"].get<int>();

    // 存储群聊成员信息
    _groupModel.addGroup(userid, groupid, "normal");
}

// 群组聊天业务
void ChatService::groupChat(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int userid = js["id"].get<int>();
    int groupid = js["groupid"].get<int>();

    // 获取群组成员列表
    vector<int> userVec = _groupModel.queryGroupUsers(userid, groupid);

    lock_guard<mutex> lock(_connMutex);
    for (int id : userVec)
    {
        auto it = _userConnMap.find(id);
        if (it != _userConnMap.end())
        {
            // 在线用户，推送消息
            it->second->send(js.dump());
        }
        else
        {
            // 查询id用户是否在其他服务器在线
            User user = _userModel.query(id);
            if (user.getState() == "online")
            {
                _redis.publish(id, js.dump());
            }
            else
            {
                // 离线用户，存储离线消息
                _offlineMsgModel.insert(id, js.dump());
            }
        }
    }
}

// 从redis消息队列中获取订阅的消息 {通道号，消息}
void ChatService::handleRedisSubscribeMessage(int userid, string msg)
{
    lock_guard<mutex> lock(_connMutex);
    auto it = _userConnMap.find(userid);
    if (it != _userConnMap.end())
    {
        it->second->send(msg);
        return;
    }

    // 存储该用户的离线消息
    _offlineMsgModel.insert(userid, msg);
}