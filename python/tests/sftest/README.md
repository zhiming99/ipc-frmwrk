### This is an advanced test case to demonstrate the streaming mechanism.
It is a complete different implementation from the C++ version of `sftest`, with the effort to   
reduce the complexity of the streaming transfer than that of the CPP [`sftest`](https://github.com/zhiming99/rpc-frmwrk/tree/master/test/sftest).   

Unlike the normal RPC request, streaming mechanism requires a set of system defined API to work.   
The workflow is as follows,   
  * To establish one or more stream channels from the proxy side with `StartStream`
  * To `Read/WriteStream` to communicate between the proxy and the server. The system provides   
  three approaches to read/write stream, as synchronous read/write, asynchronous read/write. And   
  `NoWait read` to peek the incoming queue, `NoWait write` to submit a write request, without   
  waiting for the confirmation.
  * To close the stream channel, both proxy and server can launch a CloseStream request, or stop   
the proxy or server directly.
