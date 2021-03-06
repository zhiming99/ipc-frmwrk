### Introduction

* The `ridl`, stands for `rpc idl`, as the RPC interface description language. And this project tries to deliver a rapid development tool for `rpc-frmwrk` by generating a set of skelton source files for proxy/server , as well as the configuration files, and Makefile with the input `ridl` files. It is expected to generates the skelton project for C++, Python and Java.   
* This tool helps the developer to ease the learning curve and focus efforts on business implementations. And one can still refer to the `../test` directory to manually craft an `rpc-frmwrk` applications, with the maximum flexibility and performance advantage.
* The following is a sample file `example.ridl`. 

```
// must have statement
appname "example";
typedef array< array< string > > STRMATRIX2;
const timeout_sec = 120;

struct FILE_INFO
{
    /* define the fileds here, with default value optionally*/
    string szFileName = "test.dat";
    uint64 fileSize = 0;
    bool bRead = true;
    bytearray fileHeader;
    STRMATRIX2 vecLines;
    map<int32, bytearray> vecBlocks;
};

// echo different type of information
interface IEchoThings
{
    // synchronous call on both server/proxy side by default.
    Echo( string strText )
        returns ( string strResp ) ;

    // server/proxy both sides asynchronous
    [ async ]EchoMany ( int32 i1, int16 i2,
        int64 i3, float i4, double i5, string szText )
        returns ( int32 i1r, int16 i2r,
            int64 i3r, float i4r,
            double i5r, string szTextr );

    // server side asynchronous, and proxy side synchronous
    [ async_s ]EchoStruct( FILE_INFO fi ) returns ( FILE_INFO fir );

    // An event Handler
    [ event ]OnHelloWorld( string strMsg ) returns ();
};

service SimpFileSvc [
    timeout=timeout_sec, rtpath="/",
    ipaddr="192.168.0.1",
    compress ]
{
    interface IEchoThings;
};
```

### Supported Data Types

`ridl` support 10 basic types and 3 composite types.
The basic data types are:

* **byte**
* **bool**
* **int16/uint16**
* **int32/uint32**
* **int64/uint64**
* **float(32)/double(64)**
* **string**
* **bytearray**( binary blob )
* **ObjPtr** : `rpc-frmwrk` built-in serializable data type.
* **HANDLE** : a handle representing an existing stream channel, which can be transferred between the proxy/server.

The 3 composite types are

* **array** : an array of data of basic type or composite type except `HSTREAM`
* **map** : a map consisting of key-value paires. `key` should be a comparable data type, and value could be any supported type except `HSTREAM`.
* **struct**: as `FILE_INFO` shows in the above example, is a package of informations of different data types. It is used as a build block for other data types or as the parameter to transfer between proxy and server.

### Statements

The above example shows most of the statements ridl supports. ridl now has 7 types of statements.

* **appname** : specify the application name, it is a must have statement, if there are multiple `appname` statements, the last one will be the winner.
* **include** : specify another ridl file, whose content are referred in this file. for example, `include "abc.ridl";` 
* **typedef** : define an alias for a pre-defined data type.  For example,`typedef myint int32;`
* **const**: to assign a name to a constant value. For example, `const i = 2;`
* **struct** : to define a struct.
* **interface** : an interface is a set of methods logically bounded closely to deliver some kind of service to the client. It is made up of interface id and a set of methods, as similiar to a class in an OO language.
  * **interface id**: a string to uniquely identify the interface, it is used by `rpc-frmwrk` to locate the service.
  * **method** : a method is the smallest unit of interaction between server and proxy. The proxy send the request, and the server executes at the proxy's request and returns the response. As you can see, an `ridl method` is made up of `method name`, `input parameter list`, `output parameter list`. Beside the three major components a method must have, it can have some attributes, too, within the bracket ahead of the `method name` as shown in the above example. The attributes supported by a `method` include
    * **async, async_s, async_p** : to instruct the ridl compiler to generate asynchronous or synchronous code for server or proxy. Let me explain a bit more. 
      * *Asynchronous* means the method call can return immediately with the operation not completed. specifically in the context of `rpc-frmwrk`, the method may return a status code STATUS_PENDING, to inform the caller the operation is still in-process, besides success or error.
      *  *Synchronous* is in the opposite, that when a method returns, it returns sucess, or an error code, and never returns STATUS_PENDING. And the caller is sure the operation either is done or ends up with error.
    * **timeout** : an integer to specify a method specific timeout value in second the proxy can wait before becoming impatient.
    * **noreply** : specify a method not to wait for server response. The proxy side, the method will return as soon as the request is sent and on the server side, the request is handled without sending out the response.
    * **event** : to specify a method as an event handler, as not applied to normal method, picks up a event broadcasted by the server and request the proxy to do somethings on the event.
  * **the input parameters** or **output parameters** of a method can be empty, but the method still get an status code from the server to tell if the method is handled successfully or not, unless the method is labeled with the `event` attribute.
* **service declaration** : to declare a `service` object. A `service` object contains a set of interfaces, to deliver a relatively independent feature or service. it contains a `service id` and a set of interfaces.
  * **service id**: will appear in the proxy request's `ObjPath` string, as part of an object address as to find the service.
  * Besides interfaces, the service can also be assigned some attributes, which will go into the server/proxy configuration files.
    * **timeout** : similiar to the one on a method, but it serves as the default timeout value for all the methods from all the interfaces.
    * **rtpath** : a path string to identify the target host behind a router to a cloud of hosts. You can find more information [Here](https://github.com/zhiming99/rpc-frmwrk/wiki/Introduction-of-Multihop-support#objetct-access-via-multihop-routing) about `router path`.
    * **ipaddr** : a string to specify the router ip address the proxy to connect to. It is not necessarily the address of the host of the target service.
    * **portnum** : a integer to specify the router port number the proxy to connect to.
    * **websock** : the existance of this attribute instructs to connect to the remote router via [`Websocket`](https://github.com/zhiming99/rpc-frmwrk/tree/master/rpc/wsport#technical-information)
    * **SSL** : the existance of this attribute instructs to connect to the remote router via [SSL](https://github.com/zhiming99/rpc-frmwrk/blob/master/rpc/sslport/Readme.md) connection.
    * **compress** : the existance of this attribute instruct to compress the message bound to the remote router.
    * **auth** : the so-called `AuthInfo` string, to specify the authentication information to access the remote router. If this attribute does not exist, the authentication related traffic won't happen. The above four attributes can combine freely. Currently `Kerberos` is the only supported auth mechanism, and therefore the json string contain the `Kerberos`'s authentication information. Please follow [link](https://github.com/zhiming99/rpc-frmwrk/tree/master/rpc/security#4-configure-rpc-frmwrk-with-authentication) for more information about `AuthInfo`.
    * **stream** : a flag to enable streaming support on this `service` object.

### Invoking `ridlc`

`ridlc` stands for `ridl compiler`. Its command line looks like `ridlc [options] <ridl file>`, and there are the following major options:

```
        -I:     To specify the path to search for the included files.
                And this option can repeat manytimes
        -O:     To specify the path for
                the output files. 'output' is the 
                default path if not specified
        -o:     To specify the file name as
                the base of the target image. That is,
                the <name>cli for client and <name>svr
                for server. If not specified, the
                'appname' from the ridl will be used
        -s:     To apply new serilization
                protocol only when the old serilization
                method cannot handle. Default is new
                serialization protocol always
```

Currently ridlc can only output `c++` project. In the future, it will be able to generate `python` project and `java` project as well.

### Output

On a successful compile of the above sample ridl file, `ridlc` will generate several files:

* **maincli.cpp, mainsvr.cpp**: as the name indicate, the two files define main function of the proxy and server respectively. Each file contains a same-name function, that is a `maincli` function in maincli.cpp and `mainsvr` in mainsvr.cpp, which is the ideal place to add your custom code.

* **example.cpp, example.h** : the files are named with the name defined by *appname*. It contains all the skelton method declarations, and implementations.
* **SimpFileSvc.cpp, SimpFileSvc.h:** the files are named with the *service id* of the *service statement*. Each individual service declaration has a pair of `.cpp` and `.h` file. The cpp file contains all the methods that user should implement to get server/proxy to work. They are mainly the same-name methods on Server side as request handler. The amount of efforts are different depending on whether to use `sync` or `async` approach to implement the interfaces of the service. `sync` implementation is simple and time-saving at development time, `async` is relatively difficult to develop, but with better performance. And `rpc-frmwrk` provides rich utilities to help developing asynchronous implementation to minize the developing efforts.
* **Makefile**: The make file to build the project. Note that, you must install the `rpc-frmwrk` first.
* **exampledesc.json, driver.json:** The configuration files.
