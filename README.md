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
2. GBN累计确认只需要设置一个计时器即可，每次发送端接收ACK之后重设计时器。也可以通过设置类似于GBN的一个计时器策略来实现简略的SR选择重传算法，但是完整的选择重传还是需要对每一个分组都设置一个计时器的（可用set timer的方式），如果想要实现多计时器或多线程的SR算法，可见Skyyyyy github。

part 3:
1. part 3是拥塞控制算法的实现，在接收端的源代码之中，为了模拟延迟加入了router线程，不过也可以不用这个线程，使用原来较为完整的GBN接收文件的函数即可，因为设置丢包概率，丢包也可以视为拥塞导致的丢包。
2. 对于part 3的设计，其实从我浅薄的认知上感觉，（又经过了些研究）发现Reno是TCP上的拥塞控制，TCP上兼具着响应重复ACK和缓冲区这两大特征，因此会出现适配问题。SR算法更适合适配Reno或者New Reno算法的实现，因为在设计3个重复ACK（也可以是3个失序的分组）之后需要重传所有的数据分组，GBN算法会把所有的失序分组都进行丢弃。所以如果只重传base的数据分组，那么base+1的数据分组就只能靠超时来进行处理了，这与Reno算法的提前重传思想好像不太符合。（可能说的不太对...）
3. 在这里实现的是基于New Reno算法的拥塞控制算法，为了适配GBN算法，重发所有失序数据分组（这显然可能会导致进一步拥塞），在快速恢复阶段进行了一定的改变与设计，在接收到重复的ACK后不进行递增操作，而是在成功确认一个数据分组后进行递增操作（启发式认为：成功确认->网络情况有所改善）（因为New Reno不会轻易重新进入拥塞避免阶段，需要确认所有的分组）
4. 发送端仅添加了send_file_Reno函数，主要是FSM的转换过程，如果想封装的话可以封装成3个线程（接收，发送，检验超时）（往里面传参设置结构体还挺麻烦...）
   有时间了，改改
5. Part 3的代码封装的不太好，可能看起来有点麻烦（有时间改改），欢迎提出issue。

（后续把其他课程部分也传上来...）
