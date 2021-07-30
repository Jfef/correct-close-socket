https://blog.netherlabs.nl/articles/2009/01/18/the-ultimate-so_linger-page-or-why-is-my-tcp-not-reliable

2021年7月30日10:58:15 TYPE: 翻译

[TOC]

### part One

这篇文章是关于 TCP 网络编程的一个不起眼的地饭，这个地方几乎每个人都不太清楚发生了什么。我曾经以为我理解它，但是上周我发现我并没有理解。

所以我决定在网上搜索并咨询专家，答应他们一劳永逸地写下他们的智慧，希望终止这个话题。

专家（H. Willstrand、Evgeniy Polyakov、Bill Fink、Ilpo Jarvinen 和 Herbert Xu）做出了回应，这是我的文章。

尽管我经常提到 Linux TCP 实现，但所描述的问题并非特定于 Linux，并且可能发生在任何操作系统上。

问题是什么？
有时，我们必须将未知数量的数据从一个位置发送到另一个位置。 TCP，可靠的传输控制协议，听起来正是我们所需要的。来自 Linux tcp(7) 联机帮助页：

> TCP 在 ip(7) 之上的两个套接字之间提供了可靠的、面向流的、全双工连接，适用于 v4 和 v6 版本。 TCP 保证数据按顺序到达并重新传输丢失的数据包。它生成并检查每个数据包的校验和以捕获传输错误。”

然而，当我们天真地使用 TCP 只发送我们需要传输的数据时，它通常无法做我们想要的 - 最后传输的千字节或有时兆字节的数据永远不会到达。

假设我们在两个兼容 POSIX 的操作系统上运行以下两个程序，目的是从程序 A 向程序 B 发送 100 万字节（程序可以在这里找到）：

A: 

```c
sock = socket(AF_INET, SOCK_STREAM, 0);  
connect(sock, &remote, sizeof(remote));
write(sock, buffer, 1000000);             // returns 1000000
close(sock);
```

B:

```c
int sock = socket(AF_INET, SOCK_STREAM, 0);
bind(sock, &local, sizeof(local));
listen(sock, 128);
int client=accept(sock, &local, locallen);
write(client, "220 Welcome\r\n", 13);

int bytesRead=0, res;
for(;;) {
   res = read(client, buffer, 4096);
   if(res < 0)  {
      perror("read");
      exit(1);
   }
   if(!res)
      break;
   bytesRead += res;
}
printf("%d\n", bytesRead);
```

Quiz question - what will program B print on completion?

> A) 1000000
>
> B) something less than 1000000
>
> C) it will exit reporting an error
>
> D) could be any of the above

遗憾的是，正确答案是“D”。 但这怎么会发生呢？ 程序 A 报告所有数据都已正确发送！

到底是怎么回事？

通过 TCP 套接字发送数据确实不提供与写入普通文件相同的“命中磁盘”语义（如果您记得调用 fsync()）。

> 实际上，在TCP 中所有成功的 write() 意味着==内核已接受您的数据，并且将尝试在自己的最佳时间传输它==。 即使当内核认为携带数据的数据包已经发送，实际上，它们也只是传递给网络适配器，它甚至可能在感觉时发送数据包。
>
> 从那时起，==数据将通过网络遍历许多这样的适配器和队列，直到它到达远程主机==。 那里的内核会在收到数据时确认，如果拥有套接字的进程实际上正在关注并尝试从中读取数据，那么数据最终将到达应用程序，在文件系统中，“命中磁盘”

TCP 一端的所谓`write()`发送成功，仅仅是表示成功交付给了网络适配器。并不是我们理解的成功交付给对面对的socket peer。仅仅电脑内核完成了所谓的发送。

==Note that the acknowledgment sent out only means the kernel saw the data - it does not mean the application did!==

### Part Two

<u>OK, I get all that, but why didn’t all data arrive in the example above?</u>

.好的，我明白了，但是为什么上面的示例中没有所有数据到达？ 当我们在 TCP 上发出 close() 时，这确实发生了：即使您的某些数据仍在等待发送，或者已发送但未确认：内核可以关闭整个连接。

> 理解为：即是存在等待发送的数据，或者，数据已经发送了，但是对面并没有ack保证数据发送到达。内核仍然关闭了socket 。
>
> socket 从电脑内核的角度的来说，所有数据交付了网络适配器了，可以关闭连接了；

怎么来的？

事实证明，在这种情况下，RFC 1122 的第 4.2.2.13 节告诉我们，带有任何未决可读数据的 close() 可能会导致立即发送重置。

> “主机可以实现‘半双工’TCP 关闭序列，因此调用 CLOSE 的应用程序无法继续从连接中读取数据。如果这样的主机发生close()调用后，任然存在没有处理的接收数据，或者如果在调用 CLOSE 后收到新数据，它的 TCP 应该发送一个 RST 来表明数据丢失了。”

在我们的例子中，我们有这样的数据待处理：我们在程序 B 中传输的`“220 Welcome\r\n”`，但从未在程序 A 中读取！

如果程序 B 未发送该行，则很可能我们的所有数据都已正确到达。

那么，如果我们先读取这些数据，然后再读取 LINGER，我们就可以开始了吗？

 close() 调用确实没有传达我们试图告诉内核的内容：==请在发送我通过 write() 提交的所有数据后关闭连接==。

幸运的是，系统调用 shutdown() 可用，它可以准确地告诉内核这一点。 然而，仅靠它是不够的。 当 shutdown() 返回时，我们仍然没有迹象表明程序 B 收到了所有内容。

然而，我们可以做的是发出一个shutdown()，这将导致一个FIN数据包被发送到程序B。程序B反过来会关闭它的套接字，我们可以从程序A中检测到这一点：随后的read()将 返回 0。

![image-20210729210728401](https://i.loli.net/2021/07/29/WRNUBDnFjrS72vp.png)

Program A now becomes:

```c
  sock = socket(AF_INET, SOCK_STREAM, 0);  
    connect(sock, &remote, sizeof(remote));
    write(sock, buffer, 1000000);             // returns 1000000
    shutdown(sock, SHUT_WR);
    for(;;) {
        res=read(sock, buffer, 4000);
        if(res < 0) {
            perror("reading");
            exit(1);
        }
        if(!res)
            break;
    }
    close(sock);
```

那么这是完美吗？ 好吧.. 如果我们看一下 HTTP 协议，发送的数据通常包含长度信息，无论是在 HTTP 响应的开头，还是在传输信息的过程中（所谓的“分块”模式）。

他们这样做是有原因的。 只有这样，接收端才能确定它收到了它发送的所有信息。

使用上面的 shutdown() 技术实际上只是告诉我们远程关闭了连接。 它实际上并不能保证程序 B 正确接收到所有数据。最好的建议是发送长度信息，并让远程程序主动确认所有数据都已收到。 当然，这只有在您有能力选择自己的协议时才有效。

还能做什么？
如果您需要将流数据传送到墙上的一个愚蠢的 TCP/IP 漏洞’，正如我不得不多次这样做的那样，可能无法遵循上面关于发送长度信息和获得确认的明智建议。

在这种情况下，接受套接字接收端关闭作为一切到达的指示可能还不够好。

幸运的是，事实证明 Linux 会跟踪未确认数据的数量，可以使用 SIOCOUTQ ioctl() 查询这些数据。 一旦我们看到这个数字达到 0，我们就可以合理地确定我们的数据至少到达了远程操作系统。

-----

技术有限，不太懂下面的

Unlike the shutdown() technique described above, SIOCOUTQ appears to be Linux-specific. Updates for other operating systems are welcome.

The [sample code](http://ds9a.nl/tcp-programs.tar.gz) contains an example of how to use SIOCOUTQ.

<u>But how come it ‘just worked’ lots of times!</u>

As long as you have no unread pending data, the star and moon are aligned correctly, your operating system is of a certain version, you may remain blissfully unimpacted by the story above, and things will quite often ‘just work’. But don’t count on it.

<u>Some notes on non-blocking sockets</u>

Volumes of communications have been devoted the the intricacies of SO_LINGER versus non-blocking (O_NONBLOCK) sockets. From what I can tell, the final word is: don’t do it. Rely on the shutdown()-followed-by-read()-eof technique instead. Using the appropriate calls to poll/epoll/select(), of course.

<u>A few words on the Linux sendfile() and splice() system calls</u>

It should also be noted that the Linux system calls sendfile() and splice() hit a spot in between - these usually manage to deliver the contents of the file to be sent, even if you immediately call close() after they return.

This has to do with the fact that splice() (on which sendfile() is based) can only safely return after all packets have hit the TCP stack since it is zero copy, and can’t very well change its behaviour if you modify a file *after* the call returns!

Please note that the functions do not wait until all the data has been acknowledged, it only waits until it has been sent.

------

### 关于SO_LINGER

此问题已导致在邮件列表、Usenet 和论坛上发布大量帖子，并且这些帖子都很快在 SO_LINGER 套接字选项上归零，该选项似乎只是考虑到了这个问题：“启用后，关闭 (2 ) 或 shutdown(2) 将不会返回，直到套接字的所有排队消息都已成功发送或已达到延迟超时。 否则，调用会立即返回并在后台完成关闭。 当套接字作为 exit(2) 的一部分关闭时，它总是在后台徘徊。” 所以，我们设置了这个选项，重新运行我们的程序。 它仍然不起作用，并不是我们所有的百万字节都到达了。

当socket 设置了linger 后，作为exit()的一部分，无法解决这个问题。
