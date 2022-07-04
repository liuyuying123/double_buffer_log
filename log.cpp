#include "./log.h"
#include <ctime>
#include <chrono>
#include <fstream>
#include <sys/types.h>    
#include <sys/stat.h>    
#include <fcntl.h>
#include <assert.h>
#include <unistd.h>
#include <thread>
#include <iostream>
using namespace std;


void buffer::append(string& message){
    if(messages_.size()>=bufferSize_){
        return;
    }

    messages_.push(message);
}
bool buffer::isempty(){
    return messages_.empty();
}


int buffer::avail(){
    return bufferSize_-messages_.size();
}

string buffer::get(){
    
    string res=messages_.front();
    messages_.pop();
    return res;
}



dbLog& dbLog::Instance(){
    //静态局部变量！
    static dbLog instance_;
    return instance_;
}


void dbLog::append(string logline){
    lock_guard<mutex> locker(mtx_);
    if(currbuffer_->avail()>1){
        //表明buffer未满，现在可以写进去
        currbuffer_->append(logline);
    }
    else{
        //当前缓冲区已经满了，currbuffer_的指针放入buffers_中，然后将nextbuffer移动到currbuffer_
        buffers_.emplace_back(currbuffer_.release());//此时的currbuffer_已经为空了
        if(nextbuffer_){
            //如果nextbuffer_未写
            //unique_ptr在移动过后，移动赋值或移动构造函数中会将该unique_ptr置为nullptr
            currbuffer_=std::move(nextbuffer_);//将nextbuffer_的所有权交给currbuffer,此时nextbuffer_为nullptr
        }
        else{
            //说明前端写入太快，两块缓冲区都用完了，则再创建一个缓冲区
            currbuffer_.reset(new buffer);

        }

        currbuffer_->append(logline);
        cond_.notify_one();//唤醒后端
    }

}

void dbLog::threadWrite(){
    cout<<"写线程正在执行"<<endl;
    unique_ptr<buffer> newbuffer1(new buffer);
    unique_ptr<buffer> newbuffer2(new buffer);

    vector<unique_ptr<buffer>> buffer_to_write;

    while(running_){
        {   
            unique_lock<mutex> locker(mtx_);
            if(buffers_.empty()){
                cond_.wait_for(locker,chrono::duration<int>(3));
            }
            //不管是前端写满了，还是过了3秒钟，都要把currbuffer_中的内容写出来
            buffers_.emplace_back(currbuffer_.release());
            currbuffer_=std::move(newbuffer1);
            buffer_to_write.swap(buffers_);//交换
            if(!nextbuffer_){
                //如果此时nextbuffer_也被使用了，再给它一块空缓冲
                nextbuffer_=move(newbuffer2);
            }
        }

        //这时可以将buffer_to_write中的内容写入日志文件，而不会产生竞争
        //然后将写完的buffer交给newbuffer1和newbuffer2，用来做缓冲
        for(auto &p:buffer_to_write){
            //p是一个unique_ptr<buffer>
            while(!p->isempty()){
                //p指向的buffer为非空
                do_write(p->get());
            }
            if(!newbuffer1){
                newbuffer1.reset(p.release());
                continue;
            }

            if(!newbuffer2){
                newbuffer2.reset(p.release());
                continue;
            }
            //其他的buffer自动释放
        }

        buffer_to_write.clear();

    }

}

void dbLog::do_write(const string& message){
    curr_line_++;
    string temp_message=message+string("\r\n");
    time_t now=time(nullptr);
    tm* t=localtime(&now);
    if(today_!=t->tm_mday){
        //过了一天，则重新创建文件，更换文件描述符
        logfilepath_=logdir_+to_string(t->tm_mon)+"-";
        logfilepath_+=to_string(t->tm_mday);
        logfilepath_+="-"+logname_+".log";

        close(logfd_);
        logfd_=open(logfilepath_.c_str(),O_CREAT|O_RDWR,0777);
        assert(logfd_);
        today_=t->tm_mday;
        daylogcount_=0;
    }

    else if(curr_line_ && curr_line_%max_line_==0){
        //这时候必须要分文件了
        close(logfd_);
        //这里分文件的文件名怎么设计再想一下
        logfilepath_+=to_string(daylogcount_++);
        logfd_=open(logfilepath_.c_str(),O_CREAT|O_RDWR,0777);
        assert(logfd_);
    }

    //将日志写入文件中
    string timestr=to_string(t->tm_year)+"/"+to_string(t->tm_mon)+"/"+to_string(t->tm_mday)+":";
    timestr+=to_string(t->tm_hour)+":"+to_string(t->tm_min)+":"+to_string(t->tm_sec)+":  ";
    temp_message=timestr+temp_message;
    int ret=write(logfd_,temp_message.c_str(),temp_message.size());
    assert(ret!=-1);
}

dbLog::~dbLog(){
    if(logfd_) close(logfd_);
}

void dbLog::init(string logdir, string logname, int level, bool isrunning,int maxline){
    logdir_=logdir;
    logname_=logname;
    level_=level;
    running_=isrunning;
    //生成日志文件的真正名字
    time_t now=time(nullptr);
    tm* t=localtime(&now);
    max_line_=maxline;
    curr_line_=0;
    string timestr=to_string(t->tm_mon)+"-";
    timestr+=to_string(t->tm_mday);
    char path[1024];
    string tempdir(getcwd(path,1024));
    cout<<"tempdir: "<<tempdir<<endl;
    if(access(logdir_.c_str(),F_OK)!=0){
        //如果目录不存在，创建目录
        mkdir(logdir_.c_str(),0777);
    }
    today_=t->tm_mday;
    daylogcount_=0;//当天分了多少个log文件
    logfilepath_=logdir_+timestr+"-"+logname+".log";
    logfilepath_=tempdir+logfilepath_;
    cout<<"logfilepath_: "<<logfilepath_<<endl;
    umask(0);
    logfd_=open(logfilepath_.c_str(),O_CREAT|O_RDWR,0777);
    if(logfd_<0) perror("open");
    
    //初始化两块缓冲区噻
    currbuffer_.reset(new buffer);
    nextbuffer_.reset(new buffer);

    //在init的时候开始创建写线程
    write_thread_=std::thread([](){
        dbLog::Instance().threadWrite();}
    );
    write_thread_.detach();
    // cout<<"init执行完毕"<<endl;

}