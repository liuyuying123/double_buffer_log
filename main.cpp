#include "./log.h"
#include <thread>
#include <unistd.h>
#include <iostream>
using namespace std;
void thread_function(){
    dbLog& logger=dbLog::Instance();
    int i=0;
    while(1){
        std::string message("this is message ");
        message+=std::to_string(i++);
        logger.append(message);
        sleep(1);
        cout<<"sleep finish."<<endl;
    }
}

int main(){

    dbLog& logger=dbLog::Instance();
    logger.init("/log/","log",2);
    std::thread thd1(thread_function);
    std::thread thd2(thread_function);
    std::thread thd3(thread_function);

    //等待线程退出
    thd1.detach();
    thd2.detach();
    thd3.detach();

    while(1){
        cout<<"主程序正在执行"<<endl;
        sleep(1);
    }    

    return 0;
}