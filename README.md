# Computer-Networking
The Assignment of Computer-Networking (NKU)

# 简单的聊天协议设计
目前完成的功能是在服务端打开的时候，点击客户端exe执行，即可连接到服务端进行多人通讯（但是不会显示谁上线谁下线，保护一下下），还能私聊私发消息 \
服务端负责Log信息以及每个用户的消息转发（没完成消息队列，直接转发的，不想debug辽...）


# 可靠的UDP传输 
part 1: 设计了HEADER结构体和可靠UDP类进行封装，完成的内容是rdt3.0，涉及超时重传、错误检验、三次握手和四次挥手等功能，还没进行丢包测试。
