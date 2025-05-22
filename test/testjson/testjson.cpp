#include "json.hpp"
using json = nlohmann::json;

#include <iostream>
#include <vector>
#include <map>
#include <string>
using namespace std;

// json序列化示例1
void func1() {
    json js;
    // 无序键值对
    js["msg_type"] = 2;
    js["from"] = "zhang san";
    js["to"] = "li si";
    js["msg"] = "hello, what are you doing now?";

    // 序列化：json数据对象 => json字符串
    string sendBuf = js.dump();
    // 转换成字符串之后就可以通过网络发送
    cout << sendBuf.c_str() << endl;
}

// json反序列化示例01
string func01() {
    json js;
    // 无序键值对
    js["msg_type"] = 2;
    js["from"] = "zhang san";
    js["to"] = "li si";
    js["msg"] = "hello, what are you doing now?";

    string sendBuf = js.dump();
    return sendBuf;
}

// json序列化示例2
void func2() {
    json js;
    // 添加数组
    js["id"] = {1,2,3,4,5};
    // 添加key-value
    js["name"] = "zhang san";
    // 添加对象
    js["msg"]["zhang san"] = "hello world";
    js["msg"]["liu shuo"] = "hello china";
    // 上面等同于下面这句一次性添加数组对象
    js["msg"] = {{"zhang san", "hello world"}, {"liu shuo", "hello china"}};
    cout << js << endl;
}

// json反序列化示例02
string func02() {
    json js;
    // 添加数组
    js["id"] = {1,2,3,4,5};
    // 添加key-value
    js["name"] = "zhang san";
    // 添加对象，相当于msg也是一个json对象，里面有zhang san 和 liu shuo
    js["msg"]["zhang san"] = "hello world";
    js["msg"]["liu shuo"] = "hello china";
    // 上面等同于下面这句一次性添加数组对象
    js["msg"] = {{"zhang san", "hello world"}, {"liu shuo", "hello china"}};
    return js.dump();
}

// json序列化示例3
void func3() {
    json js;

    // 直接序列化一个vector容器
    vector<int> vec;
    vec.push_back(1);
    vec.push_back(2);
    vec.push_back(5);
    js["list"] = vec;

    // 直接序列化一个map容器
    map<int, string> m;
    m.insert({1, "黄山"});
    m.insert({2, "华山"});
    m.insert({3, "泰山"});
    js["path"] = m;

    string sendBuf = js.dump();
    cout<< sendBuf.c_str() <<endl;
}

// json反序列化示例03
string func03() {
    json js;

    // 直接序列化一个vector容器
    vector<int> vec;
    vec.push_back(1);
    vec.push_back(2);
    vec.push_back(5);
    js["list"] = vec;

    // 直接序列化一个map容器
    map<int, string> m;
    m.insert({1, "黄山"});
    m.insert({2, "华山"});
    m.insert({3, "泰山"});
    js["path"] = m;

    string sendBuf = js.dump();
    return sendBuf;
}


int main() {

    // func1();
    // func2();
    // func3();

    string recvBuf = func01();
    // 反序列化：json字符串 => json数据对象（可以保留数据类型）
    json jsbuf = json::parse(recvBuf);
    cout << jsbuf["msg_type"] << endl;
    cout << jsbuf["from"] << endl;
    cout << jsbuf["to"] << endl;
    cout << jsbuf["msg"] << endl;

    // string recvBuf = func02();
    // // 反序列化：json字符串 => json数据对象（可以保留数据类型）
    // json jsbuf = json::parse(recvBuf);
    // auto arr = jsbuf["id"];
    // cout << arr[2] << endl;
    // cout << jsbuf["name"] << endl;
    // auto msgjs = jsbuf["msg"];
    // cout << msgjs["zhang san"] << endl;
    // cout << msgjs["liu shuo"] << endl;

    // string recvBuf = func03();
    // // 反序列化：json字符串 => json数据对象（可以保留数据类型）
    // json jsbuf = json::parse(recvBuf);
    // // 可以直接使用容器接收
    // vector<int>vec = jsbuf["list"];
    // for(int& v : vec)
    //     cout << v << " ";
    // cout << endl;

    // map<int, string> mymap = jsbuf["path"];
    // for(auto& p : mymap)
    //     cout << p.first << " " << p.second << endl;

    return 0;
}
