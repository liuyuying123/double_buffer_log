# 双缓冲异步日志
- 基本思路是准备两块buffer A和B。前端负责向buffer A填数据，后端负责将buffer B的数据写入文件、当buffer A写满之后，交换A和B，让后端将buffer A的数据写入文件，而前端则往buffer B填入新的日志消息，如此往复，这样在新建日志消息的时候不必等待磁盘文件操作，也避免每条新日志消息都触发（唤醒）后端日志线程。(相当于每次唤醒写线程的时候，交给它的是以一个buffer为单位的日志)。
- 使用双缓冲技术实现的异步日志系统，基于静态局部变量的单例模式。日志前端调用Log::Instrance().append(string& message)方法添加日志。主程序首先调用init()方法初始化日志系统，在init()函数当中会自动创建后端写日志。
- 实际使用4个buffer，参照陈硕大佬的《Linux多线程服务端编程》，（真是获益匪浅，这样只会在前端写满当前buffer或者后端等待条件变量超时时，才会唤醒写端线程，大大减少了前端去cond_.notify_one()的次数，且后端获取到buffer过后自己写，这时已经释放了锁，不会阻塞前端线程）。
- 此处的buffer是一个简易实现，每个buffer能存储固定数量的string日志消息，底层容器使用queue。
- 实现了日志按天分文件和按超过最大行数分文件（简陋版），新文件的命名还有点问题（因为懒，反正可以用就行了）。
- 交换前端和后端的buffer使用的是移动操作，大大提高了效率。
- main.cpp简易测试了一下log的功能。