#ifndef DOUGLE_BUFFER_LOG
#define DOUBLE_BUFFER_LOG

#include <string>
#include <queue>
#include <mutex>
#include <memory>
#include <condition_variable>
#include <thread>
class buffer{
public:
    buffer():bufferSize_(1024){}
    void append(std::string& message);

    int avail(); //buffer还可以存多少条日志消息

    std::string get();//从buffer获取一条日志消息
    bool isempty();
private:
    int bufferSize_;
    std::queue<std::string> messages_;  //使用队列，先进先出
    
};


class dbLog{
public:
    static dbLog& Instance();
    void append(std::string logline);//前端写日志
    void append(const char* logline);

    void init(std::string logpath, std::string logname, int level,bool isrunning=true,int maxline=5000); //初始化日志系统
    void threadWrite(); //后端写日志,线程调用的函数

    void do_write(const std::string& message);
    ~dbLog();
private:
    dbLog(){};    //构造函数私有化
    dbLog& operator=(const dbLog&); //拷贝赋值函数私有化
    dbLog(const dbLog&);    //拷贝构造函数私有化

    int fd_;//日志文件的文件描述符
    std::string logdir_;
    std::string logname_;
    std::string logfilepath_;
    int level_;
    bool running_;

    int logfd_;
    int today_; //日志运行在哪天
    int max_line_;//日志文件的最大行数
    int curr_line_;
    int daylogcount_;

    std::unique_ptr<buffer> currbuffer_; //当前buffer
    std::unique_ptr<buffer> nextbuffer_; //预备buffer
    std::vector<std::unique_ptr<buffer>> buffers_; //待写入文件的已填满的缓冲
    std::mutex mtx_;    //互斥锁
    std::condition_variable cond_;  //条件变量
    std::thread write_thread_;
};


#endif