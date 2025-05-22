/*
TcpServer : 用于编写服务器程序
TcpClient ：用于编写客户端程序

好处：将网络I/O代码与业务代码区分开
                    用户的连接与断开、用户的可读写事件
*/

#include <muduo/net/TcpServer.h>
#include <muduo/net/EventLoop.h>
#include <iostream>
#include <functional>
#include <string>
using namespace std;
using namespace muduo;
using namespace muduo::net;
using namespace placeholders;

/*
基于muduo网络库开发服务器程序
1.组合TcpServer对象
2.创建EventLoop事件循环对象的指针
3.明确TcpServer构造函数需要什么参数，实现ChatServer构造参数
4.在当前服务器类的构造函数当中，注册处理连接的回调函数和处理读写事件的回调函数
5.设置合适的服务端线程数量，muduo库会自动分配I/O线程和worker线程
*/
class ChatServer
{
private:
    // 用于处理用户的连接创建和断开
    void onConnection(const TcpConnectionPtr& conn) {
        if(conn->connected()) {
            cout << conn->peerAddress().toIpPort() << " -> " << 
                conn->localAddress().toIpPort() << " state:online" << endl;
        }
        else {
            cout << conn->peerAddress().toIpPort() << " -> " << 
                conn->localAddress().toIpPort() << " state:offline" << endl;
            // 断开连接
            conn->shutdown();
            // _loop->quit();
        }
    }
    // 用于处理用户的读写事件
    void onMessage (const TcpConnectionPtr& conn,  // 连接
                            Buffer* buffer,    // 缓冲区
                            Timestamp time) // 接收到数据的时间信息
    {
        string buf = buffer->retrieveAllAsString();
        cout << "recv data:" << buf << "time:" << time.toString() << endl;
        // 回显
        conn->send(buf);
    }

    TcpServer _server;
    EventLoop *_loop;

public:
    ChatServer(EventLoop* loop,     // 事件循环
            const InetAddress& listenAddr,  // IP + Prot
            const string& nameArg)  // 服务器名称
            : _server(loop, listenAddr, nameArg), _loop(loop)
    {
        // 普通函数调用：在什么地方发生、要实现什么功能，两者都明确
        // 回调：在什么地方发生、要实现什么不能，两者是分开的
        // 给服务器注册用户的创建和断开回调，知道要做新用户的连接创建或已连接用户的断开，但是不知道应该什么时候执行，需要网络对端上报
        _server.setConnectionCallback(std::bind(&ChatServer::onConnection, this, _1));

        // 给服务器注册用户读写事件回调
        _server.setMessageCallback(std::bind(&ChatServer::onMessage, this, _1, _2, _3));

        // 设置服务器端的线程数量 1个I/O线程 3个worker线程
        _server.setThreadNum(4);
    }

    // 开启事件循环
    void start() {
        _server.start();
    }

    ~ChatServer() {}
};


int main()
{
    EventLoop loop;
    InetAddress addr("127.0.0.1", 6000);
    ChatServer server(&loop, addr, "ChatServer");

    server.start();
    loop.loop();    // epoll_wait以阻塞方式等待新用户连接，已连接用户的读写事件等

    return 0;
}