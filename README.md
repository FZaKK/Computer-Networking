# Computer-Networking
The Assignment of Computer-Networking (NKU)

# 简单的聊天协议设计
1. 目前完成的功能是在服务端打开的时候，点击客户端exe执行，即可连接到服务端进行多人通讯（但是不会显示谁上线谁下线，保护一下下），还能私聊私发消息 
2. 服务端负责Log信息以及每个用户的消息转发（没完成消息队列，直接转发的，不想debug辽...）


# 可靠的UDP传输 
part 1: 
1. 设计了HEADER结构体和可靠UDP类进行封装，完成的内容是rdt3.0，涉及超时重传、错误检验、三次握手和四次挥手等功能。 
2. 已经完成了错误error和丢包drop的测试，没有使用路由程序（可能是因为Windows 11的兼容性问题），在程序中手动添加了错误概率和丢包概率进行的测试，如果不想进行测试，将带有“仅供测试”注释的代码段删除即可

part 2:
1. part 2展示的是基于GBN算法的数据分组发送，在接收端返回的ACK序列号为最后一次确认分组的序列号，发送过程之中使用queue队列来当前的窗口之中未确认的数据分组。
2. GBN累计确认只需要设置一个计时器即可，每次发送端接收ACK之后重设计时器。也可以通过设置类似于GBN的计时器策略来实现简略的SR选择重传算法，但是完整的选择重传还是需要对每一个分组都设置一个计时器的，如果想要实现多计时器或多线程的SR算法，可见Skyyyyy github。
