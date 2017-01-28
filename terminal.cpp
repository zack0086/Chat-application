#include "terminal.h"
#include "common.h"
#include <vector>
#include <iostream>
#include <stdio.h>
#include <string>
#include <sstream>
#include <cstring>
#include <string.h>

using namespace std;

extern char* str2raw(string& s);
extern int debug;

Terminal::Terminal() {
    status = 0;
}

void Terminal::correctUsage() {
    cout << "Correct usage sample: " << "block remove/add [client name1] [client name 2] ..." << endl
         << "              " << "broadcast msg ['message content']" << endl
         << "              " << "broadcast file [file name]" << endl
         << "              " << "unicast msg ['message content']/[message content(with no space)] [client name 1] [client name 2] ..." << endl
         << "              " << "unicast file [file name] [client name 1] [client name 2]" << endl
         << "              " << "blockcast msg ['message content']/[message content(with no space)] [client name 1] [client name 2] ..." << endl
         << "              " << "blockcast file [file name] [client name 1] [client name 2] ..." << endl;
}

int Terminal::run() {
    cout << "Welcome to use our chat app, first you need to set the terminal" << endl 
         << "to server or client, only one server is allowed at a time" << endl
         << "you can also set debug mode on or off" << endl
         << "Sample usage: " << "set server" << endl 
         << "              " << "set client [IP address] [port#] [client name]" << endl
         << "              " << "set debug true/false" << endl
         << "The application can implement following functions:" << endl
         << "block: add or remove clients into/from blocklists" << endl
         << "broadcast: send message or file to all clients apart from those in the blocklists" << endl
         << "unicast: send message or file to a specific client" << endl
         << "blockcast: send message or file to all clients apart from those you specified" << endl
         << "Sample usage: " << "block remove/add [client name1] [client name 2] ..." << endl
         << "              " << "broadcast msg ['message content']/[message content(with no space)]" << endl
         << "              " << "broadcast file [file name]" << endl
         << "              " << "unicast msg ['message content']/[message content(with no space)] [client name 1] [client name 2] ..." << endl
         << "              " << "unicast file [file name] [client name 1] [client name 2]" << endl
         << "              " << "blockcast msg ['message content']/[message content(with no space)] [client name 1] [client name 2] ..." << endl
         << "              " << "blockcast file [file name] [client name 1] [client name 2] ..." << endl;
    while(true) {
        string line;
        getline(cin, line, '\n');
        if(line.empty() || line.at(0) == ' '){
            correctUsage();
            continue;
        }
        int res = parse(line);

        if (res == -1) break;
    }
    return 0;
}

int Terminal::parse(string line) {
    stringstream ss(line);
    string tmp; vector<string> v;
    while(ss >> tmp) {
        string::iterator bg = tmp.begin();
        string::reverse_iterator ed = tmp.rbegin() + 1;
        if (*bg == '\'') {
            tmp.erase(tmp.begin());
            if (*ed == '\''){
                tmp.erase(tmp.end()-1);
            }
            else {
                string msgtmp;
                while(ss >> msgtmp){
                    tmp += " ";
                    string::reverse_iterator msged = msgtmp.rbegin();
                    if(*msged == '\''){
                        msgtmp.erase(msgtmp.end() - 1);
                        tmp += msgtmp;
                        break;
                    }
                    tmp += msgtmp;
                }
            }
        }
        if (*bg == '\"') {
            tmp.erase(tmp.begin());
            if (*ed == '\"'){
                tmp.erase(tmp.end()-1);
            }
            else {
                string msgtmp;
                while(ss >> msgtmp){
                    tmp += " ";
                    string::reverse_iterator msged = msgtmp.rbegin();
                    if(*msged == '\"'){
                        msgtmp.erase(msgtmp.end() - 1);
                        tmp += msgtmp;
                        break;
                    }
                    tmp += msgtmp;
                }
            }
        }
        v.push_back(tmp);
    }
    
    if (v[0] == "set") {
        if (v.size() < 2) 
            cout << "Correct usage: " << "set server" << endl 
                 << "               " << "set client [IP address] [port#] [client name1] [client name 2] ... "<< endl
                 << "               " << "set debug true/false" << endl;
        else if ((v[1] == "client") && (v.size() == 5)) {
            if (status == 1){
                cout << "you have already set it to server" << endl;
                return 0;
            } else if (status == 2){
                cout << "you have already set it to client" << endl;
                return 0;
            }
            if(stoi(v[3])>65535){
                cout<< "port # too large" << endl;
                return 0;
            }
            if(client.create_and_connect(v[2].c_str(), v[2].size(), stoi(v[3])) == -1){
                cout << "create connection failed, please input the correct IP address or Port #" << endl;
                return 0;
            }

            client.send_reg(v[4].c_str(), v[4].size());
            status = 2;
        } else if (v[1] == "server") {
            if (status == 1){
                cout << "you have already set it to server" << endl;
                return 0;
            } else if (status == 2){
                cout << "you have already set it to client" << endl;
                return 0;
            }
            server.start();
            status = 1;
        } else if (v[1] == "debug") {
            if (v.size() < 3) {
                cout << "Correct usage: set debug true/false" << endl;
                return 0;
            } 
            if (v[2] == "true") debug = true;
            else if (v[2] == "false") debug = false;
            else {
                cout << "Correct usage: set debug true/false" << endl;
                return 0;
            }
        } else {
            cout << "Correct usage: " << "set server" << endl 
                 << "               " << "set client [IP address] [port#] [client name1] [client name 2] ... "<< endl
                 << "               " << "set debug true/false" << endl;
        }
    } else if(status == 1) {
        if(v[0] == "stop") {
            server.stop();
            return -1;
        } else {
            cout << "You can't do anything except stop" << endl;
        }
    }   else if(status == 2) {
        if(v[0] == "stop")
            return -1;
        parseClient(v);
    } else{
        cout << "Correct usage: " << "set server" << endl 
                 << "               " << "set client [IP address] [port#] [client name1] [client name 2] ... "<< endl
                 << "               " << "set debug true/false" << endl;
    }
    return 0;
}

void Terminal::parseClient(vector<string>& v) {
    if (v[0] == "block") {
        if (v.size() < 3)
            correctUsage();
        else if (v[1] ==  "add") {
            for (int i=2; i<v.size(); ++i) {
                mblock.insert(v[i]);
                cout << "successfully blocked " << v[i] << endl;
            } 
        } else if (v[1] == "remove") {
            for (int i=2; i<v.size(); ++i) {
                if (mblock.count(v[i]) > 0) 
                    mblock.erase(v[i]);
                cout << "successfully removed " << v[i]
                    << " from blocklists" << endl;
            }
        } else {
            correctUsage();
        }
    } else if (v[0] == "broadcast") {
        if (v.size() < 3) {
            correctUsage();
            return;
        }
        if (v[1] == "file" && v.size() > 3) {
            cout << "Only support 1 file 1 time" << endl;
            correctUsage();
            return;
        }
        if (v[1] == "file" || (v[1] == "msg" && v.size()==3)) {
            if (broadcast_hdl(v) < 0){
                cout << "Failed on broadcast" << endl;
                correctUsage();
            }
        }
        else {
            correctUsage();
        }
    } else if (v[0] == "unicast") {
        if (v.size()<4 || (v[1] != "msg" && v[1] != "file")) {
            correctUsage();
            return;
        }
        if (unicast_hdl(v) < 0) {
            cout << "Failed on unicast" << endl;
            correctUsage();
        }
    } else if (v[0] == "blockcast"){
        if (v.size()<4 || (v[1] !="msg" && v[1]!="file")) {
            correctUsage();
            return;
        }
        if (blockcast_hdl(v) < 0) {
            cout << "Failed on blockcast" << endl;
            correctUsage();
        }
    } else
        correctUsage();
}

void Terminal::mblock2raw(vector<char*>& res) {
    for (set<string>::iterator itor = mblock.begin(); itor != mblock.end(); ++itor){
        int len = itor->size();
        char* tmp = (char*)malloc(len+1);
        copy(itor->c_str(), itor->c_str()+len, tmp);
        tmp[len] = 0;

        res.push_back(tmp);
    }
}

void Terminal::mblock_bc2raw(vector<char*>& res) {
    for (set<string>::iterator itor = mblock_bc.begin(); itor != mblock.end(); ++itor){
        int len = itor->size();
        char* tmp = (char*)malloc(len+1);
        copy(itor->c_str(), itor->c_str()+len, tmp);
        tmp[len] = 0;

        res.push_back(tmp);
    }
}

int Terminal::unicast_hdl(vector<string>& v) {
    // write msg context / file name 
    char* msg = str2raw(v[2]);

    // write namelists
    vector<char*> nlist;
    for(int i=3; i<v.size(); ++i)
        nlist.push_back(str2raw(v[i])); 
    
    if (v[1] == "file")
        return client.send_file(msg, strlen(msg), false, nlist);  
    else if (v[1] == "msg") 
        return client.send_msg(msg, strlen(msg), false, nlist); 

    return -1;
}

int Terminal::blockcast_hdl(vector<string>& v) {
    // write msg context / file name
    char* msg = str2raw(v[2]);
    
    // write namelists
    vector<char*> nlist;
    for (int i=3; i<v.size(); ++i) 
        nlist.push_back(str2raw(v[i]));

    if (v[1] == "file") 
        return client.send_file(msg, strlen(msg), true, nlist);
    else if (v[1] == "msg")
        return client.send_msg(msg, strlen(msg), true, nlist);

    return -1;
}

int Terminal::broadcast_hdl(vector<string>& v) {
    // get blocked users
    vector<char*> res;
    for(string s: mblock)
        res.push_back(str2raw(s));

    char* ctx = str2raw(v[2]);

    if (v[1] == "file") 
        return client.send_file(ctx, strlen(ctx), true, res);
    else if (v[1] == "msg") 
        return client.send_msg(ctx, strlen(ctx), true, res);

    return -1;
}
