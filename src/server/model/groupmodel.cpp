#include "groupmodel.hpp"
#include "db.h"
#include <iostream>
using namespace std;

bool GroupModel::createGroup(Group &group)
{
    // 1.组装sql语句
    char sql[1024] = {0};
    sprintf(sql, "insert into allgroup(groupname, groupdesc) values('%s', '%s')",
            group.getName().c_str(), group.getDesc().c_str());

    MySQL mysql;
    if (mysql.connect())
    {
        if (mysql.update(sql))
        {
            // 获取插入成功的用户数据生成的主键id
            group.setID(mysql_insert_id(mysql.getConnection()));
            return true;
        }
    }

    return false;
}

void GroupModel::addGroup(int userid, int groupid, string role)
{
    // 1.组装sql语句
    char sql[1024] = {0};
    sprintf(sql, "insert into groupuser values(%d, %d, '%s')", groupid, userid, role.c_str());

    MySQL mysql;
    if (mysql.connect())
    {
        mysql.update(sql);
    }
}

// 查询用户所在群组信息
vector<Group> GroupModel::queryGroups(int userid)
{
    /*
    1. 先查询用户所在的群组id
    2. 再根据用户群组id利用联合查询得到群组的所有信息
    */
    char sql[1024] = {0};
    sprintf(sql, "select a.id, a.groupname, a.groupdesc from allgroup a inner join groupuser b on a.id = b.groupid where b.userid = %d", userid);
    
    vector<Group> gropuVec;
    MySQL mysql;
    if(mysql.connect())
    {
        MYSQL_RES *res = mysql.query(sql);
        if(res != nullptr)
        {
            MYSQL_ROW row;
            while((row = mysql_fetch_row(res)) != nullptr)
            {
                Group group;
                group.setID(atoi(row[0]));
                group.setName(row[1]);
                group.setDesc(row[2]);
                gropuVec.push_back(group);
            }
            mysql_free_result(res);
        }
    }

    // 还需要查询群组成员信息
    for(Group &group : gropuVec)
    {
        // 获取群组成员信息，对user和groupuser表进行联合查询
        sprintf(sql, "select a.id, a.name, a.state, b.grouprole from user a inner join groupuser b on a.id = b.userid where b.groupid = %d", group.getID());
        MYSQL_RES *res = mysql.query(sql);
        if(res != nullptr)
        {
            MYSQL_ROW row;
            while((row = mysql_fetch_row(res)) != nullptr)
            {
                GroupUser user;
                user.setID(atoi(row[0]));
                user.setName(row[1]);
                user.setState(row[2]);
                user.setRole(row[3]);
                group.getUsers().push_back(user);
            }
            mysql_free_result(res);
        }
    }

    return gropuVec;
}

vector<int> GroupModel::queryGroupUsers(int userid, int groupid)
{
    char sql[1024] = {0};
    // 查找groupuser表中除了userid以外的同组全部成员id
    sprintf(sql, "select userid from groupuser where groupid = %d and userid != %d", groupid, userid);

    vector<int> idVec;
    MySQL mysql;
    if(mysql.connect())
    {
        MYSQL_RES *res = mysql.query(sql);
        if(res != nullptr)
        {
            MYSQL_ROW row;
            while((row = mysql_fetch_row(res)) != nullptr)
            {
                idVec.push_back(atoi(row[0]));
            }
            mysql_free_result(res);
        }
    }

    return idVec;
}